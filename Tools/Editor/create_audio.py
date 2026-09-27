"""Synthesise every game sound as a MetaSound, headless, with nothing downloaded.

    /Game/Audio/Classes/SCL_Master, SCL_SFX, SCL_Ambient, SCL_UI   sound classes (SFX, Ambient and UI
                                                                   under Master, UI under SFX)
    /Game/Audio/Classes/SMX_Settings                               the mix the settings sliders drive
    /Game/Audio/Classes/ATT_World                                  natural falloff, 300 to 3000 cm
    /Game/Audio/Classes/ATT_Lamp                                   natural falloff, 50 to 400 cm
    /Game/Audio/SFX/MS_*                                           one-shots (bow, grapple, movement,
                                                                   melee, thugs, trick arrows, UI)
    /Game/Audio/Ambient/MS_Amb_*                                   loops (wind, street hum, lamp buzz)

Route: the MetaSound Builder API from Python. UMetaSoundBuilderSubsystem.CreateSourceBuilder makes a
source graph (OnPlay, OnFinished, a mono out), AddNodeByClassName adds the engine's standard nodes
(UE.Sine, UE.Saw, UE.Noise, UE.State Variable Filter, UE.One-Pole Low/High Pass Filter, AD Envelope,
UE.Multiply/Add/Subtract, UE.RandomFloat, UE.LFO, UE.Lfo Frequency Noise, UE.Trigger Delay,
UE.TriggerRepeat, UE.ConversionFloatToTime), ConnectNodes wires them, SetNodeInputDefault sets the
literals, and UMetaSoundEditorSubsystem.BuildToAsset saves the source. Every sound has a
PitchVariation input: a random factor in [1 - v, 1 + v] drawn on play scales its frequencies.
Controlled inputs the game sets: Draw (bow creak), Speed (zip hum), Intensity (landing).

Idempotent: each MetaSound carries a HawkeyeAudioHash metadata tag (its recipe's bytecode plus the
asset settings). An unchanged recipe is left alone; a changed one is deleted and built again at the
same path (a saved MetaSound cannot be overwritten by a builder headless).

Git: Content/Audio/ is the Game Animation Sample's (ignored); .gitignore re-includes Audio/SFX,
Audio/Ambient and Audio/Classes, which are ours.
"""

import hashlib
import inspect
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

AUDIO_ROOT = "/Game/Audio"
CLASSES_PATH = AUDIO_ROOT + "/Classes"
SFX_PATH = AUDIO_ROOT + "/SFX"
AMBIENT_PATH = AUDIO_ROOT + "/Ambient"
HASH_TAG = "HawkeyeAudioHash"
AUDIO_BUILD = "audio-3"     # bump to rebuild every sound (a change to the Graph helpers below)
AUTHOR = "Hawkeye create_audio.py"

# class name -> parent class name (None for the root)
SOUND_CLASSES = [
    ("SCL_Master", None),
    ("SCL_SFX", "SCL_Master"),
    ("SCL_Ambient", "SCL_Master"),
    ("SCL_UI", "SCL_SFX"),
]
MIX_NAME = "SMX_Settings"
# name -> (inner radius, falloff distance), cm
ATTENUATIONS = {
    "ATT_World": (300.0, 2700.0),
    "ATT_Lamp": (50.0, 350.0),
}

WHITE = 1
PINK = 0


# --------------------------------------------------------------------------------------
# builder plumbing
# --------------------------------------------------------------------------------------


def _items(ret):
    """A Python binding returns the value, or a tuple of the value and its out params."""
    if isinstance(ret, tuple):
        return list(ret)
    return [ret]


def _pick(ret, kind):
    for item in _items(ret):
        if isinstance(item, kind):
            return item
    return None


def _ok(ret, what):
    result = _pick(ret, unreal.MetaSoundBuilderResult)
    if result is not None and result != unreal.MetaSoundBuilderResult.SUCCEEDED:
        raise RuntimeError("{0} failed".format(what))
    return ret


class Sig(object):
    """An output pin and what flows out of it: 'a' audio, 'f' float, 't' trigger."""

    def __init__(self, handle, kind):
        self.handle = handle
        self.kind = kind


