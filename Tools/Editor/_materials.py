"""Procedural material helpers for the Castle art passes.

Everything here is built out of engine material expressions - no imported textures, no
downloads. The room-art pass (``create_room_art.py``) is the only consumer so far.

Conventions are the same as ``_common.py``: check before creating, save only what
changed, print one ``created`` / ``exists`` / ``updated`` / ``FAILED`` line per asset,
and never let one broken node graph abort the script.

Materials live in ``/Game/Materials`` per claude-docs/asset-conventions.md.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

MATERIALS_PATH = "/Game/Materials"

# Wall / floor / metal
M_CONCRETE = MATERIALS_PATH + "/M_Concrete"
M_CONCRETE_FLOOR = MATERIALS_PATH + "/M_ConcreteFloor"
M_STEEL_PAINTED = MATERIALS_PATH + "/M_SteelPainted"

# Emissive master + its instances
M_EMISSIVE = MATERIALS_PATH + "/M_Emissive"
M_FLUORESCENT_FLICKER = MATERIALS_PATH + "/M_FluorescentFlicker"
MI_FLUORESCENT_TUBE = MATERIALS_PATH + "/MI_FluorescentTube"
MI_RED_EMERGENCY = MATERIALS_PATH + "/MI_RedEmergency"
MI_KEYCARD_STRIPE = MATERIALS_PATH + "/MI_KeycardStripe"
MI_MONITOR = MATERIALS_PATH + "/MI_Monitor"

# Props
M_PISTOL = MATERIALS_PATH + "/M_Pistol"
M_KEYCARD_BODY = MATERIALS_PATH + "/M_KeycardBody"

EMISSIVE_COLOR_PARAM = "Color"
EMISSIVE_INTENSITY_PARAM = "Intensity"


# --------------------------------------------------------------------------------------
# tiny wrappers over MaterialEditingLibrary
# --------------------------------------------------------------------------------------


def _mel():
    return unreal.MaterialEditingLibrary


def expr(material, class_name, x=0, y=0, props=None, context=""):
    """Create one material expression. Returns None when the node type is unavailable.

    ``props`` is a list of (name, value) pairs handed to ``c.set_props``.
    """
    cls = getattr(unreal, class_name, None)
    if cls is None:
        unreal.log_warning("[Castle] skipped   {0}: {1} not scriptable".format(
            context or material.get_name(), class_name))
        return None
    try:
        node = _mel().create_material_expression(material, cls, x, y)
    except Exception as exc:  # noqa: BLE001
        c.log_error("create_material_expression " + class_name, exc)
        return None
    if node is not None and props:
        c.set_props(node, props, context or class_name)
    return node


def connect(from_node, from_output, to_node, to_input):
    """Wire two expressions. False (with a warning) when either end is missing."""
    if from_node is None or to_node is None:
        return False
    try:
        return bool(_mel().connect_material_expressions(
            from_node, from_output, to_node, to_input))
    except Exception as exc:  # noqa: BLE001
        c.log_error("connect {0} -> {1}".format(from_output, to_input), exc)
        return False


def connect_property(from_node, material_property, from_output=""):
    if from_node is None:
        return False
    try:
        return bool(_mel().connect_material_property(
            from_node, from_output, material_property))
    except Exception as exc:  # noqa: BLE001
        c.log_error("connect_material_property " + str(material_property), exc)
        return False


def constant(material, value, x=0, y=0):
    return expr(material, "MaterialExpressionConstant", x, y, [("r", float(value))])


def constant3(material, rgb, x=0, y=0):
    return expr(
        material,
        "MaterialExpressionConstant3Vector",
        x, y,
        [("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))],
    )


def set_scalar_property(material, value, material_property, x=0, y=0):
    return connect_property(constant(material, value, x, y), material_property)


def noise(material, scale, x=0, y=0, out_min=0.0, out_max=1.0, levels=3, turbulence=True):
    """A MaterialExpressionNoise with its Position input left unconnected.

    Unconnected Position defaults to absolute world position, which is exactly what we
    want for world-aligned concrete - no UVs on the greybox cubes to worry about.
    """
    props = [
        ("scale", float(scale)),
        ("output_min", float(out_min)),
        ("output_max", float(out_max)),
        ("levels", int(levels)),
        ("turbulence", bool(turbulence)),
    ]
    node = expr(material, "MaterialExpressionNoise", x, y, props, "Noise")
    if node is not None:
        # Cheapest ALU noise; the enum member name has moved around between versions.
        fn = getattr(unreal, "NoiseFunction", None)
        if fn is not None:
            for member in ("NOISEFUNCTION_GRADIENT_ALU", "NOISEFUNCTION_VALUE_ALU"):
                value = getattr(fn, member, None)
                if value is not None:
                    c.set_props(node, [("noise_function", value)], "Noise")
                    break
    return node


def lerp(material, a_node, b_node, alpha_node, x=0, y=0, const_a=None, const_b=None):
    props = []
    if const_a is not None:
        props.append(("const_a", float(const_a)))
    if const_b is not None:
        props.append(("const_b", float(const_b)))
    node = expr(material, "MaterialExpressionLinearInterpolate", x, y, props, "Lerp")
    connect(a_node, "", node, "A")
    connect(b_node, "", node, "B")
    connect(alpha_node, "", node, "Alpha")
    return node


def multiply(material, a_node, b_node, x=0, y=0, const_b=None):
    props = [("const_b", float(const_b))] if const_b is not None else []
    node = expr(material, "MaterialExpressionMultiply", x, y, props, "Multiply")
    connect(a_node, "", node, "A")
    connect(b_node, "", node, "B")
    return node


def add(material, a_node, b_node, x=0, y=0, const_b=None):
    props = [("const_b", float(const_b))] if const_b is not None else []
    node = expr(material, "MaterialExpressionAdd", x, y, props, "Add")
    connect(a_node, "", node, "A")
    connect(b_node, "", node, "B")
    return node


def subtract(material, a_node, b_node, x=0, y=0, const_b=None):
    props = [("const_b", float(const_b))] if const_b is not None else []
    node = expr(material, "MaterialExpressionSubtract", x, y, props, "Subtract")
    connect(a_node, "", node, "A")
    connect(b_node, "", node, "B")
    return node


def divide(material, a_node, b_node, x=0, y=0, const_b=None):
    props = [("const_b", float(const_b))] if const_b is not None else []
    node = expr(material, "MaterialExpressionDivide", x, y, props, "Divide")
    connect(a_node, "", node, "A")
    connect(b_node, "", node, "B")
    return node


def clamp01(material, input_node, x=0, y=0):
    node = expr(
        material,
        "MaterialExpressionClamp",
        x, y,
        [("min_default", 0.0), ("max_default", 1.0)],
        "Clamp",
    )
    connect(input_node, "", node, "")
    return node


def frac(material, input_node, x=0, y=0):
    node = expr(material, "MaterialExpressionFrac", x, y, None, "Frac")
    connect(input_node, "", node, "")
    return node


def absolute(material, input_node, x=0, y=0):
    node = expr(material, "MaterialExpressionAbs", x, y, None, "Abs")
    connect(input_node, "", node, "")
    return node


def sine(material, input_node, x=0, y=0):
    node = expr(material, "MaterialExpressionSine", x, y, None, "Sine")
    connect(input_node, "", node, "")
    return node


def maximum(material, a_node, b_node, x=0, y=0):
    node = expr(material, "MaterialExpressionMax", x, y, None, "Max")
    connect(a_node, "", node, "A")
    connect(b_node, "", node, "B")
    return node


def component_mask(material, input_node, r=False, g=False, b=False, a=False, x=0, y=0):
    node = expr(
        material,
        "MaterialExpressionComponentMask",
        x, y,
        [("r", r), ("g", g), ("b", b), ("a", a)],
        "ComponentMask",
    )
    connect(input_node, "", node, "")
    return node


def world_position(material, x=0, y=0):
    return expr(material, "MaterialExpressionWorldPosition", x, y, None, "WorldPosition")


def time_node(material, x=0, y=0):
    return expr(material, "MaterialExpressionTime", x, y, None, "Time")


def step(material, input_node, edge, x=0, y=0, sharpness=200.0):
    """step(edge, x) as saturate((x - edge) * sharpness). No If node needed."""
    shifted = subtract(material, input_node, None, x, y, const_b=float(edge))
    scaled = multiply(material, shifted, None, x + 150, y, const_b=float(sharpness))
    return clamp01(material, scaled, x + 300, y)


def height_darken(material, floor_meters=3.0, floor_value=0.45, x=-900, y=400):
    """1.0 high up, ``floor_value`` at Z = 0 - grime pooling at the bottom of walls."""
    wp = world_position(material, x, y)
    z = component_mask(material, wp, b=True, x=x + 200, y=y)
    normalised = divide(material, z, None, x + 400, y, const_b=float(floor_meters) * 100.0)
    mask = clamp01(material, normalised, x + 600, y)
    return lerp(material, None, None, mask, x + 800, y,
                const_a=float(floor_value), const_b=1.0)


# --------------------------------------------------------------------------------------
# material / instance creation
# --------------------------------------------------------------------------------------


def _split(full_path):
    full_path = full_path.rstrip("/")
    return full_path.rsplit("/", 1)[0], full_path.rsplit("/", 1)[1]


def ensure_material(full_path, build_fn, rebuild=False):
    """Idempotent Material. ``build_fn(material)`` wires the graph on first creation.

    An existing material is returned untouched unless ``rebuild`` is True, in which case
    its expressions are cleared and ``build_fn`` runs again.
    """
    package_path, name = _split(full_path)
    existing = c.load_or_none(full_path)
    if existing is not None and not rebuild:
        c.log("exists", full_path)
        return existing

    material = existing
    created = False
    try:
        if material is None:
            c.ensure_directory(package_path)
            factory = c.new_factory("MaterialFactoryNew")
            if factory is None:
                c.log("FAILED", full_path, "MaterialFactoryNew unavailable")
                return None
            material = c.asset_tools().create_asset(
                name, package_path, unreal.Material, factory)
            if material is None:
                c.log("FAILED", full_path, "create_asset returned None")
                return None
            created = True
        else:
            try:
                unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
            except Exception as exc:  # noqa: BLE001
                c.log_error("delete_all_material_expressions " + full_path, exc)

        build_fn(material)
        unreal.MaterialEditingLibrary.recompile_material(material)
        c.save(material)
        c.log("created" if created else "updated", full_path)
        return material
    except Exception as exc:  # noqa: BLE001
        c.log_error("ensure_material " + full_path, exc)
        return material


_COMPONENT_SETS = (("x", "y", "z", "w"), ("r", "g", "b", "a"), ("x", "y", "z"))


def _components(value):
    """Numeric components of a Vector / Vector4 / LinearColor / Color, or None."""
    for attrs in _COMPONENT_SETS:
        if all(hasattr(value, attr) for attr in attrs):
            try:
                return [float(getattr(value, attr)) for attr in attrs]
            except (TypeError, ValueError):
                return None
    return None


def same_value(current, wanted, tolerance=1e-4):
    """True when a property already holds ``wanted``.

    Everything numeric compares with a tolerance, because a float the editor stored as
    float32 never reads back exactly equal to the Python literal that wrote it - and an
    exact comparison would rewrite (and dirty) the asset on every run.
    """
    if current is None:
        return False
    if isinstance(wanted, bool) or isinstance(current, bool):
        return bool(current) == bool(wanted)
    if isinstance(wanted, (int, float)) and isinstance(current, (int, float)):
        return abs(float(current) - float(wanted)) <= tolerance

    mine, theirs = _components(current), _components(wanted)
    if mine is not None and theirs is not None and len(mine) == len(theirs):
        return all(abs(a - b) <= tolerance for a, b in zip(mine, theirs))

    try:
        return bool(current == wanted)
    except Exception:  # noqa: BLE001
        return False


def instance_vector(instance, param):
    try:
        return unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
            instance, param)
    except Exception:  # noqa: BLE001
        return None


def instance_scalar(instance, param):
    try:
        return unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(
            instance, param)
    except Exception:  # noqa: BLE001
        return None


def ensure_material_instance(full_path, parent, vectors=None, scalars=None):
    """Idempotent MaterialInstanceConstant. Parameters are only written when they differ."""
    package_path, name = _split(full_path)
    if parent is None:
        c.log("FAILED", full_path, "parent material is None")
        return None

    instance = c.load_or_none(full_path)
    created = False
    if instance is None:
        try:
            c.ensure_directory(package_path)
            factory = c.new_factory("MaterialInstanceConstantFactoryNew")
            if factory is None:
                c.log("FAILED", full_path, "MaterialInstanceConstantFactoryNew unavailable")
                return None
            instance = c.asset_tools().create_asset(
                name, package_path, unreal.MaterialInstanceConstant, factory)
            if instance is None:
                c.log("FAILED", full_path, "create_asset returned None")
                return None
            created = True
        except Exception as exc:  # noqa: BLE001
            c.log_error("ensure_material_instance " + full_path, exc)
            return None

    changed = created
    try:
        if instance.get_editor_property("parent") != parent:
            instance.set_editor_property("parent", parent)
            changed = True
    except Exception as exc:  # noqa: BLE001
        c.log_error("set parent on " + full_path, exc)

    for param, rgb in (vectors or []):
        wanted = unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)
        if same_value(instance_vector(instance, param), wanted):
            continue
        try:
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
                instance, param, wanted)
            changed = True
        except Exception as exc:  # noqa: BLE001
            c.log_error("set vector param {0} on {1}".format(param, full_path), exc)

    for param, value in (scalars or []):
        if same_value(instance_scalar(instance, param), float(value)):
            continue
        try:
            unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
                instance, param, float(value))
            changed = True
        except Exception as exc:  # noqa: BLE001
            c.log_error("set scalar param {0} on {1}".format(param, full_path), exc)

    if changed:
        c.save(instance)
    c.log("created" if created else ("updated" if changed else "exists"), full_path)
    return instance


# --------------------------------------------------------------------------------------
# the graphs
# --------------------------------------------------------------------------------------


def _build_concrete(material):
    """Two greys lerped by fine noise, multiplied by large-scale grime and a floor mask."""
    fine = noise(material, 0.05, -1200, -400, levels=4)
    dark = constant3(material, (0.18, 0.18, 0.18), -1200, -200)
    light = constant3(material, (0.30, 0.30, 0.30), -1200, -60)
    base = lerp(material, dark, light, fine, -900, -200)

    # Large, soft blotches: 0.55..1.0, so it only ever darkens.
    grime = noise(material, 0.004, -1200, 100, out_min=0.55, out_max=1.0, levels=2)
    with_grime = multiply(material, base, grime, -650, -100)

    floor_mask = height_darken(material, floor_meters=3.0, floor_value=0.45)
    final = multiply(material, with_grime, floor_mask, -350, -100)
    connect_property(final, unreal.MaterialProperty.MP_BASE_COLOR)

    set_scalar_property(material, 0.85, unreal.MaterialProperty.MP_ROUGHNESS, -350, 200)


def _build_concrete_floor(material):
    """Darker concrete with a world-aligned 100 cm grout grid."""
    fine = noise(material, 0.05, -1600, -400, levels=4)
    dark = constant3(material, (0.10, 0.10, 0.10), -1600, -200)
    light = constant3(material, (0.16, 0.16, 0.16), -1600, -60)
    base = lerp(material, dark, light, fine, -1350, -200)

    # frac(worldXY / 100) -> distance from tile centre -> grout on the outer 3 %.
    wp = world_position(material, -1900, 300)
    xy = component_mask(material, wp, r=True, g=True, x=-1700, y=300)
    tiles = divide(material, xy, None, -1500, 300, const_b=100.0)
    f = frac(material, tiles, -1350, 300)
    centred = subtract(material, f, None, -1200, 300, const_b=0.5)
    dist = absolute(material, centred, -1050, 300)
    dx = component_mask(material, dist, r=True, x=-900, y=250)
    dy = component_mask(material, dist, g=True, x=-900, y=380)
    worst = maximum(material, dx, dy, -750, 300)
    grout_mask = step(material, worst, 0.47, -600, 300)

    grout = constant3(material, (0.045, 0.045, 0.05), -1350, 520)
    tiled = lerp(material, base, grout, grout_mask, -100, 0)

    floor_dirt = height_darken(material, floor_meters=3.0, floor_value=0.7, x=-900, y=700)
    final = multiply(material, tiled, floor_dirt, 100, 0)
    connect_property(final, unreal.MaterialProperty.MP_BASE_COLOR)

    set_scalar_property(material, 0.95, unreal.MaterialProperty.MP_ROUGHNESS, 100, 300)


def _build_steel_painted(material):
    """Dark green-grey paint with noise scuffs showing brighter metal."""
    scuff = noise(material, 0.6, -900, -400, levels=3)
    paint = constant3(material, (0.05, 0.07, 0.06), -900, -200)
    worn = constant3(material, (0.16, 0.18, 0.17), -900, -60)
    base = lerp(material, paint, worn, scuff, -600, -200)
    connect_property(base, unreal.MaterialProperty.MP_BASE_COLOR)

    set_scalar_property(material, 0.5, unreal.MaterialProperty.MP_ROUGHNESS, -600, 150)
    set_scalar_property(material, 0.6, unreal.MaterialProperty.MP_METALLIC, -600, 280)


def _build_flat(rgb, roughness, metallic=0.0):
    def build(material):
        connect_property(
            constant3(material, rgb, -600, -200), unreal.MaterialProperty.MP_BASE_COLOR)
        set_scalar_property(material, roughness, unreal.MaterialProperty.MP_ROUGHNESS, -600, 0)
        if metallic:
            set_scalar_property(
                material, metallic, unreal.MaterialProperty.MP_METALLIC, -600, 150)
    return build


def _emissive_params(material, x=-1100, y=-200):
    """The shared (Color, Intensity) parameter pair used by every light material."""
    color = expr(
        material,
        "MaterialExpressionVectorParameter",
        x, y,
        [
            ("parameter_name", EMISSIVE_COLOR_PARAM),
            ("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0)),
        ],
        "Color",
    )
    intensity = expr(
        material,
        "MaterialExpressionScalarParameter",
        x, y + 200,
        [("parameter_name", EMISSIVE_INTENSITY_PARAM), ("default_value", 8.0)],
        "Intensity",
    )
    return color, intensity


def _build_emissive(material):
    color, intensity = _emissive_params(material)
    connect_property(multiply(material, color, intensity, -800, -100),
                     unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    connect_property(constant3(material, (0.02, 0.02, 0.02), -800, 250),
                     unreal.MaterialProperty.MP_BASE_COLOR)
    set_scalar_property(material, 0.4, unreal.MaterialProperty.MP_ROUGHNESS, -800, 400)


def _build_fluorescent_flicker(material):
    """emissive * (1 + 0.3 * sin(Time * 37) * step(0.6, frac(Time * 2.3)))

    A bad ballast: mostly steady, with bursts of buzz. The light actor itself stays
    steady until a UFlickerLightComponent exists on the C++ side.
    """
    color, intensity = _emissive_params(material)
    steady = multiply(material, color, intensity, -800, -200)

    t = time_node(material, -1400, 300)
    fast = multiply(material, t, None, -1200, 300, const_b=37.0)
    wave = sine(material, fast, -1050, 300)
    amount = multiply(material, wave, None, -900, 300, const_b=0.3)

    slow = multiply(material, t, None, -1200, 550, const_b=2.3)
    cycle = frac(material, slow, -1050, 550)
    gate = step(material, cycle, 0.6, -900, 550)

    burst = multiply(material, amount, gate, -300, 400)
    modulation = add(material, burst, None, -150, 400, const_b=1.0)

    connect_property(multiply(material, steady, modulation, 0, 0),
                     unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    connect_property(constant3(material, (0.02, 0.02, 0.02), -800, 700),
                     unreal.MaterialProperty.MP_BASE_COLOR)
    set_scalar_property(material, 0.4, unreal.MaterialProperty.MP_ROUGHNESS, -800, 850)


# --------------------------------------------------------------------------------------
# public entry points
# --------------------------------------------------------------------------------------


def ensure_surface_materials():
    """M_Concrete / M_ConcreteFloor / M_SteelPainted. Returns a dict keyed by short name."""
    out = {}
    for key, path, build in (
        ("concrete", M_CONCRETE, _build_concrete),
        ("concrete_floor", M_CONCRETE_FLOOR, _build_concrete_floor),
        ("steel", M_STEEL_PAINTED, _build_steel_painted),
    ):
        try:
            out[key] = ensure_material(path, build)
        except Exception as exc:  # noqa: BLE001
            c.log_error("ensure_surface_materials " + path, exc)
            out[key] = None
    return out


def ensure_light_materials(intensity_factor=1.0):
    """M_Emissive + M_FluorescentFlicker and the four lamp instances.

    ``intensity_factor`` scales the tuned strengths below - they were eyeballed at the
    room art pass's -4.5 EV default, so a brighter exposure preset needs a proportionally
    dimmer emissive or the tubes blow out. create_room_art.py derives this from
    ROOM_EXPOSURE_EV; anyone calling this module directly gets the -4.5 tuning as-is.
    """
    out = {}
    try:
        out["emissive"] = ensure_material(M_EMISSIVE, _build_emissive)
    except Exception as exc:  # noqa: BLE001
        c.log_error("ensure_light_materials " + M_EMISSIVE, exc)
        out["emissive"] = None

    try:
        out["flicker"] = ensure_material(M_FLUORESCENT_FLICKER, _build_fluorescent_flicker)
    except Exception as exc:  # noqa: BLE001
        c.log_error("ensure_light_materials " + M_FLUORESCENT_FLICKER, exc)
        out["flicker"] = None

    instances = (
        # Emissive strength is in the same ballpark as the lights themselves, because the
        # scene is graded several stops down - a tube at 8 would read as dark plastic.
        # These are the -4.5 EV values; intensity_factor scales them for other presets.
        ("tube", MI_FLUORESCENT_TUBE, (0.85, 0.90, 1.00), 150.0),
        ("red", MI_RED_EMERGENCY, (1.00, 0.05, 0.02), 250.0),
        ("stripe", MI_KEYCARD_STRIPE, (0.10, 0.90, 0.30), 60.0),
        # A CRT left on in the guard station: bright enough to glow, too dim to light the room.
        ("monitor", MI_MONITOR, (0.20, 0.55, 1.00), 25.0),
    )
    for key, path, rgb, strength in instances:
        try:
            out[key] = ensure_material_instance(
                path,
                out.get("emissive"),
                vectors=[(EMISSIVE_COLOR_PARAM, rgb)],
                scalars=[(EMISSIVE_INTENSITY_PARAM, strength * intensity_factor)],
            )
        except Exception as exc:  # noqa: BLE001
            c.log_error("ensure_light_materials " + path, exc)
            out[key] = None
    return out


def ensure_prop_materials():
    """Materials the pickup Blueprints want: a near-black pistol and white keycard plastic.

    Returned as a dict so create_world_blueprints.py can pick them up without importing
    the room-art pass: {'pistol': M_Pistol, 'keycard': M_KeycardBody, 'stripe': MI_KeycardStripe}.
    """
    out = {}
    try:
        out["pistol"] = ensure_material(M_PISTOL, _build_flat((0.02, 0.02, 0.02), 0.35, 0.9))
    except Exception as exc:  # noqa: BLE001
        c.log_error("ensure_prop_materials " + M_PISTOL, exc)
        out["pistol"] = None
    try:
        out["keycard"] = ensure_material(
            M_KEYCARD_BODY, _build_flat((0.85, 0.85, 0.85), 0.4))
    except Exception as exc:  # noqa: BLE001
        c.log_error("ensure_prop_materials " + M_KEYCARD_BODY, exc)
        out["keycard"] = None
    out["stripe"] = c.load_or_none(MI_KEYCARD_STRIPE)
    return out


def ensure_all(intensity_factor=1.0):
    """Every material the room-art pass needs, in one dict."""
    materials = {}
    materials.update(ensure_surface_materials())
    materials.update(ensure_light_materials(intensity_factor))
    materials.update(ensure_prop_materials())
    return materials


def run(intensity_factor=1.0):
    c.ensure_directory(MATERIALS_PATH)
    return ensure_all(intensity_factor)


if __name__ == "__main__":
    run()
    c.print_summary("materials")
