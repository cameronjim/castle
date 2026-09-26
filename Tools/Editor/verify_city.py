"""Check L_District_EastVillage against the OSM records it was generated from. Read-only.

Prints one line per check and a final ``[Castle] verify_city PASS`` or ``FAIL``:

* one City_Bldg_<id> actor per building record, and no strays
* every building actor has a static mesh with collision (complex as simple) and M_Greybox
* every building mesh's top is the record height within 1 cm (plus the parapet if it has one)
* a PlayerStart, a NavMeshBoundsVolume, a directional light, a sky light, and no light with
  Static mobility
* handedness: the streets come out in the real order (1st Ave west of Ave A west of Ave B
  west of Ave C, East 6th south of East 11th), i.e. the map is not mirrored
* the BP_GameMode_EastVillage override starting DA_CH01_Rooftops, one City_Obj_* volume per
  chapter-1 objective sitting above its roof, the street lamps (light, pole, head) matching the
  generator, and no prison-build actors (thugs, keycards, doors, pickups)
* the grapple anchors match the generator, and every anchor's landing point is on a roof: a
  trace from just above it down 50 cm hits a City_Bldg mesh
* one City_Ledge_ BP_TraversableBlock per roof edge the generator considers, hidden and
  blocking only the Traversable channel, its Ledge_1 spline ending on the parapet's outer
  corners within 5 cm; the parkour test blocks and park walls at their heights
* chapter 1's fight: four City_Thug_ (one gunner, two bats, one fists), the RoofPair on the
  cross_block roof, two City_Patrol_ points 40 m apart, City_ThugGroup_clear_roof, and every
  thug's feet on the navmesh (the navmesh is built in the editor world first, not saved)
* the five tallest and five shortest buildings with their OSM ids and streets, to eyeball

    UnrealEditor-Cmd.exe Castle.uproject -run=pythonscript ^
        -script="Tools\\Editor\\verify_city.py" -unattended -nullrhi -nosplash -nop4 -stdout
"""

import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402
import generate_city as gen  # noqa: E402
import _geo as geo  # noqa: E402

HEIGHT_TOLERANCE_CM = 1.0

_failures = []


def check(ok, what, detail=""):
    line = "[Castle] {0}  {1}{2}".format("PASS" if ok else "FAIL", what, "  (" + detail + ")" if detail else "")
    if ok:
        unreal.log(line)
    else:
        unreal.log_error(line)
        _failures.append(what)
    return ok


def tag_value(actor, prefix):
    for t in actor.get_editor_property("tags"):
        s = str(t)
        if s.startswith(prefix):
            return s[len(prefix):]
    return None


def mesh_top_cm(static_mesh):
    box = static_mesh.get_bounding_box()
    return box.max.z, box.min.z


def has_collision(component, static_mesh):
    try:
        if component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
            return False, "component collision disabled"
        body = static_mesh.get_editor_property("body_setup")
        if body is None:
            return False, "no body setup"
        flag = body.get_editor_property("collision_trace_flag")
        if flag != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
            return False, "trace flag {0}".format(flag)
        return True, ""
    except Exception as exc:  # noqa: BLE001
        return False, "{0}: {1}".format(type(exc).__name__, exc)


LANDING_PROBE_UP = 20.0     # cm above the landing point the probe starts
LANDING_PROBE_DOWN = 50.0   # cm below it the probe must hit a roof by


def _of_type(result, cls):
    """The first value of type cls in a Geometry Script return (a value or a tuple of out-params)."""
    for item in (result if isinstance(result, tuple) else (result,)):
        if isinstance(item, cls):
            return item
    return None