class Graph(object):
    """One MetaSound source being built. Helpers return Sig; plain numbers are float literals."""

    def __init__(self, name, one_shot):
        self.name = name
        self.sub = unreal.get_engine_subsystem(unreal.MetaSoundBuilderSubsystem)
        ret = _ok(self.sub.create_source_builder(
            "HawkeyeBuild_" + name, unreal.MetaSoundOutputAudioFormat.MONO, one_shot), name + " builder")
        self.builder = _pick(ret, unreal.MetaSoundSourceBuilder)
        if self.builder is None:
            raise RuntimeError("no source builder for " + name)
        self.play = Sig(_pick(ret, unreal.MetaSoundBuilderNodeOutputHandle), "t")
        self.finished = _pick(ret, unreal.MetaSoundBuilderNodeInputHandle) if one_shot else None
        outs = list(_pick(ret, unreal.Array) or [])
        if not outs:
            raise RuntimeError("no audio out on " + name)
        self.audio_out = outs[0]
        self.nodes = 0
        self._pitch = None

    # --- literals and wiring -----------------------------------------------------------

    def literal(self, value):
        if isinstance(value, bool):
            return _pick(self.sub.create_bool_meta_sound_literal(value), unreal.MetasoundFrontendLiteral)
        if isinstance(value, Int):
            return _pick(self.sub.create_int_meta_sound_literal(value.value), unreal.MetasoundFrontendLiteral)
        return _pick(self.sub.create_float_meta_sound_literal(float(value)), unreal.MetasoundFrontendLiteral)

    def node(self, namespace, name, variant="", major=1):
        class_name = unreal.MetasoundFrontendClassName(namespace=namespace, name=name, variant=variant)
        ret = _ok(self.builder.add_node_by_class_name(class_name, major), "add {0}.{1}.{2}".format(namespace, name, variant))
        self.nodes += 1
        return _pick(ret, unreal.MetaSoundNodeHandle)

    def pin_in(self, node, pin):
        ret = _ok(self.builder.find_node_input_by_name(node, pin), "input " + pin)
        return _pick(ret, unreal.MetaSoundBuilderNodeInputHandle)

    def pin_out(self, node, pin, kind):
        ret = _ok(self.builder.find_node_output_by_name(node, pin), "output " + pin)
        return Sig(_pick(ret, unreal.MetaSoundBuilderNodeOutputHandle), kind)

    def feed(self, node, pin, value):
        handle = self.pin_in(node, pin)
        if isinstance(value, Sig):
            _ok(self.builder.connect_nodes(value.handle, handle), "connect " + pin)
        else:
            _ok(self.builder.set_node_input_default(handle, self.literal(value)), "default " + pin)

    def make(self, spec, inputs, output, kind):
        """spec (namespace, name, variant[, major]); inputs {pin: value}; returns Sig of output."""
        node = self.node(*spec)
        for pin, value in inputs.items():
            if value is not None:
                self.feed(node, pin, value)
        return self.pin_out(node, output, kind) if output else node

    def input(self, name, default):
        literal = self.literal(float(default))
        ret = _ok(self.builder.add_graph_input_node(name, "Float", literal, False), "graph input " + name)
        return Sig(_pick(ret, unreal.MetaSoundBuilderNodeOutputHandle), "f")

    def finish(self, audio, done=None):
        _ok(self.builder.connect_nodes(audio.handle, self.audio_out), "audio out")
        if self.finished is not None:
            if done is None:
                raise RuntimeError(self.name + ": a one-shot needs a done trigger")
            _ok(self.builder.connect_nodes(done.handle, self.finished), "on finished")

    # --- maths --------------------------------------------------------------------------

    def mul(self, a, b):
        if not isinstance(a, Sig) and not isinstance(b, Sig):
            return float(a) * float(b)
        if not isinstance(a, Sig) or (a.kind == "f" and isinstance(b, Sig) and b.kind == "a"):
            a, b = b, a
        if a.kind == "a" and isinstance(b, Sig) and b.kind == "a":
            return self.make(("UE", "Multiply", "Audio"), {"PrimaryOperand": a, "AdditionalOperands": b}, "Out", "a")
        if a.kind == "a":
            return self.make(("UE", "Multiply", "Audio by Float"),
                             {"PrimaryOperand": a, "AdditionalOperands": b}, "Out", "a")
        return self.make(("UE", "Multiply", "Float"), {"PrimaryOperand": a, "AdditionalOperands": b}, "Out", "f")

    def add(self, a, b):
        if not isinstance(a, Sig) and not isinstance(b, Sig):
            return float(a) + float(b)
        if not isinstance(a, Sig):
            a, b = b, a
        if a.kind == "a":
            return self.make(("UE", "Add", "Audio"), {"PrimaryOperand": a, "AdditionalOperands": b}, "Out", "a")
        return self.make(("UE", "Add", "Float"), {"PrimaryOperand": a, "AdditionalOperands": b}, "Out", "f")

    def sub_f(self, a, b):
        return self.make(("UE", "Subtract", "Float"), {"PrimaryOperand": a, "AdditionalOperands": b}, "Out", "f")

    def mix(self, *parts):
        """Sum of (audio, gain) pairs."""
        total = None
        for audio, gain in parts:
            scaled = audio if gain == 1.0 else self.mul(audio, gain)
            total = scaled if total is None else self.add(total, scaled)
        return total

    def lerp(self, frm, to, alpha):
        """frm + (to - frm) * alpha for a float Sig alpha."""
        return self.add(self.mul(alpha, to - frm), frm)

    # --- sources and shapers ------------------------------------------------------------

    def pitch(self, variation=0.06):
        """The random pitch factor for this play, from the PitchVariation input."""
        if self._pitch is None:
            v = self.input("PitchVariation", variation)
            self._pitch = self.make(("UE", "RandomFloat", ""), {
                "Next": self.play, "Min": self.sub_f(1.0, v), "Max": self.add(v, 1.0)}, "Value", "f")
        return self._pitch

    def sine(self, freq):
        return self.make(("UE", "Sine", "Audio"), {"Frequency": freq}, "Audio", "a")

    def saw(self, freq):
        return self.make(("UE", "Saw", "Audio"), {"Frequency": freq}, "Audio", "a")

    def noise(self, kind=WHITE, seed=-1):
        return self.make(("UE", "Noise", "Audio"), {"Type": Int(kind), "Seed": Int(seed)}, "Audio", "a")

    def svf(self, audio, cutoff, resonance=0.0, out="Band Pass"):
        return self.make(("UE", "State Variable Filter", "Audio"),
                         {"In": audio, "Cutoff Frequency": cutoff, "Resonance": resonance}, out, "a")

    def lowpass(self, audio, cutoff):
        return self.make(("UE", "One-Pole Low Pass Filter", "Audio"), {"In": audio, "Cutoff Frequency": cutoff},
                         "Out", "a")

    def highpass(self, audio, cutoff):
        return self.make(("UE", "One-Pole High Pass Filter", "Audio"), {"In": audio, "Cutoff Frequency": cutoff},
                         "Out", "a")

    def env(self, attack, decay, trigger=None, audio=True, attack_curve=1.0, decay_curve=1.0):
        """(value, done) of an AD envelope, audio or float rate, fired by trigger (OnPlay by default)."""
        variant = "Audio" if audio else "Float"
        node = self.node("AD Envelope", "AD Envelope", variant)
        self.feed(node, "Trigger", trigger or self.play)
        self.feed(node, "Attack Time", attack)
        self.feed(node, "Decay Time", decay)
        self.feed(node, "Attack Curve", attack_curve)
        self.feed(node, "Decay Curve", decay_curve)
        return self.pin_out(node, "Out Envelope", "a" if audio else "f"), self.pin_out(node, "On Done", "t")

    def lfo(self, freq, lo, hi):
        return self.make(("UE", "LFO", "Audio"), {"Frequency": freq, "Min Value": lo, "Max Value": hi}, "Out", "f")

    def wander(self, rate, lo, hi):
        """Smooth random float, rate new values a second, between lo and hi."""
        return self.make(("UE", "Lfo Frequency Noise", "Audio"),
                         {"Rate": rate, "Min Value": lo, "Max Value": hi, "Rate Jitter": 0.5}, "Out", "f")

    def delay(self, trigger, seconds):
        return self.make(("UE", "Trigger Delay", ""), {"In": trigger, "Delay Time": seconds}, "Out", "t")

    def repeat(self, start, period):
        if isinstance(period, Sig):
            period = self.make(("UE", "ConversionFloatToTime", ""), {"In": period}, "Out", "f")
        return self.make(("UE", "TriggerRepeat", ""), {"Start": start, "Period": period}, "RepeatOut", "t")

    # --- recurring shapes ---------------------------------------------------------------

    def burst(self, cutoff, decay, resonance=1.0, out="Band Pass", kind=WHITE, attack=0.002, curve=0.6,
              trigger=None, seed=-1):
        """A filtered noise hit: (audio, done)."""
        e, done = self.env(attack, decay, trigger, decay_curve=curve)
        return self.mul(self.svf(self.noise(kind, seed), cutoff, resonance, out), e), done

    def tone(self, freq, decay, attack=0.002, curve=0.6, trigger=None, drop=None):
        """A decaying sine at freq (times the pitch factor), optionally falling to drop x freq: (audio, done)."""
        p = self.pitch()
        e, done = self.env(attack, decay, trigger, decay_curve=curve)
        if drop is None:
            hz = self.mul(p, freq)
        else:
            fall, _d = self.env(0.001, decay, trigger, audio=False, decay_curve=curve)
            hz = self.mul(p, self.lerp(freq * drop, freq, fall))
        return self.mul(self.sine(hz), e), done


