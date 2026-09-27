"""Create the street crimes' assets and plan where they can start.

    /Game/Crimes/DA_Crime_<type>              UCrimeDefinition: mugging, robbery, ambush, rooftop
    /Game/Blueprints/AI/BP_Civilian           ACivilian: the mugging's victim, the old SK_Mannequin
    /Game/Characters/Civilian/M_Civilian      the victim's neutral grey (skeletal usage)

and ``plan_crime_spots(district)``, which generate_city.py places as City_CrimeSpot_<n> (ACrimeSpot):
twelve places a crime can start, planned from the same OpenStreetMap records the city is built from,
so nothing is placed by hand:

* eight on the street at block corners (create_challenges.street_corners: on the sidewalk 2.5 m out
  from a building's corner where two roads meet), nearest the PlayerStart first, each with an escape
  point on the sidewalk 60 m along one of its streets for the robbery;
* four on roofs 8 to 25 m tall with a grapple anchor, a spot 4.5 m clear of every edge and prop, and
  no chapter or challenge business on them;
* none within 40 m of the safehouse door or a challenge pedestal, none within 25 m of chapter 1's
  thugs or archers, and every two at least 35 m apart.

Street spots take the mugging, robbery and ambush; rooftops the rooftop crime. The names are
bracketed placeholders ("[Crime: mugging]"); civilians have no lines, only "[thank you]".

Idempotent: an asset is rewritten only when its planned values differ from what it holds.

    UnrealEditor-Cmd.exe Hawkeye.uproject -run=pythonscript ^
        -script="Tools\\Editor\\create_crimes.py" -unattended -nullrhi -nosplash -nop4 -stdout
"""

import math
import os
import re
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402
import _geo as geo  # noqa: E402
import _materials as m  # noqa: E402
import create_blueprints as cb  # noqa: E402

CRIME_PATH = "/Game/Crimes"
ASSET_PREFIX = "DA_Crime_"
AI_PATH = "/Game/Blueprints/AI"
BOSS_PATH = "/Game/Blueprints/Bosses"
CIVILIAN_BP = "BP_Civilian"
CIVILIAN_MATERIAL_PATH = "/Game/Characters/Civilian"
M_CIVILIAN = CIVILIAN_MATERIAL_PATH + "/M_Civilian"
CIVILIAN_GREY = (0.32, 0.32, 0.31)
MANNEQUIN_MESH_PATH = "/Game/Mannequin/Character/Mesh/SK_Mannequin"
MANNEQUIN_IDLE_PATH = "/Game/Mannequin/Animations/ThirdPersonIdle"
MANNEQUIN_RUN_PATH = "/Game/Mannequin/Animations/ThirdPersonRun"
MESH_LOCATION = unreal.Vector(0.0, 0.0, -96.0)
MESH_ROTATION = unreal.Rotator(0.0, 0.0, -90.0)

# --- the spots --------------------------------------------------------------------------------------
STREET_SPOTS = 8
ROOF_SPOTS = 4
SEARCH_RADIUS = 25000.0         # cm from the PlayerStart: the block and its streets (one block at a time)
SAFEHOUSE_CLEAR = 4000.0        # cm from the safehouse door
PEDESTAL_CLEAR = 4000.0         # cm from any challenge pedestal
CHAPTER_THUG_CLEAR = 2500.0     # cm from chapter 1's thugs and archers
SPOT_APART = 3500.0             # cm between two spots
ROOF_HEIGHT = (8.0, 25.0)       # m
ROOF_EDGE_CLEAR = 450.0         # cm from any roof edge to a rooftop spot (the roster stands 3.5 m round it)
ESCAPE_DISTANCE = 6000.0        # cm from a street spot to the robbery's escape point
ESCAPE_TOLERANCE = 600.0        # cm either way the escape point may land from ESCAPE_DISTANCE
ESCAPE_STEP = 200.0             # cm between escape candidates along a street

# --- the crimes -------------------------------------------------------------------------------------
# (id, type, roster [(weapon, count, blueprint)], radius, time to fail, alert on start)
CRIMES = [
    ("mugging", "Mugging", [("Fists", 1, "BP_Thug"), ("Bat", 1, "BP_Thug")], 160.0, 45.0, False),
    ("robbery", "Robbery", [("Fists", 1, "BP_Thug"), ("Bat", 1, "BP_Thug"), ("Pistol", 1, "BP_Thug")], 250.0, 40.0, False),
    ("ambush", "Ambush", [("Fists", 1, "BP_Thug"), ("Bat", 2, "BP_Thug"), ("Pistol", 1, "BP_Thug")], 500.0, 45.0, True),
    ("rooftop", "Rooftop", [("Fists", 1, "BP_Thug"), ("Bat", 1, "BP_Thug"), ("Bow", 1, "BP_Archer")], 350.0, 45.0, False),
]
STREET_CRIMES = ("mugging", "robbery", "ambush")
ROOF_CRIMES = ("rooftop",)
VICTIM_HITS = 3
REWARD_ARROWS = 5