class MeshProbe(object):
    """Ray casts against building meshes with Geometry Script's BVH. The editor world has no
    physics scene in a commandlet, so a collision trace from Python always misses; this casts
    the same ray against the same triangles the complex-as-simple collision is built from."""

    def __init__(self):
        self.cache = {}

    def _bvh(self, static_mesh):
        key = static_mesh.get_path_name()
        if key not in self.cache:
            mesh = unreal.DynamicMesh()
            unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(
                static_mesh, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                unreal.GeometryScriptMeshReadLOD(), True)
            bvh = _of_type(unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(mesh),
                           unreal.GeometryScriptDynamicMeshBVH)
            self.cache[key] = (mesh, bvh)
        return self.cache[key]

    def hits_down(self, actor, point):
        """True when a ray from LANDING_PROBE_UP above point hits actor's mesh within
        LANDING_PROBE_DOWN below point."""
        static_mesh = actor.get_editor_property("static_mesh_component").get_editor_property("static_mesh")
        if static_mesh is None:
            return False
        mesh, bvh = self._bvh(static_mesh)
        if bvh is None:
            return False
        origin = point - actor.get_actor_location() + unreal.Vector(0.0, 0.0, LANDING_PROBE_UP)
        hit = _of_type(unreal.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(
            mesh, bvh, origin, unreal.Vector(0.0, 0.0, -1.0), unreal.GeometryScriptSpatialQueryOptions()),
            unreal.GeometryScriptRayHitResult)
        return bool(hit is not None and hit.hit and hit.ray_parameter <= LANDING_PROBE_UP + LANDING_PROBE_DOWN)


def check_anchors(district, actors):
    anchors = {label: a for label, a in actors.items() if label.startswith(gen.ANCHOR_PREFIX)}
    expected = gen.anchor_spots(district)
    labels_ok = set(anchors) == {gen.ANCHOR_PREFIX + str(i) for i in range(len(expected))}
    check(labels_ok, "grapple anchors match the generator",
          "{0} anchors, {1} expected, on {2} roofs".format(len(anchors), len(expected), len({s[6] for s in expected})))

    buildings = []
    for label, actor in actors.items():
        if label.startswith(gen.BUILDING_PREFIX):
            origin, extent = actor.get_actor_bounds(False)
            buildings.append((label, actor, origin, extent))

    probe = MeshProbe()
    off_roof = []
    heights = []
    for label, anchor in sorted(anchors.items()):
        landing = anchor.get_landing_point()
        point = landing.get_world_location() if landing is not None else anchor.get_actor_location()
        found = None
        for b_label, actor, origin, extent in buildings:
            if abs(point.x - origin.x) <= extent.x and abs(point.y - origin.y) <= extent.y \
                    and probe.hits_down(actor, point):
                found = b_label
                break
        if found is None:
            off_roof.append(label)
        heights.append(anchor.get_actor_location().z - point.z)
    check(anchors and not off_roof, "every anchor's landing point is on a City_Bldg roof (50 cm probe)",
          "{0} of {1} off a roof{2}".format(len(off_roof), len(anchors),
                                            ": " + ", ".join(off_roof[:5]) if off_roof else ""))
    if heights:
        unreal.log("[Castle] info  anchors sit {0:.0f} to {1:.0f} cm above their landing points".format(
            min(heights), max(heights)))


LEDGE_TOLERANCE_CM = 5.0



def closest_point_on_ring(pt, ring):
    return gen.closest_point_on_polyline(pt, list(ring) + [ring[0]])[0]