class Int(object):
    """An int (or enum) literal, as opposed to a float."""

    def __init__(self, value):
        self.value = int(value)


# --------------------------------------------------------------------------------------
# the sounds
# --------------------------------------------------------------------------------------
# Each recipe takes a Graph and finishes it. Keep peaks around 0.5 to 0.8 of full scale.


def bow_draw(g):
    # Loop: the limbs and string creak as Draw (0..1, set by UBowComponent every tick) rises.
    p = g.pitch(0.05)
    d = g.input("Draw", 0.0)
    band = g.svf(g.noise(PINK), g.mul(p, g.add(g.mul(d, 1500.0), 300.0)), 4.0)
    grain = g.mul(band, g.lfo(g.add(g.mul(d, 16.0), 8.0), 0.15, 1.0))
    groan = g.sine(g.mul(p, g.add(g.mul(d, 50.0), 70.0)))
    body = g.mix((grain, 0.7), (groan, 0.12))
    g.finish(g.mul(body, g.mul(d, 0.8)))


def bow_release(g):
    # The twang: 180 and 360 Hz decaying over 0.15 s, a string slap on top.
    low, done = g.tone(180.0, 0.15, curve=0.5)
    high, _d = g.tone(360.0, 0.12, curve=0.4)
    slap, _s = g.burst(2600.0, 0.03, 0.5, out="High Pass Filter")
    g.finish(g.mix((low, 0.5), (high, 0.3), (slap, 0.25)), done)


def arrow_whistle(g):
    # 0.3 s of band-passed air, the pitch falling as it goes.
    p = g.pitch(0.08)
    fall, _f = g.env(0.001, 0.3, audio=False, decay_curve=0.8)
    band = g.svf(g.noise(WHITE), g.mul(p, g.lerp(1400.0, 3600.0, fall)), 8.0)
    e, done = g.env(0.04, 0.26, decay_curve=0.7)
    g.finish(g.mul(g.mul(band, e), 0.45), done)


def impact_wood(g):
    thunk, _t = g.tone(220.0, 0.07, drop=0.7)
    crack, done = g.burst(1800.0, 0.09, 2.0, out="Low Pass Filter")
    g.finish(g.mix((thunk, 0.4), (crack, 0.5)), done)