def _enum(enum_name, value):
    """unreal.CrimeType.MUGGING from ("CrimeType", "Mugging")."""
    enum = getattr(unreal, enum_name)
    return getattr(enum, re.sub(r"(?<!^)(?=[A-Z])", "_", value).upper())


def _path(value):
    """Package path of an object, class or soft reference ("" for none), so reads compare with writes."""
    if value is None:
        return ""
    for getter in ("get_path_name", "export_text"):
        method = getattr(value, getter, None)
        if method is not None:
            try:
                return str(method()).split(".")[0]
            except Exception:  # noqa: BLE001
                pass
    return str(value).split(".")[0]


# --------------------------------------------------------------------------------------------------
# the spots
# --------------------------------------------------------------------------------------------------


def _dist2(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def _blockers(district, gen, cc):
    """[(x, y, clearance)] a spot must keep away from: the safehouse, every pedestal, chapter 1's fighters."""
    out = []
    safehouse = gen.safehouse_spot(district)
    if safehouse is not None:
        out.append((safehouse["x"], safehouse["y"], SAFEHOUSE_CLEAR))
    for plan in cc.plan_challenges(district):
        out.append((plan["start"][0], plan["start"][1], PEDESTAL_CLEAR))
    thugs, _points, _roof = gen.thug_placements(district)
    for t in thugs:
        out.append((t[1], t[2], CHAPTER_THUG_CLEAR))
    for a in gen.archer_placements(district):
        out.append((a[1], a[2], CHAPTER_THUG_CLEAR))
    return out


def _clear(pt, blockers, chosen):
    return all(_dist2(pt, (x, y)) >= r for x, y, r in blockers) and all(_dist2(pt, s["at"]) >= SPOT_APART for s in chosen)


def escape_point(world, cc, spot):
    """A sidewalk point about ESCAPE_DISTANCE from spot along one of the streets it is on, or None."""
    best = None
    for a, b, _half, _name in world.roads:
        if cc._segment_point_distance(spot, a, b)[0] > cc.CORNER_ROADS:
            continue
        ux, uy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(ux, uy)
        if length < 1.0:
            continue
        ux, uy = ux / length, uy / length
        for sign in (1.0, -1.0):
            offset = -ESCAPE_TOLERANCE
            while offset <= ESCAPE_TOLERANCE:
                reach = ESCAPE_DISTANCE + offset
                offset += ESCAPE_STEP
                pt = (spot[0] + ux * sign * reach, spot[1] + uy * sign * reach)
                if not cc.on_sidewalk(world, pt) or not cc.street_clear(world, pt, 150.0):
                    continue
                miss = abs(_dist2(pt, spot) - ESCAPE_DISTANCE)
                if best is None or miss < best[0]:
                    best = (miss, pt)
    return best[1] if best is not None else None


def _street_spots(world, gen, cc, blockers):
    chosen = []
    for spot, top, out in cc.street_corners(world):
        if len(chosen) >= STREET_SPOTS:
            break
        if gen.ring_distance(world.start, top[1]) > SEARCH_RADIUS or not _clear(spot, blockers, chosen):
            continue
        escape = escape_point(world, cc, spot)
        if escape is None:
            continue
        # Facing in toward the corner's storefronts: where a robbery happens.
        yaw = math.degrees(math.atan2(-out[1], -out[0]))
        chosen.append({"at": spot, "z": gen.SIDEWALK_TOP, "yaw": yaw, "rooftop": False, "osm": top[0]["id"],
                       "escape": (escape[0], escape[1], gen.SIDEWALK_TOP), "kind": "corner"})
    return chosen


def _roof_spots(world, gen, cc, blockers, taken):
    anchored = {a[6] for a in world.anchors}
    tops = sorted(world.tops, key=lambda t: (gen.ring_distance(world.start, t[1]), t[0]["id"]))
    chosen = []
    for top in tops:
        if len(chosen) >= ROOF_SPOTS:
            break
        rec = top[0]
        if gen.ring_distance(world.start, top[1]) > SEARCH_RADIUS or rec["id"] in taken or rec["id"] not in anchored \
                or not ROOF_HEIGHT[0] <= rec["height_m"] <= ROOF_HEIGHT[1]:
            continue
        spots, _props = cc.roof_spots(world, top, inset=ROOF_EDGE_CLEAR)
        centre = geo.centroid(top[1])
        spots = [s for s in spots if _clear(s, blockers, chosen)]
        if not spots:
            continue
        spot = min(spots, key=lambda s: (_dist2(s, centre), s))
        yaw = math.degrees(math.atan2(world.start[1] - spot[1], world.start[0] - spot[0]))
        chosen.append({"at": spot, "z": world.roof_z(top), "yaw": yaw, "rooftop": True, "osm": rec["id"],
                       "escape": None, "kind": "roof"})
    return chosen


_PLAN_CACHE = {}


def plan_crime_spots(district):
    """The twelve spots, street ones first, each {'index', 'at', 'z', 'yaw', 'rooftop', 'osm', 'escape',
    'kind', 'crimes'}. Cached per district object."""
    key = id(district)
    if key in _PLAN_CACHE:
        return _PLAN_CACHE[key]
    import generate_city as gen  # noqa: PLC0415 - generate_city imports this lazily in turn
    import create_challenges as cc  # noqa: PLC0415
    world = cc.planning_world(district)
    blockers = _blockers(district, gen, cc)
    taken = set(world.taken)
    for plan in cc.plan_challenges(district):
        if plan.get("osm"):
            taken.add(plan["osm"])
        taken |= set(plan.get("roofs", []))
    street = _street_spots(world, gen, cc, blockers)
    roofs = _roof_spots(world, gen, cc, blockers + [(s["at"][0], s["at"][1], SPOT_APART) for s in street], taken)
    spots = street + roofs
    for index, spot in enumerate(spots):
        spot["index"] = index
        spot["crimes"] = list(ROOF_CRIMES if spot["rooftop"] else STREET_CRIMES)
    _PLAN_CACHE[key] = spots
    return spots


def describe(spot):
    extra = ""
    if spot["escape"] is not None:
        extra = ", escape {0:.0f} m to ({1:.0f}, {2:.0f})".format(_dist2(spot["at"], spot["escape"]) / 100.0, spot["escape"][0], spot["escape"][1])
    return "spot {0} {1} on osm {2} at ({3:.0f}, {4:.0f}, {5:.0f}) yaw {6:.0f}: {7}{8}".format(
        spot["index"], spot["kind"], spot["osm"], spot["at"][0], spot["at"][1], spot["z"], spot["yaw"], "/".join(spot["crimes"]), extra)


# --------------------------------------------------------------------------------------------------
# the victim
# --------------------------------------------------------------------------------------------------


def ensure_civilian_material():
    c.ensure_directory(CIVILIAN_MATERIAL_PATH)
    return m.ensure_material(M_CIVILIAN, m._build_flat(CIVILIAN_GREY, 0.75), skeletal=True)


def _mesh_component(cdo):
    try:
        return cdo.get_editor_property("mesh")
    except Exception:  # noqa: BLE001
        return None


def _configure_civilian_mesh(bp, material):
    cdo = c.blueprint_cdo(bp)
    component = _mesh_component(cdo) if cdo is not None else None
    mesh_asset = c.load_or_none(MANNEQUIN_MESH_PATH)
    if component is None or mesh_asset is None:
        c.log("skipped", CIVILIAN_BP + ".Mesh", "no mesh component or no SK_Mannequin")
        return False
    changed = []
    if component.get_editor_property("skeletal_mesh_asset") != mesh_asset:
        component.set_skeletal_mesh_asset(mesh_asset)
        changed.append("skeletal_mesh_asset")
    for prop, value in (("relative_location", MESH_LOCATION), ("relative_rotation", MESH_ROTATION),
                        ("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)):
        if component.get_editor_property(prop) != value and c.set_props(component, [(prop, value)], CIVILIAN_BP + ".Mesh"):
            changed.append(prop)
    for slot in (0, 1):
        try:
            if component.get_material(slot) != material:
                component.set_material(slot, material)
                changed.append("material {0}".format(slot))
        except Exception as exc:  # noqa: BLE001
            c.log_error(CIVILIAN_BP + ".Mesh material", exc)
    if changed:
        c.log("updated", CIVILIAN_BP + ".Mesh", ", ".join(changed))
    return bool(changed)


def ensure_civilian():
    """BP_Civilian: ACivilian on the old mannequin in M_Civilian, idle and run on the single-node slot."""
    parent = c.find_class("Civilian", "/Script/Hawkeye.Civilian")
    if parent is None:
        c.log("FAILED", c.asset_path(AI_PATH, CIVILIAN_BP), "ACivilian not exposed; build the module")
        return None
    bp, _created = cb.make_blueprint(CIVILIAN_BP, AI_PATH, parent, ("BlueprintFactory",))
    if bp is None:
        return None
    material = ensure_civilian_material()
    changed = bool(cb.apply_defaults(bp, CIVILIAN_BP, AI_PATH, [
        ("idle_anim", c.load_or_none(MANNEQUIN_IDLE_PATH)),
        ("run_anim", c.load_or_none(MANNEQUIN_RUN_PATH)),
    ]))
    if _configure_civilian_mesh(bp, material):
        changed = True
    if changed:
        c.compile_blueprint(bp)
        c.save(bp)
    else:
        c.log("exists", c.asset_path(AI_PATH, CIVILIAN_BP), "mannequin, M_Civilian, idle and run set")
    return c.load_generated_class(AI_PATH, CIVILIAN_BP)


# --------------------------------------------------------------------------------------------------
# the definitions
# --------------------------------------------------------------------------------------------------


def _thug_class(name):
    return c.load_generated_class(BOSS_PATH if name == "BP_Archer" else AI_PATH, name)


def _roster(entries):
    out = []
    for weapon, count, blueprint in entries:
        entry = unreal.CrimeRosterEntry()
        entry.set_editor_property("weapon", _enum("ThugWeapon", weapon))
        entry.set_editor_property("count", count)
        entry.set_editor_property("thug_class", _thug_class(blueprint))
        out.append(entry)
    return out


def _roster_key(entries):
    return [(str(e.get_editor_property("weapon")), int(e.get_editor_property("count")), _path(e.get_editor_property("thug_class")))
            for e in entries or []]


def _fields(crime, civilian_class):
    cid, ctype, roster, radius, time_to_fail, alert = crime
    return [
        ("id", unreal.Name(cid)),
        ("type", _enum("CrimeType", ctype)),
        ("name", unreal.Text("[Crime: {0}]".format(cid))),
        ("roster", _roster(roster)),
        ("radius", radius),
        ("time_to_fail_seconds", time_to_fail),
        ("victim_hits_to_fail", VICTIM_HITS),
        ("victim_class", civilian_class if ctype == "Mugging" else None),
        ("escape_distance", ESCAPE_DISTANCE),
        ("alert_on_start", alert),
        ("reward", _enum("CrimeReward", "ArrowRefill")),
        ("reward_arrows", REWARD_ARROWS),
    ]


def _same(prop, current, wanted):
    if prop == "roster":
        return _roster_key(current) == _roster_key(wanted)
    if prop == "victim_class":
        return _path(current) == _path(wanted)
    if isinstance(wanted, float):
        return abs(float(current) - wanted) < 1e-3
    if isinstance(wanted, unreal.Text):
        return str(current) == str(wanted)
    return current == wanted


def ensure_crime(crime, civilian_class):
    cls = c.find_class("CrimeDefinition", "/Script/Hawkeye.CrimeDefinition")
    if cls is None:
        c.log("FAILED", CRIME_PATH, "UCrimeDefinition not exposed to Python; build the module")
        return None
    name = ASSET_PREFIX + crime[0]
    full = c.asset_path(CRIME_PATH, name)
    factory = c.new_factory("DataAssetFactory")
    c.set_props(factory, [("data_asset_class", cls)], "DataAssetFactory")
    asset, created = c.create_asset(name, CRIME_PATH, cls, factory, quiet=True)
    if asset is None:
        return None
    changed = []
    for prop, value in _fields(crime, civilian_class):
        try:
            current = asset.get_editor_property(prop)
        except Exception as exc:  # noqa: BLE001
            c.log_error("{0}.{1}".format(full, prop), exc)
            continue
        if not _same(prop, current, value):
            asset.set_editor_property(prop, value)
            changed.append(prop)
    if created or changed:
        c.save(asset)
    thugs = sum(n for _w, n, _b in crime[2])
    c.log("created" if created else ("updated" if changed else "exists"), full,
          "{0} thugs{1}".format(thugs, " (" + ", ".join(changed) + ")" if changed and not created else ""))
    return asset


def crime_definitions():
    """{id: UCrimeDefinition} for every DA_Crime_ asset, by id."""
    out = {}
    if not unreal.EditorAssetLibrary.does_directory_exist(CRIME_PATH):
        return out
    for path in sorted(unreal.EditorAssetLibrary.list_assets(CRIME_PATH, recursive=False)):
        package = path.split(".")[0]
        if not package.split("/")[-1].startswith(ASSET_PREFIX):
            continue
        asset = c.load_or_none(package)
        if asset is not None:
            out[str(asset.get_editor_property("id"))] = asset
    return out


def run():
    c.ensure_directory(CRIME_PATH)
    civilian = ensure_civilian()
    assets = [ensure_crime(crime, civilian) for crime in CRIMES]
    return [a for a in assets if a is not None]


if __name__ == "__main__":
    run()
    c.print_summary("crimes")