def check_ledges(district, actors):
    """Every roof edge the generator considers has its City_Ledge_ block, and each block's Ledge_1
    spline runs corner to corner along the parapet top: both ends within LEDGE_TOLERANCE_CM of the
    expected corners, of the footprint outline (horizontally) and of the parapet top (vertically)."""
    expected = gen.ledge_spots(district)
    ledges = {label: a for label, a in actors.items() if label.startswith(gen.LEDGE_PREFIX)}
    labels_ok = set(ledges) == {s[0] for s in expected}
    check(labels_ok, "traversable ledges match the generator's roof edges",
          "{0} ledges, {1} roof edges of {2} cm or more on {3} buildings".format(
              len(ledges), len(expected), int(gen.LEDGE_MIN_EDGE), len({s[1] for s in expected})))

    rings = {}
    for rec in district.buildings:
        rings[rec["id"]] = geo.clean_ring(district.ring_cm(rec["outer"]), min_edge=5.0, collinear_tol=2.0)
    cls = gen.traversable_class()
    channel = gen.traversable_channel()
    wrong_class, no_spline, off_corner, off_edge, visible, bad_collision = [], [], [], [], [], []
    worst = 0.0
    for label, osm, _origin, _yaw, _scale, corner_a, corner_b, _spec in expected:
        actor = ledges.get(label)
        if actor is None:
            continue
        if cls is not None and actor.get_class() != cls:
            wrong_class.append(label)
            continue
        spline = next((comp for comp in actor.get_components_by_class(unreal.SplineComponent)
                       if comp.get_name() == gen.LEDGE_SPLINE), None)
        if spline is None or spline.get_number_of_spline_points() < 2:
            no_spline.append(label)
            continue
        ends = [spline.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD)
                for i in (0, spline.get_number_of_spline_points() - 1)]
        for end, want in zip(ends, (corner_a, corner_b)):
            error = max(abs(end.x - want[0]), abs(end.y - want[1]), abs(end.z - want[2]))
            worst = max(worst, error)
            if error > LEDGE_TOLERANCE_CM:
                off_corner.append("{0} {1:.1f} cm".format(label, error))
            if closest_point_on_ring((end.x, end.y), rings[osm]) > LEDGE_TOLERANCE_CM:
                off_edge.append(label)
        for mesh in actor.get_components_by_class(unreal.StaticMeshComponent):
            if mesh.get_editor_property("visible"):
                visible.append(label)
            if channel is not None and (
                    mesh.get_collision_response_to_channel(channel) != unreal.CollisionResponseType.ECR_BLOCK
                    or mesh.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)
                    != unreal.CollisionResponseType.ECR_IGNORE):
                bad_collision.append(label)
    check(not wrong_class, "every ledge is a BP_TraversableBlock (a LevelBlock_Traversable)", ", ".join(wrong_class[:5]))
    check(not no_spline, "every ledge has its " + gen.LEDGE_SPLINE + " spline", ", ".join(no_spline[:5]))
    check(not off_corner, "ledge spline ends on the parapet corners within {0:.0f} cm (worst {1:.2f} cm)".format(
        LEDGE_TOLERANCE_CM, worst), "; ".join(off_corner[:5]))
    check(not off_edge, "ledge spline ends on the roof outline within {0:.0f} cm".format(LEDGE_TOLERANCE_CM),
          ", ".join(off_edge[:5]))
    check(not visible, "ledge blocks are hidden", ", ".join(visible[:5]))
    check(not bad_collision, "ledge blocks block only the Traversable channel (not Pawn)", ", ".join(bad_collision[:5]))

    blocks = gen.test_block_spots(district, actors)
    missing = [s[0] for s in blocks if s[0] not in actors]
    check(not missing, "parkour test blocks present ({0})".format(", ".join(
        "{0} {1:.0f} cm".format(s[0], s[4][2] * 100.0) for s in blocks)), ", ".join(missing))
    wrong_height = []
    for label, _tag, origin, _yaw, scale in blocks:
        actor = actors.get(label)
        if actor is None:
            continue
        box_origin, extent = actor.get_actor_bounds(True)
        top = box_origin.z + extent.z
        if abs(top - (origin[2] + scale[2] * 100.0)) > 1.0:
            wrong_height.append("{0} top {1:.1f}".format(label, top))
    check(not wrong_height, "parkour test blocks stand their heights above the ground under them, within 1 cm",
          "; ".join(wrong_height))