def impact_stone(g):
    chip, done = g.burst(3500.0, 0.07, 1.0, curve=0.4)
    tick, _t = g.tone(1900.0, 0.03)
    g.finish(g.mix((chip, 0.6), (tick, 0.15)), done)


def impact_flesh(g):
    thud, done = g.tone(90.0, 0.1, drop=0.8)
    smack, _s = g.burst(600.0, 0.08, 1.0, out="Low Pass Filter", kind=PINK)
    g.finish(g.mix((thud, 0.5), (smack, 0.5)), done)


def arrow_pickup(g):
    first, _f = g.tone(2200.0, 0.02)
    second, done = g.tone(3300.0, 0.03, trigger=g.delay(g.play, 0.05))
    g.finish(g.mix((first, 0.25), (second, 0.2)), done)


def grapple_fire(g):
    p = g.pitch(0.06)
    rise, _r = g.env(0.22, 0.18, audio=False)
    band = g.svf(g.noise(PINK), g.mul(p, g.lerp(500.0, 2500.0, rise)), 3.0)
    e, done = g.env(0.05, 0.35, decay_curve=0.7)
    g.finish(g.mul(g.mul(band, e), 0.6), done)


def grapple_zip(g):
    # Loop: the line hums and the air rushes, both up with Speed (0..1).
    p = g.pitch(0.04)
    s = g.input("Speed", 1.0)
    hum = g.lowpass(g.saw(g.mul(p, g.add(g.mul(s, 60.0), 110.0))), 700.0)
    rush = g.svf(g.noise(PINK), g.add(g.mul(s, 1500.0), 1200.0), 2.0)
    g.finish(g.mix((hum, 0.12), (g.mul(rush, g.mul(s, 0.25)), 1.0)))


def grapple_land(g):
    thud, done = g.tone(70.0, 0.22, drop=0.7)
    snow, _s = g.burst(400.0, 0.14, 1.0, out="Low Pass Filter", kind=PINK)
    g.finish(g.mix((thud, 0.6), (snow, 0.5)), done)


def _footstep(g, cutoff, decay, seed):
    p = g.pitch(0.1)
    crunch, done = g.burst(g.mul(p, cutoff), decay, 1.5, attack=0.004, curve=0.6, seed=seed)
    grain = g.mul(crunch, g.wander(180.0, 0.25, 1.0))
    g.finish(g.mul(grain, 0.4), done)


def foot_snow_01(g):
    _footstep(g, 1800.0, 0.09, 11)


def foot_snow_02(g):
    _footstep(g, 2300.0, 0.11, 23)


def foot_snow_03(g):
    _footstep(g, 2800.0, 0.08, 37)


def foot_snow_04(g):
    _footstep(g, 2050.0, 0.10, 53)


def footstep_wood(g):
    # Indoors on boards and stair treads: a hollow knock under a short heel click.
    g.pitch(0.12)
    knock, done = g.tone(150.0, 0.07, drop=0.75)
    body, _b = g.burst(420.0, 0.06, 2.5, out="Band Pass", kind=PINK)
    click, _c = g.burst(3200.0, 0.018, 1.0, attack=0.001, curve=0.4)
    g.finish(g.mix((knock, 0.32), (body, 0.28), (click, 0.1)), done)


def footstep_carpet(g):
    # Carpet: a soft, low, muffled pad with no click.
    g.pitch(0.12)
    pad, done = g.burst(380.0, 0.09, 0.5, out="Low Pass Filter", kind=PINK, attack=0.006, curve=0.8)
    thud, _t = g.tone(85.0, 0.06, attack=0.004)
    g.finish(g.mix((pad, 0.3), (thud, 0.12)), done)


def land(g):
    # Scaled by Intensity (0..1, the fall height; UHawkeye sets it).
    i = g.input("Intensity", 0.5)
    thud, done = g.tone(55.0, 0.22, drop=0.8)
    body, _b = g.burst(500.0, 0.15, 1.0, out="Low Pass Filter", kind=PINK)
    crunch, _c = g.burst(2200.0, 0.12, 1.5)
    mix = g.mix((thud, 0.6), (body, 0.4), (g.mul(crunch, i), 0.35))
    g.finish(g.mul(mix, g.add(g.mul(i, 0.7), 0.3)), done)


def vault_grunt(g):
    blip, done = g.tone(140.0, 0.12, attack=0.01, drop=0.85)
    breath, _b = g.burst(900.0, 0.1, 0.5, out="Low Pass Filter", kind=PINK, attack=0.01)
    g.finish(g.mix((g.lowpass(blip, 600.0), 0.3), (breath, 0.12)), done)


def roll_thump(g):
    thump, _t = g.tone(80.0, 0.18, drop=0.8)
    body, _b = g.burst(300.0, 0.2, 1.0, out="Low Pass Filter", kind=PINK)
    rustle, done = g.burst(1500.0, 0.3, 1.0, attack=0.03)
    g.finish(g.mix((thump, 0.5), (body, 0.4), (rustle, 0.1)), done)


def punch(g):
    thud, done = g.tone(160.0, 0.09, drop=0.65)
    slap, _s = g.burst(1500.0, 0.04, 1.0, out="Low Pass Filter")
    g.finish(g.mix((thud, 0.55), (slap, 0.35)), done)


def heavy_whump(g):
    boom, done = g.tone(120.0, 0.28, drop=0.5)
    body, _b = g.burst(700.0, 0.16, 1.0, out="Low Pass Filter", kind=PINK)
    g.finish(g.mix((boom, 0.7), (body, 0.45)), done)


