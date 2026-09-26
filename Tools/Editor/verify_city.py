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
* the five tallest and five shortest buildings with their OSM ids and streets, to eyeball

    UnrealEditor-Cmd.exe Castle.uproject -run=pythonscript ^
        -script="Tools\\Editor\\verify_city.py" -unattended -nullrhi -nosplash -nop4 -stdout
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402
import generate_city as gen  # noqa: E402

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
    check(sorted(mission_ids) == sorted(gen.OBJECTIVE_IDS), "DA_CH01_Rooftops objectives are "
          + ", ".join(gen.OBJECTIVE_IDS), ", ".join(mission_ids))
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

    prison = [a.get_actor_label() for a in all_actors
              if any(w in gen.actor_class_name(a) for w in gen.PRISON_CLASS_WORDS)]
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