NAV_QUERY_EXTENT = unreal.Vector(50.0, 50.0, 150.0)   # cm; how far off a foot may be from the navmesh


def build_navigation():
    """Builds the district's navmesh in this editor world (blocking), as the game mode does at
    BeginPlay, through UCastleNavigationLibrary (the editor's async-load lock would refuse a
    plain RebuildNavigation here). Returns (world, built). Nothing is saved."""
    world = c.editor_world()
    lib = getattr(unreal, "CastleNavigationLibrary", None)
    if world is None or lib is None:
        return world, False
    return world, bool(lib.build_navigation_now(world))


def weapon_name(value):
    """ThugWeapon.FISTS -> FISTS, whatever the enum's repr looks like."""
    name = getattr(value, "name", None)
    return str(name if name else value).split(".")[-1].split(":")[0].strip("<> ").upper()


def check_thugs(district, actors, records):
    """Four City_Thug_<n> with one gunner, two patrol points 40 m apart, the roof pair on the
    cross_block roof, the clear_roof group, and every thug's feet on the navmesh."""
    thugs = {l: a for l, a in actors.items() if l.startswith(gen.THUG_PREFIX)}
    points = {l: a for l, a in actors.items() if l.startswith(gen.PATROL_PREFIX)}
    wanted, wanted_points, roof_rec = gen.thug_placements(district)
    check(sorted(thugs) == sorted(t[0] for t in wanted), "four chapter-1 thugs placed",
          "{0} thugs: {1}".format(len(thugs), ", ".join(sorted(thugs))))
    check(len(points) == 2 and all(isinstance(a, unreal.TargetPoint) for a in points.values()),
          "two patrol points (ATargetPoint)", ", ".join(sorted(points)))
    if len(points) == 2:
        a, b = [points[l].get_actor_location() for l in sorted(points)]
        gap = math.hypot(a.x - b.x, a.y - b.y)
        check(abs(gap - gen.PATROL_LENGTH) <= 1.0, "patrol points 40 m apart", "{0:.1f} cm".format(gap))

    weapons = {}
    detail = []
    for label, actor in sorted(thugs.items()):
        weapon = weapon_name(actor.get_editor_property("weapon"))
        weapons[weapon] = weapons.get(weapon, 0) + 1
        tags = [str(t) for t in actor.get_editor_property("tags")]
        patrol = [p.get_actor_label() for p in actor.get_editor_property("patrol_points") if p]
        loc = actor.get_actor_location()
        detail.append("{0} {1} {2} ({3:.0f}, {4:.0f}, {5:.0f}){6}".format(
            label, weapon.lower(), "/".join(t for t in tags if t.endswith("Pair")), loc.x, loc.y, loc.z,
            " patrol " + ">".join(patrol) if patrol else ""))
    unreal.log("[Castle] info  thugs: " + "; ".join(detail))
    check(weapons.get("PISTOL", 0) == 1 and weapons.get("BAT", 0) == 2 and weapons.get("FISTS", 0) == 1,
          "one gunner, two bats, one fists", str(weapons))

    roof_pair = [a for a in thugs.values() if "RoofPair" in [str(t) for t in a.get_editor_property("tags")]]
    on_roof = roof_rec is not None and len(roof_pair) == 2 and all(
        abs(a.get_actor_location().z - (roof_rec["height_m"] * 100.0 + gen.THUG_HALF_HEIGHT + 2.0)) <= 1.0
        and geo.point_in_polygon((a.get_actor_location().x, a.get_actor_location().y), roof_rec["ring"])
        for a in roof_pair)
    check(on_roof, "the RoofPair stands on the cross_block roof",
          "osm {0}".format(roof_rec["id"] if roof_rec else "none"))
    group = actors.get(gen.THUG_GROUP_LABEL)
    check(group is not None and str(group.get_editor_property("objective_id")) == "clear_roof"
          and str(group.get_editor_property("group_tag")) == "RoofPair",
          gen.THUG_GROUP_LABEL + " completes clear_roof for the RoofPair")

    world, built = build_navigation()
    check(built, "navmesh builds in the editor world (UCastleNavigationLibrary)")
    if not built:
        return
    off = []
    for label, actor in sorted(thugs.items()):
        feet = actor.get_actor_location() - unreal.Vector(0.0, 0.0, gen.THUG_HALF_HEIGHT)
        result = unreal.CastleNavigationLibrary.project_to_navigation(world, feet, NAV_QUERY_EXTENT)
        # Python hands a bool-returning function with an out parameter back either as
        # (ok, point) or as the point itself / None; take both.
        if isinstance(result, tuple):
            ok, point = bool(result[0]), result[1]
        else:
            ok, point = result is not None, result
        if not ok:
            off.append(label)
        else:
            unreal.log("[Castle] info  {0} feet {1:.0f} -> navmesh ({2:.0f}, {3:.0f}, {4:.0f})".format(
                label, feet.z, point.x, point.y, point.z))
    check(not off, "every thug starts on the navmesh", ", ".join(off))