def block_clank(g):
    # A riot shield: three inharmonic partials ringing down, a scrape on the front.
    ring1, done = g.tone(523.0, 0.35, curve=0.5)
    ring2, _r2 = g.tone(1307.0, 0.25, curve=0.5)
    ring3, _r3 = g.tone(2210.0, 0.18, curve=0.5)
    hit, _h = g.burst(3000.0, 0.02, 0.5, out="High Pass Filter")
    g.finish(g.mix((ring1, 0.3), (ring2, 0.2), (ring3, 0.12), (hit, 0.3)), done)


def parry(g):
    # A bright ring off the bow limb: three high partials over a sharp crack and a small thud, shorter
    # and higher than the shield's clank so the two never read as one.
    ring1, done = g.tone(1568.0, 0.4, curve=0.45)
    ring2, _r2 = g.tone(2637.0, 0.3, curve=0.45)
    ring3, _r3 = g.tone(3951.0, 0.18, curve=0.5)
    crack, _c = g.burst(4500.0, 0.03, 0.5, out="High Pass Filter")
    thud, _t = g.tone(220.0, 0.08, drop=0.6)
    g.finish(g.mix((ring1, 0.28), (ring2, 0.2), (ring3, 0.12), (crack, 0.35), (thud, 0.3)), done)


def stagger_hit(g):
    thud, done = g.tone(180.0, 0.12, drop=0.5)
    smack, _s = g.burst(900.0, 0.06, 1.0)
    g.finish(g.mix((thud, 0.45), (smack, 0.35)), done)


def telegraph(g):
    # A short rising blip: 600 to 1200 Hz over 0.12 s.
    p = g.pitch(0.03)
    rise, _r = g.env(0.12, 0.02, audio=False)
    e, done = g.env(0.01, 0.16, decay_curve=0.8)
    g.finish(g.mul(g.mul(g.sine(g.mul(p, g.lerp(600.0, 1200.0, rise))), e), 0.2), done)


def gunshot(g):
    blast, _b = g.burst(5000.0, 0.12, 0.5, out="Low Pass Filter", curve=0.4)
    thump, done = g.tone(60.0, 0.15, drop=0.6)
    crack, _c = g.burst(3000.0, 0.02, 0.5, out="High Pass Filter")
    g.finish(g.mix((blast, 0.55), (thump, 0.6), (crack, 0.4)), done)


def bat_whoosh(g):
    p = g.pitch(0.08)
    sweep, _s = g.env(0.12, 0.12, audio=False)
    band = g.svf(g.noise(PINK), g.mul(p, g.lerp(400.0, 1800.0, sweep)), 2.0)
    e, done = g.env(0.1, 0.12)
    g.finish(g.mul(g.mul(band, e), 0.55), done)


def hurt_grunt(g):
    p = g.pitch(0.1)
    e, done = g.env(0.01, 0.14, decay_curve=0.7)
    throat = g.svf(g.noise(PINK), g.mul(p, 500.0), 3.0, out="Low Pass Filter")
    voice = g.sine(g.mul(p, 120.0))
    g.finish(g.mul(g.mix((throat, 0.4), (voice, 0.2)), e), done)


def death_groan(g):
    p = g.pitch(0.08)
    fall, _f = g.env(0.001, 0.8, audio=False)
    wobble = g.lfo(5.0, 0.97, 1.03)
    voice = g.sine(g.mul(g.mul(p, wobble), g.lerp(70.0, 110.0, fall)))
    throat = g.svf(g.noise(PINK), 400.0, 2.0, out="Low Pass Filter")
    e, done = g.env(0.05, 0.8, decay_curve=0.8)
    g.finish(g.mul(g.mix((voice, 0.3), (throat, 0.25)), e), done)


def putty_splat(g):
    wet, done = g.burst(900.0, 0.12, 1.0, out="Low Pass Filter", kind=PINK)
    blob, _b = g.tone(180.0, 0.08, drop=0.5)
    g.finish(g.mix((g.mul(wet, g.wander(60.0, 0.3, 1.0)), 0.7), (blob, 0.35)), done)


def bola_whirl(g):
    p = g.pitch(0.06)
    band = g.svf(g.noise(PINK), g.mul(p, 900.0), 3.0)
    whirl = g.mul(band, g.lfo(12.0, 0.0, 1.0))
    e, done = g.env(0.05, 0.6, decay_curve=0.8)
    snap, _s = g.tone(300.0, 0.06, trigger=g.delay(g.play, 0.55))
    g.finish(g.mix((g.mul(whirl, e), 0.5), (snap, 0.3)), done)


def smoke_hiss(g):
    # Loop for the cloud's life; the cloud actor owns the component.
    g.pitch(0.05)
    hiss = g.highpass(g.noise(WHITE), 3000.0)
    g.finish(g.mul(g.mul(hiss, g.wander(2.0, 0.6, 1.0)), 0.18))


def emp_pulse(g):
    p = g.pitch(0.04)
    fall, _f = g.env(0.001, 0.6, audio=False, decay_curve=0.6)
    sweep = g.sine(g.mul(p, g.lerp(120.0, 1600.0, fall)))
    e, done = g.env(0.005, 0.7, decay_curve=0.7)
    crackle = g.mul(g.highpass(g.noise(WHITE), 2000.0), g.wander(300.0, 0.0, 1.0))
    g.finish(g.mul(g.mix((sweep, 0.35), (crackle, 0.25)), e), done)


def explosion(g):
    # 1.2 s: a low boom and a noise tail closing down.
    boom, _b = g.tone(45.0, 1.0, curve=0.7)
    drop, _d = g.tone(80.0, 0.7, drop=0.45)
    p = g.pitch()
    close, _c = g.env(0.001, 1.2, audio=False, decay_curve=0.7)
    tail = g.svf(g.noise(PINK), g.mul(p, g.lerp(200.0, 2000.0, close)), 1.0, out="Low Pass Filter")
    e, done = g.env(0.003, 1.2, decay_curve=0.6)
    g.finish(g.mix((boom, 0.6), (drop, 0.4), (g.mul(tail, e), 0.7)), done)


def amb_wind(g):
    # Loop: gusting wind; AHawkeyeAmbience turns it up above 10 m.
    g.pitch(0.03)
    gust = g.wander(0.2, 0.3, 1.0)
    band = g.svf(g.noise(PINK), g.wander(0.15, 300.0, 900.0), 2.0)
    body = g.lowpass(g.noise(PINK), 250.0)
    g.finish(g.mul(g.mix((band, 0.35), (body, 0.2)), gust))


def amb_street(g):
    # Loop: the city's low drone, and a distant horn every 15 to 40 s.
    g.pitch(0.02)
    drone = g.mix((g.sine(60.0), 0.05), (g.sine(120.0), 0.03))
    rumble = g.mul(g.lowpass(g.noise(PINK), 250.0), g.wander(0.1, 0.6, 1.0))
    honk_at = g.repeat(g.delay(g.play, 9.0), g.wander(0.05, 15.0, 40.0))
    e, _d = g.env(0.02, 0.35, trigger=honk_at, decay_curve=0.8)
    horn = g.lowpass(g.mix((g.sine(415.0), 0.5), (g.sine(520.0), 0.5)), 1500.0)
    g.finish(g.mix((drone, 1.0), (rumble, 0.25), (g.mul(horn, e), 0.06)))


def amb_lamp(g):
    # Loop: a quiet 120 Hz mains buzz; ATT_Lamp fades it out by 400 cm.
    g.pitch(0.01)
    buzz = g.mix((g.sine(120.0), 0.05), (g.lowpass(g.saw(120.0), 400.0), 0.03), (g.sine(240.0), 0.02))
    g.finish(buzz)


def ui_hover(g):
    blip, done = g.tone(1500.0, 0.07)
    g.finish(g.mul(blip, 0.12), done)


def ui_click(g):
    low, done = g.tone(900.0, 0.07)
    high, _h = g.tone(1800.0, 0.03)
    g.finish(g.mix((low, 0.2), (high, 0.1)), done)


def ui_objective_complete(g):
    # Two notes, E then B, the second held longer.
    first, _f = g.tone(660.0, 0.35, curve=0.7)
    second, done = g.tone(990.0, 0.5, curve=0.7, trigger=g.delay(g.play, 0.14))
    g.finish(g.mix((first, 0.2), (second, 0.2)), done)


def ui_new_objective(g):
    tick, done = g.tone(1250.0, 0.08)
    top, _t = g.tone(2500.0, 0.03)
    g.finish(g.mix((tick, 0.18), (top, 0.06)), done)


def ui_toast(g):
    first, _f = g.tone(880.0, 0.1)
    second, done = g.tone(1320.0, 0.12, trigger=g.delay(g.play, 0.06))
    g.finish(g.mix((first, 0.12), (second, 0.12)), done)