def run():
    if not gen.data_available():
        check(False, "OSM data present", "run Tools\\fetch-osm.ps1")
        return False
    if not c.exists(gen.MAP_PATH):
        check(False, "map exists", gen.MAP_PATH)
        return False
    c.level_editor_subsystem().load_level(gen.MAP_PATH)

    district = gen.District()
    records = {b["id"]: b for b in district.buildings}
    actors = gen.actors_by_label()
    buildings = {label[len(gen.BUILDING_PREFIX):]: a for label, a in actors.items()
                 if label.startswith(gen.BUILDING_PREFIX)}

    missing = sorted(set(records) - set(buildings))
    strays = sorted(set(buildings) - set(records))
    check(len(buildings) == len(records) and not missing and not strays,
          "building actors match records",
          "{0} actors, {1} records, {2} missing, {3} stray{4}".format(
              len(buildings), len(records), len(missing), len(strays),
              ": " + ", ".join((missing + strays)[:5]) if missing or strays else ""))

    greybox = c.load_or_none(gen.KIT_MATERIALS + "/M_Greybox")
    no_mesh, no_collision, wrong_height, wrong_material = [], [], [], []
    rows = []
    for rid, actor in sorted(buildings.items()):
        rec = records.get(rid)
        if rec is None:
            continue
        comp = actor.get_editor_property("static_mesh_component")
        sm = comp.get_editor_property("static_mesh")
        if sm is None:
            no_mesh.append(rid)
            continue
        ok, why = has_collision(comp, sm)
        if not ok:
            no_collision.append("{0} ({1})".format(rid, why))
        overrides = comp.get_editor_property("override_materials")
        if greybox is not None and (len(overrides) < 1 or overrides[0] != greybox):
            wrong_material.append(rid)
        top, bottom = mesh_top_cm(sm)
        parapet = tag_value(actor, "parapet:") == "1"
        expected = rec["height_m"] * 100.0 + (gen.PARAPET_HEIGHT if parapet else 0.0)
        z0 = actor.get_actor_location().z
        if abs(top - expected) > HEIGHT_TOLERANCE_CM or abs(bottom) > HEIGHT_TOLERANCE_CM or abs(z0) > 0.01:
            wrong_height.append("{0} top {1:.1f} want {2:.1f} base {3:.1f}".format(rid, top, expected, bottom + z0))
        rows.append((rec["height_m"], rid, rec.get("tags", {}).get("addr:street") or "?",
                     rec.get("tags", {}).get("addr:housenumber") or ""))

    check(not no_mesh, "every building has a static mesh", ", ".join(no_mesh[:5]))
    check(not no_collision, "every building mesh has complex-as-simple collision", ", ".join(no_collision[:5]))
    check(not wrong_height, "building heights match records within 1 cm", "; ".join(wrong_height[:5]))
    check(not wrong_material, "every building uses M_Greybox", ", ".join(wrong_material[:5]))

    # Streets and ground
    roads = [a for label, a in actors.items() if label.startswith(gen.ROAD_PREFIX)]
    walks = [a for label, a in actors.items() if label.startswith(gen.SIDEWALK_PREFIX)]
    parks = [a for label, a in actors.items() if label.startswith(gen.PARK_PREFIX)]
    check(gen.GROUND_LABEL in actors, "ground slab present")
    check(len(roads) == len(district.roads), "road actors match records",
          "{0} actors, {1} records".format(len(roads), len(district.roads)))
    check(len(walks) >= len(district.roads) * 0.9, "sidewalk actors present", str(len(walks)))
    check(len(parks) == len(district.parks), "park actors match records", str(len(parks)))

    # Scene
    all_actors = c.all_level_actors()
    nav_class = c.find_class("NavMeshBoundsVolume", "/Script/NavigationSystem.NavMeshBoundsVolume")
    check(any(isinstance(a, unreal.PlayerStart) for a in all_actors), "PlayerStart present")
    check(nav_class is not None and any(isinstance(a, nav_class) for a in all_actors), "NavMeshBoundsVolume present")
    check(any(isinstance(a, unreal.DirectionalLight) for a in all_actors), "directional light present")
    check(any(isinstance(a, unreal.SkyLight) for a in all_actors), "sky light present")
    static_lights = []
    for a in all_actors:
        if isinstance(a, (unreal.Light, unreal.SkyLight)):
            root = a.get_editor_property("root_component")
            if root is not None and root.get_editor_property("mobility") == unreal.ComponentMobility.STATIC:
                static_lights.append(a.get_actor_label())
    check(not static_lights, "no static-mobility lights", ", ".join(static_lights))

    ws = c.world_settings()
    gm = ws.get_editor_property("default_game_mode") if ws else None
    check(gm is not None and gen.DISTRICT_GAME_MODE in c.class_name(gm),
          "GameMode override is " + gen.DISTRICT_GAME_MODE, c.class_name(gm))
    mission = c.load_or_none(gen.MISSION_ASSET)
    starting = None
    if gm is not None:
        try:
            starting = unreal.get_default_object(gm).get_editor_property("starting_mission")
        except Exception:  # noqa: BLE001
            starting = None
    check(mission is not None and starting == mission, "its StartingMission is DA_CH01_Rooftops",
          c.safe_name(starting) if starting is not None else "None")

    # Chapter 1 objective volumes: one per objective id, each above a roof.
    volume_class = c.find_class("ObjectiveTriggerVolume", "/Script/Castle.ObjectiveTriggerVolume")
    volumes = [a for a in all_actors if volume_class is not None and isinstance(a, volume_class)]
    by_id = {}
    for v in volumes:
        by_id.setdefault(str(v.get_editor_property("objective_id")), []).append(v)
    mission_ids = []
    if mission is not None:
        for obj in mission.get_editor_property("objectives") or []:
            mission_ids.append(str(obj.get_editor_property("objective_id")))
    check(mission_ids == list(gen.MISSION_OBJECTIVE_IDS), "DA_CH01_Rooftops objectives are "
          + ", ".join(gen.MISSION_OBJECTIVE_IDS), ", ".join(mission_ids))
    detail = []
    ok = len(volumes) == len(gen.OBJECTIVE_IDS)
    for oid in gen.OBJECTIVE_IDS:
        found = by_id.get(oid, [])
        if len(found) != 1:
            ok = False
            detail.append("{0} x{1}".format(oid, len(found)))
            continue
        v = found[0]
        osm = tag_value(v, "osm:")
        rec = records.get(osm)
        z = v.get_actor_location().z
        above = rec is not None and abs(z - (rec["height_m"] * 100.0 + gen.OBJECTIVE_ABOVE_ROOF
                                             + gen.OBJECTIVE_HALF_HEIGHT)) <= 1.0
        ok = ok and above and v.get_actor_label() == gen.OBJECTIVE_PREFIX + oid
        detail.append("{0} on {1}{2}".format(oid, osm, "" if above else " NOT above its roof"))
    check(ok, "three objective volumes with the chapter-1 ObjectiveIds", "; ".join(detail))

    # Street lamps: each light has its pole and head; shadows only near the park.
    lights = {l[len(gen.LAMP_PREFIX):] for l in actors if l.startswith(gen.LAMP_PREFIX)}
    poles = {l[len(gen.LAMP_POLE_PREFIX):] for l in actors if l.startswith(gen.LAMP_POLE_PREFIX)}
    heads = {l[len(gen.LAMP_HEAD_PREFIX):] for l in actors if l.startswith(gen.LAMP_HEAD_PREFIX)}
    shadowed = sum(1 for l, a in actors.items() if l.startswith(gen.LAMP_PREFIX)
                   and a.get_editor_property("spot_light_component").get_editor_property("cast_shadows"))
    expected = len(gen.lamp_spots(district))
    check(len(lights) == expected and lights == poles == heads, "street lamps match the generator",
          "{0} lights, {1} poles, {2} heads, {3} expected, {4} casting shadows".format(
              len(lights), len(poles), len(heads), expected, shadowed))

    check_anchors(district, actors)
    check_ledges(district, actors)
    check_thugs(district, actors, records)

    prison = [a.get_actor_label() for a in all_actors
              if not gen.is_chapter_actor(a.get_actor_label())
              and any(w in gen.actor_class_name(a) for w in gen.PRISON_CLASS_WORDS)]
    check(not prison, "no thug, keycard, door or pickup actors", ", ".join(prison[:5]))
    try:
        wp = ws.get_world_partition() if ws and hasattr(ws, "get_world_partition") else None
    except Exception:  # noqa: BLE001
        wp = None
    unreal.log("[Castle] info  World Partition: {0}".format("ON" if wp else "off"))

    # Handedness: mean X of each avenue and mean Y of the two boundary streets, in Unreal cm.
    def mean_xy(name):
        pts = [district.cm(p) for r in district.roads if r.get("name") == name
               for piece in r["pieces"] for p in piece]
        if not pts:
            return None
        return sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts)

    avenues = [mean_xy(n) for n in ("1st Avenue", "Avenue A", "Avenue B", "Avenue C")]
    e6, e11 = mean_xy("East 6th Street"), mean_xy("East 11th Street")
    if all(avenues) and e6 and e11:
        xs = [a[0] for a in avenues]
        order_ok = xs == sorted(xs) and e6[1] > e11[1]
        check(order_ok, "handedness: avenues run 1st, A, B, C west to east and E 6th is south of E 11th",
              "avenue X {0}; E6 Y {1:.0f} vs E11 Y {2:.0f}".format(
                  ", ".join("{0:.0f}".format(x) for x in xs), e6[1], e11[1]))
    else:
        check(False, "handedness streets found")

    rows.sort()
    unreal.log("[Castle] info  tallest: " + "; ".join(
        "{0} {1:.1f} m ({2})".format(r[1], r[0], (r[3] + " " + r[2]).strip() if r[2] != "?" else "no address") for r in reversed(rows[-5:])))
    unreal.log("[Castle] info  shortest: " + "; ".join(
        "{0} {1:.1f} m ({2})".format(r[1], r[0], (r[3] + " " + r[2]).strip() if r[2] != "?" else "no address") for r in rows[:5]))

    if _failures:
        unreal.log_error("[Castle] verify_city FAIL ({0} check(s): {1})".format(len(_failures), "; ".join(_failures)))
        return False
    unreal.log("[Castle] verify_city PASS")
    return True


if __name__ == "__main__":
    run()