# (asset, folder, recipe, one shot, sound class, attenuation or None for 2D)
SOUNDS = [
    ("MS_Bow_Draw", SFX_PATH, bow_draw, False, "SCL_SFX", "ATT_World"),
    ("MS_Bow_Release", SFX_PATH, bow_release, True, "SCL_SFX", "ATT_World"),
    ("MS_Arrow_Whistle", SFX_PATH, arrow_whistle, True, "SCL_SFX", "ATT_World"),
    ("MS_Arrow_Impact_Wood", SFX_PATH, impact_wood, True, "SCL_SFX", "ATT_World"),
    ("MS_Arrow_Impact_Stone", SFX_PATH, impact_stone, True, "SCL_SFX", "ATT_World"),
    ("MS_Arrow_Impact_Flesh", SFX_PATH, impact_flesh, True, "SCL_SFX", "ATT_World"),
    ("MS_Arrow_Pickup", SFX_PATH, arrow_pickup, True, "SCL_SFX", "ATT_World"),
    ("MS_Grapple_Fire", SFX_PATH, grapple_fire, True, "SCL_SFX", "ATT_World"),
    ("MS_Grapple_Zip", SFX_PATH, grapple_zip, False, "SCL_SFX", "ATT_World"),
    ("MS_Grapple_Land", SFX_PATH, grapple_land, True, "SCL_SFX", "ATT_World"),
    ("MS_Foot_Snow_01", SFX_PATH, foot_snow_01, True, "SCL_SFX", "ATT_World"),
    ("MS_Foot_Snow_02", SFX_PATH, foot_snow_02, True, "SCL_SFX", "ATT_World"),
    ("MS_Foot_Snow_03", SFX_PATH, foot_snow_03, True, "SCL_SFX", "ATT_World"),
    ("MS_Foot_Snow_04", SFX_PATH, foot_snow_04, True, "SCL_SFX", "ATT_World"),
    ("MS_Footstep_Wood", SFX_PATH, footstep_wood, True, "SCL_SFX", "ATT_World"),
    ("MS_Footstep_Carpet", SFX_PATH, footstep_carpet, True, "SCL_SFX", "ATT_World"),
    ("MS_Land", SFX_PATH, land, True, "SCL_SFX", "ATT_World"),
    ("MS_Vault_Grunt", SFX_PATH, vault_grunt, True, "SCL_SFX", "ATT_World"),
    ("MS_Roll_Thump", SFX_PATH, roll_thump, True, "SCL_SFX", "ATT_World"),
    ("MS_Melee_Punch", SFX_PATH, punch, True, "SCL_SFX", "ATT_World"),
    ("MS_Melee_Heavy", SFX_PATH, heavy_whump, True, "SCL_SFX", "ATT_World"),
    ("MS_Melee_Block", SFX_PATH, block_clank, True, "SCL_SFX", "ATT_World"),
    ("MS_Melee_Stagger", SFX_PATH, stagger_hit, True, "SCL_SFX", "ATT_World"),
    ("MS_Parry", SFX_PATH, parry, True, "SCL_SFX", "ATT_World"),
    ("MS_Thug_Telegraph", SFX_PATH, telegraph, True, "SCL_SFX", "ATT_World"),
    ("MS_Thug_Gunshot", SFX_PATH, gunshot, True, "SCL_SFX", "ATT_World"),
    ("MS_Thug_BatSwing", SFX_PATH, bat_whoosh, True, "SCL_SFX", "ATT_World"),
    ("MS_Thug_Hurt", SFX_PATH, hurt_grunt, True, "SCL_SFX", "ATT_World"),
    ("MS_Thug_Death", SFX_PATH, death_groan, True, "SCL_SFX", "ATT_World"),
    ("MS_Trick_Putty", SFX_PATH, putty_splat, True, "SCL_SFX", "ATT_World"),
    ("MS_Trick_Bola", SFX_PATH, bola_whirl, True, "SCL_SFX", "ATT_World"),
    ("MS_Trick_Smoke", SFX_PATH, smoke_hiss, False, "SCL_SFX", "ATT_World"),
    ("MS_Trick_Emp", SFX_PATH, emp_pulse, True, "SCL_SFX", "ATT_World"),
    ("MS_Trick_Explosion", SFX_PATH, explosion, True, "SCL_SFX", "ATT_World"),
    ("MS_Amb_Wind", AMBIENT_PATH, amb_wind, False, "SCL_Ambient", None),
    ("MS_Amb_Street", AMBIENT_PATH, amb_street, False, "SCL_Ambient", None),
    ("MS_Amb_LampBuzz", AMBIENT_PATH, amb_lamp, False, "SCL_Ambient", "ATT_Lamp"),
    ("MS_UI_Hover", SFX_PATH, ui_hover, True, "SCL_UI", None),
    ("MS_UI_Click", SFX_PATH, ui_click, True, "SCL_UI", None),
    ("MS_UI_ObjectiveComplete", SFX_PATH, ui_objective_complete, True, "SCL_UI", None),
    ("MS_UI_NewObjective", SFX_PATH, ui_new_objective, True, "SCL_UI", None),
    ("MS_UI_Toast", SFX_PATH, ui_toast, True, "SCL_UI", None),
]


def sound_path(name):
    for entry in SOUNDS:
        if entry[0] == name:
            return c.asset_path(entry[1], name)
    raise KeyError(name)


# --------------------------------------------------------------------------------------
# classes, mix, attenuation
# --------------------------------------------------------------------------------------


def _object_path(value):
    return value.get_path_name().split(".")[0] if value is not None else ""


def ensure_sound_classes():
    classes = {}
    for name, parent_name in SOUND_CLASSES:
        asset, created = c.create_asset(name, CLASSES_PATH, unreal.SoundClass, c.new_factory("SoundClassFactory"),
                                        quiet=True)
        if asset is None:
            continue
        classes[name] = asset
        parent = classes.get(parent_name) if parent_name else None
        changed = created
        if parent is not None and _object_path(asset.get_editor_property("parent_class")) != _object_path(parent):
            c.set_props(asset, [("parent_class", parent)], name)
            changed = True
        if changed:
            c.save(asset)
            if parent is not None:
                c.save(parent)
        c.log("created" if created else ("updated" if changed else "exists"), c.asset_path(CLASSES_PATH, name))
    return classes


def ensure_mix():
    mix, created = c.create_asset(MIX_NAME, CLASSES_PATH, unreal.SoundMix, c.new_factory("SoundMixFactory"))
    if created and mix is not None:
        c.save(mix)
    return mix


def ensure_attenuations():
    out = {}
    for name, (inner, falloff) in ATTENUATIONS.items():
        asset, created = c.create_asset(name, CLASSES_PATH, unreal.SoundAttenuation,
                                        c.new_factory("SoundAttenuationFactory"), quiet=True)
        if asset is None:
            continue
        out[name] = asset
        settings = asset.get_editor_property("attenuation")
        wanted = [
            ("attenuate", True),
            ("spatialize", True),
            ("distance_algorithm", unreal.AttenuationDistanceModel.NATURAL_SOUND),
            ("attenuation_shape", unreal.AttenuationShape.SPHERE),
            ("attenuation_shape_extents", unreal.Vector(inner, 0.0, 0.0)),
            ("falloff_distance", falloff),
        ]
        changed = created
        for prop, value in wanted:
            current = settings.get_editor_property(prop)
            same = (abs(current.x - value.x) < 0.01) if isinstance(value, unreal.Vector) else (
                abs(float(current) - value) < 0.01 if isinstance(value, float) else current == value)
            if not same:
                settings.set_editor_property(prop, value)
                changed = True
        if changed:
            asset.set_editor_property("attenuation", settings)
            c.save(asset)
        c.log("created" if created else ("updated" if changed else "exists"), c.asset_path(CLASSES_PATH, name),
              "{0:.0f} to {1:.0f} cm".format(inner, inner + falloff))
    return out


# --------------------------------------------------------------------------------------
# building
# --------------------------------------------------------------------------------------


def _code_fingerprint(code):
    """Bytecode, constants and names of a code object and the ones nested in it. The commandlet runs
    this file through exec, so inspect.getsource has nothing to read."""
    parts = [code.co_code.hex(), repr(code.co_names)]
    for const in code.co_consts:
        parts.append(_code_fingerprint(const) if inspect.iscode(const) else repr(const))
    return "|".join(parts)


def recipe_hash(recipe, one_shot, sound_class, attenuation):
    helpers = [_code_fingerprint(f.__code__) for f in (_footstep,)]
    helpers += [_code_fingerprint(m.__code__) for _n, m in sorted(vars(Graph).items()) if inspect.isfunction(m)]
    text = "|".join([AUDIO_BUILD, _code_fingerprint(recipe.__code__)] + helpers
                    + [str(one_shot), sound_class, attenuation or "2d"])
    return hashlib.sha1(text.encode("utf-8")).hexdigest()[:16]


def build_graph(name, recipe, one_shot):
    graph = Graph(name, one_shot)
    recipe(graph)
    return graph


def apply_asset_settings(asset, sound_class, attenuation):
    values = [("sound_class_object", sound_class)]
    values.append(("attenuation_settings", attenuation))
    c.set_props(asset, values, asset.get_name())


def ensure_sound(entry, classes, attenuations):
    name, folder, recipe, one_shot, class_name, attenuation_name = entry
    full = c.asset_path(folder, name)
    wanted = recipe_hash(recipe, one_shot, class_name, attenuation_name)
    existing = c.load_or_none(full)
    if existing is not None and unreal.EditorAssetLibrary.get_metadata_tag(existing, HASH_TAG) == wanted:
        c.log("exists", full)
        return existing, False

    if existing is not None:
        c.log("FAILED", full, "a stale build is still loaded; delete_stale_sounds runs first")
        return None, False
    c.ensure_directory(folder)
    graph = build_graph(name, recipe, one_shot)
    action = "updated" if full in _REPLACED else "created"
    editor = unreal.get_editor_subsystem(unreal.MetaSoundEditorSubsystem)
    # BuildToAsset hands back the document interface, not the UObject: load the asset it saved.
    _ok(editor.build_to_asset(graph.builder, AUTHOR, name, folder, None), "build " + name)
    asset = c.load_or_none(full)
    if asset is None:
        c.log("FAILED", full, "the builder made no asset")
        return None, False

    apply_asset_settings(asset, classes.get(class_name), attenuations.get(attenuation_name))
    unreal.EditorAssetLibrary.set_metadata_tag(asset, HASH_TAG, wanted)
    c.save(asset)
    c.log(action, full, "{0} nodes, {1}".format(graph.nodes, "one-shot" if one_shot else "loop"))
    return asset, True


_REPLACED = set()


def delete_stale_sounds():
    """Deletes every MetaSound whose recipe changed, then collects garbage so the path is free.

    A saved MetaSound cannot be overwritten by a builder headless (BuildAndOverwriteMetaSound refuses
    serialized assets), so a changed one is replaced. Its users hold soft paths, which the new asset
    at the same path satisfies.
    """
    stale = []
    for name, folder, recipe, one_shot, class_name, attenuation_name in SOUNDS:
        full = c.asset_path(folder, name)
        existing = c.load_or_none(full)
        wanted = recipe_hash(recipe, one_shot, class_name, attenuation_name)
        if existing is not None and unreal.EditorAssetLibrary.get_metadata_tag(existing, HASH_TAG) != wanted:
            stale.append(full)
    existing = None
    for full in stale:
        if unreal.EditorAssetLibrary.delete_asset(full):
            _REPLACED.add(full)
        else:
            c.log("FAILED", full, "could not delete the stale build")
    if stale:
        unreal.SystemLibrary.collect_garbage()
    return len(stale)


def run():
    for path in (CLASSES_PATH, SFX_PATH, AMBIENT_PATH):
        c.ensure_directory(path)
    delete_stale_sounds()
    classes = ensure_sound_classes()
    ensure_mix()
    attenuations = ensure_attenuations()

    built = 0
    sounds = {}
    for entry in SOUNDS:
        try:
            asset, changed = ensure_sound(entry, classes, attenuations)
            if asset is not None:
                sounds[entry[0]] = asset
            built += 1 if changed else 0
        except Exception as exc:  # noqa: BLE001 - one broken recipe must not stop the rest
            c.log_error(c.asset_path(entry[1], entry[0]), exc)
    c.log("exists" if not built else "updated", AUDIO_ROOT,
          "{0} MetaSounds, {1} built this run".format(len(sounds), built))
    return sounds


if __name__ == "__main__":
    run()
    c.print_summary("audio")
