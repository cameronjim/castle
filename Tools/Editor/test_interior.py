"""Unit tests for _interior.py: layout parsing, room adjacency, door and window placement, the owner grid's
walls and slabs, stair geometry, and the enemies and the keycards they carry. Plain unittest, no editor:

    Tools/test-interior.ps1
    (or) python Tools/Editor/test_interior.py
"""

import copy
import json
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _interior as it  # noqa: E402

SAMPLE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Interiors", "Sample.json")


def sample_raw():
    with open(SAMPLE, "r") as handle:
        return json.load(handle)


def two_rooms(**door):
    """A 600 x 400 room beside a 400 x 400 one, sharing the wall x = 600, with one door between them."""
    raw = {"name": "Two", "rooms": [
        {"id": "a", "rect": [0, 0, 600, 400]},
        {"id": "b", "rect": [600, 0, 1000, 400]}],
        "exits": [{"id": "front", "room": "a", "side": "N", "entrance": True}],
        "doors": [dict({"id": "ab", "rooms": ["a", "b"]}, **door)]}
    return raw


class ParseTest(unittest.TestCase):
    def test_sample_parses(self):
        layout = it.parse(SAMPLE)
        self.assertEqual(layout.name, "Sample")
        self.assertEqual(layout.map_path, "/Game/Maps/L_Int_Sample")
        self.assertEqual(len(layout.rooms), 6)
        self.assertEqual(layout.bands, 2)

    def test_parses_a_json_string_and_a_dict(self):
        text = json.dumps(two_rooms())
        self.assertEqual(it.parse(text).name, "Two")
        self.assertEqual(it.parse(json.loads(text)).name, "Two")

    def test_missing_name_is_an_error(self):
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse({"rooms": []})
        self.assertIn("missing name", str(ctx.exception))

    def test_a_room_without_a_rect_is_an_error(self):
        with self.assertRaises(it.LayoutError):
            it.parse({"name": "Bad", "rooms": [{"id": "a"}]})

    def test_every_broken_rule_is_listed(self):
        raw = two_rooms()
        raw["rooms"].append({"id": "c", "rect": [100, 100, 300, 300]})
        raw["rooms"].append({"id": "d", "rect": [0, 0, 100, 100]})
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertGreaterEqual(len(ctx.exception.errors), 2)

    def test_overlapping_rooms_on_one_floor(self):
        raw = two_rooms()
        raw["rooms"][1]["rect"] = [500, 0, 1000, 400]
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn("overlap", str(ctx.exception))

    def test_rooms_on_different_floors_may_stack(self):
        raw = two_rooms()
        raw["rooms"].append({"id": "up", "floor": 1, "rect": [0, 0, 600, 400]})
        self.assertEqual(it.parse(raw).bands, 2)

    def test_a_mezzanine_must_sit_inside_its_host(self):
        raw = sample_raw()
        gallery = [r for r in raw["rooms"] if r["id"] == "gallery"][0]
        gallery["rect"] = [700, 0, 1200, 1400]
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn("mezzanine", str(ctx.exception))

    def test_exactly_one_entrance(self):
        raw = two_rooms()
        raw["exits"][0]["entrance"] = False
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn("entrance", str(ctx.exception))

    def test_unknown_prop_tag(self):
        raw = two_rooms()
        raw["props"] = [{"room": "a", "tag": "piano", "at": [100, 100]}]
        with self.assertRaises(it.LayoutError):
            it.parse(raw)

    def test_patrol_point_outside_its_room(self):
        raw = two_rooms()
        raw["patrols"] = [{"id": "p", "points": [["a", 100, 100], ["a", 590, 100]]}]
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn("40 cm inside", str(ctx.exception))


class EnemyTest(unittest.TestCase):
    def locked(self, **enemy):
        raw = two_rooms(leaf="steel", locked=True, keycard="vault")
        raw["enemies"] = [dict({"id": "g", "type": "BOW", "room": "a", "at": [100, 100]}, **enemy)]
        return raw

    def test_sample_has_four_enemies_and_the_archer_carries_the_vault_card(self):
        layout = it.parse(sample_raw())
        enemies = {e["id"]: e for e in layout.enemies()}
        self.assertEqual(sorted(e["type"] for e in enemies.values()), ["BAT", "BOW", "FISTS", "PISTOL"])
        self.assertEqual(enemies["vault"]["keycards"], ["vault"])
        self.assertEqual(enemies["gallery"]["room"], "gallery")
        self.assertEqual(enemies["lobby"]["alert_group"], "front")
        self.assertEqual({e["alert_group"] for e in enemies.values() if e["id"] != "lobby"}, {"hall"})

    def test_a_patroller_with_no_start_begins_on_his_first_point(self):
        layout = it.parse(sample_raw())
        hall = next(e for e in layout.enemies() if e["id"] == "hall")
        (x, y, z), yaw = layout.enemy_start(hall)
        self.assertEqual((x, y, z), (1500.0, 300.0, 0.0))
        self.assertEqual([(p[0], p[1]) for p in layout.enemy_route(hall)], [(1500.0, 1100.0), (1500.0, 300.0)])
        self.assertAlmostEqual(yaw, 90.0)

    def test_a_start_is_walked_from_to_the_first_point(self):
        layout = it.parse(sample_raw())
        lobby = next(e for e in layout.enemies() if e["id"] == "lobby")
        (x, y, _z), _yaw = layout.enemy_start(lobby)
        self.assertEqual((x, y), (620.0, 300.0))
        self.assertEqual([(p[0], p[1]) for p in layout.enemy_route(lobby)], [(250.0, 480.0), (250.0, 900.0)])

    def test_points_on_an_upper_floor_and_their_facing(self):
        layout = it.parse(sample_raw())
        gunner = next(e for e in layout.enemies() if e["id"] == "gallery")
        (_x, _y, z), yaw = layout.enemy_start(gunner)
        self.assertEqual(z, it.FLOOR_HEIGHT)
        self.assertEqual(yaw, 0.0)
        self.assertEqual([p[3] for p in layout.enemy_route(gunner)], [0.0, 0.0])

    def test_a_post_has_no_route(self):
        layout = it.parse(self.locked())
        guard = layout.enemies()[0]
        self.assertEqual(layout.enemy_route(guard), [])

    def test_old_patrols_read_as_enemies(self):
        raw = two_rooms()
        raw["patrols"] = [{"id": "p", "weapon": "BAT", "points": [["a", 100, 100], ["a", 300, 100]]}]
        enemy = it.parse(raw).enemies()[0]
        self.assertEqual((enemy["type"], enemy["room"], enemy["at"]), ("BAT", "a", (100.0, 100.0)))
        self.assertEqual([(p[1], p[2]) for p in enemy["patrol"]], [(300.0, 100.0), (100.0, 100.0)])

    def assertLayoutError(self, raw, words):
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn(words, str(ctx.exception))

    def test_unknown_type(self):
        self.assertLayoutError(self.locked(type="ROCKET"), "type ROCKET")

    def test_start_inside_a_wall(self):
        self.assertLayoutError(self.locked(at=[590, 100]), "40 cm inside")

    def test_patrol_point_inside_a_wall(self):
        self.assertLayoutError(self.locked(patrol=[["a", 100, 100], ["b", 5, 100]]), "40 cm inside")

    def test_one_point_is_not_a_patrol(self):
        self.assertLayoutError(self.locked(patrol=[["a", 300, 100]]), "two points or more")

    def test_unknown_room(self):
        self.assertLayoutError(self.locked(room="cellar"), "unknown room cellar")

    def test_ids_are_unique(self):
        raw = self.locked()
        raw["enemies"].append(dict(raw["enemies"][0]))
        self.assertLayoutError(raw, "id used twice")

    def test_a_keycard_needs_a_real_carrier_and_a_lock(self):
        raw = self.locked()
        raw["keycards"] = [{"keycard": "vault", "carrier": "nobody"}]
        self.assertLayoutError(raw, "carrier nobody is not an enemy")
        raw["keycards"] = [{"keycard": "red", "carrier": "g"}]
        self.assertLayoutError(raw, "opens no locked door")
        raw["keycards"] = [{"keycard": "vault", "carrier": "g"}]
        self.assertEqual(it.parse(raw).enemies()[0]["keycards"], ["vault"])

    def test_alert_group_is_a_plain_name(self):
        self.assertLayoutError(self.locked(alert_group="the hall"), "alert group")


class AdjacencyTest(unittest.TestCase):
    def setUp(self):
        self.layout = it.parse(SAMPLE)

    def test_rooms_sharing_a_wall_are_adjacent(self):
        self.assertTrue(self.layout.adjacent("lobby", "office"))
        self.assertTrue(self.layout.adjacent("lobby", "hall", 0))
        self.assertTrue(self.layout.adjacent("stair", "gallery", 1))

    def test_rooms_apart_are_not(self):
        self.assertFalse(self.layout.adjacent("lobby", "vault"))
        self.assertFalse(self.layout.adjacent("office", "hall"))

    def test_adjacency_is_per_floor(self):
        self.assertFalse(self.layout.adjacent("stair", "gallery", 0))
        self.assertTrue(self.layout.adjacent("stair", "hall", 0))

    def test_a_mezzanine_meets_its_host_at_a_rail_not_a_wall(self):
        self.assertFalse(self.layout.adjacent("gallery", "hall"))
        rails = [b for b in self.layout.boundaries(1) if b.kind == "rail"]
        self.assertEqual(len(rails), 1)
        self.assertEqual((rails[0].axis, rails[0].coord, rails[0].lo, rails[0].hi), ("x", 1200.0, 0.0, 1400.0))

    def test_a_shared_wall_is_one_boundary(self):
        shared = [b for b in self.layout.boundaries(0) if b.rooms() == {"lobby", "hall"}]
        self.assertEqual(len(shared), 1)
        self.assertEqual((shared[0].coord, shared[0].lo, shared[0].hi), (800.0, 0.0, 600.0))
        self.assertEqual((shared[0].a, shared[0].b), ("lobby", "hall"))

    def test_outside_walls(self):
        north = [b for b in self.layout.boundaries(1) if b.kind == "exterior" and b.axis == "y" and b.coord == 0.0]
        self.assertEqual(sorted((b.lo, b.hi, b.b) for b in north), [(800.0, 1200.0, "gallery"), (1200.0, 2200.0, "hall")])
        # Nothing stands over the lobby on floor 1, so the gallery's west wall is an outside wall there.
        west = [b for b in self.layout.boundaries(1) if b.axis == "x" and b.coord == 800.0]
        self.assertIn(("exterior", 0.0, 600.0), [(b.kind, b.lo, b.hi) for b in west])
        self.assertIn(("wall", 600.0, 1220.0), [(b.kind, b.lo, b.hi) for b in west])

    def test_every_room_is_closed(self):
        """Every room edge is covered by boundaries on each of its floors (the grid leaves no hole)."""
        for room in self.layout.rooms:
            for band in room.bands:
                if room.overlooks:
                    continue
                for side in it.SIDES:
                    axis, coord, lo, hi = room.side_line(side)
                    # A mezzanine owns the host's edge where it runs along it.
                    own = {room.id} | {r.id for r in self.layout.rooms if r.overlooks == room.id and band in r.bands}
                    covered = sorted((b.lo, b.hi) for b in self.layout.boundaries(band)
                                     if b.axis == axis and b.coord == coord and b.rooms() & own)

                    cursor = lo
                    for b_lo, b_hi in covered:
                        self.assertLessEqual(b_lo, cursor + 0.01, "{0} {1} floor {2}".format(room.id, side, band))
                        cursor = max(cursor, b_hi)
                    # A double-height room's upper floor can open onto its own mezzanine; that side is a rail.
                    if cursor < hi - 0.01:
                        self.assertTrue(any(r.overlooks == room.id for r in self.layout.rooms), (room.id, side, band))


class DoorPlacementTest(unittest.TestCase):
    def test_default_door_goes_in_the_middle(self):
        (door, bd, at, width), = it.parse(two_rooms()).door_spots()
        self.assertEqual((bd.axis, bd.coord), ("x", 600.0))
        self.assertEqual(at, 200.0)
        self.assertEqual(width, it.DOOR_WIDTH)

    def test_door_at_a_coordinate(self):
        (_d, _bd, at, _w), = it.parse(two_rooms(at=100)).door_spots()
        self.assertEqual(at, 100.0)

    def test_door_too_near_a_corner(self):
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(two_rooms(at=60))
        self.assertIn("within 15 cm", str(ctx.exception))

    def test_door_between_rooms_that_do_not_touch(self):
        raw = two_rooms()
        raw["rooms"][1]["rect"] = [700, 0, 1000, 400]
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn("no wall between", str(ctx.exception))

    def test_door_on_the_wrong_floor(self):
        with self.assertRaises(it.LayoutError):
            it.parse(two_rooms(floor=1))

    def test_a_door_does_not_fit_a_short_wall(self):
        raw = two_rooms()
        raw["rooms"][1]["rect"] = [600, 0, 1000, 160]
        raw["rooms"].append({"id": "c", "rect": [600, 160, 1000, 400]})
        raw["doors"][0].update(width=150, leaf="none")
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn("does not fit", str(ctx.exception))

    def test_a_leaf_door_is_100_wide_and_an_arch_may_be_wider(self):
        with self.assertRaises(it.LayoutError):
            it.parse(two_rooms(width=150))
        (_d, _bd, _at, width), = it.parse(two_rooms(width=150, leaf="none")).door_spots()
        self.assertEqual(width, 150.0)

    def test_window_on_an_inside_wall(self):
        raw = two_rooms()
        raw["windows"] = [{"room": "a", "side": "E", "at": 200}]
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn("not an outside wall", str(ctx.exception))

    def test_openings_too_close(self):
        raw = two_rooms()
        raw["windows"] = [{"room": "a", "side": "N", "at": 380, "sill": 60}]
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn("closer than", str(ctx.exception))

    def test_sample_openings(self):
        layout = it.parse(SAMPLE)
        self.assertEqual(len(layout.door_spots()), 5)
        self.assertEqual(len(layout.exit_spots()), 2)
        self.assertEqual(len(layout.window_spots()), 13)
        upper = [s for s in layout.door_spots() if s[0]["id"] == "stair_gallery"][0]
        self.assertEqual(upper[1].band, 1)
        parts = it.stair_parts(layout, layout.stairs[0])
        top = parts["top_landing"]
        self.assertTrue(top[1] + it.MIN_JAMB <= upper[2] - it.DOOR_WIDTH / 2 and upper[2] + it.DOOR_WIDTH / 2 <= top[4],
                        "the upper stair door opens onto the top landing")


class WallAndSlabTest(unittest.TestCase):
    def test_a_door_leaves_two_side_pieces_and_a_lintel(self):
        layout = it.parse(two_rooms())
        (door, bd, at, width), = layout.door_spots()
        cuts = layout.openings()[id(bd)][1]
        pieces = it.wall_pieces(bd, cuts)
        self.assertEqual(sorted(pieces), [(0.0, 150.0, 0.0, 330.0), (150.0, 250.0, 220.0, 330.0),
                                          (250.0, 400.0, 0.0, 330.0)])

    def test_a_window_leaves_a_sill_and_a_head(self):
        layout = it.parse(dict(two_rooms(), windows=[{"room": "b", "side": "E", "at": 200, "width": 120}]))
        (item, bd, at, width), = layout.window_spots()
        pieces = it.wall_pieces(bd, layout.openings()[id(bd)][1])
        self.assertIn((140.0, 260.0, 0.0, 90.0), pieces)
        self.assertIn((140.0, 260.0, 240.0, 330.0), pieces)

    def test_a_rail_has_no_wall(self):
        layout = it.parse(SAMPLE)
        rail = [b for b in layout.boundaries(1) if b.kind == "rail"][0]
        self.assertEqual(it.wall_pieces(rail, []), [])

    def test_slabs(self):
        layout = it.parse(SAMPLE)
        slabs = layout.slabs()
        gallery_floor = [s for s in slabs if s[0] == 1 and s[5] == "gallery"]
        self.assertEqual([(s[1], s[2], s[3], s[4], s[6]) for s in gallery_floor], [(800.0, 0.0, 1200.0, 1400.0, "hall")])
        # The hall's open volume and the stair well have no floor-1 slab; the stair builds its own landings.
        self.assertFalse([s for s in slabs if s[0] == 1 and s[6] in ("hall", "stair") and s[5] in ("hall", "stair")])
        roofs = [s for s in slabs if s[5] is None]
        self.assertAlmostEqual(sum((s[3] - s[1]) * (s[4] - s[2]) for s in roofs),
                               sum((r.x1 - r.x0) * (r.y1 - r.y0) for r in layout.rooms if not r.overlooks))
        floors = [s for s in slabs if s[0] == 0]
        self.assertTrue(all(s[6] is None for s in floors))


class StairTest(unittest.TestCase):
    def setUp(self):
        self.layout = it.parse(SAMPLE)
        self.parts = it.stair_parts(self.layout, self.layout.stairs[0])

    def test_risers_and_treads(self):
        self.assertEqual(self.parts["n"], 10)
        self.assertAlmostEqual(self.parts["riser"], 16.5)
        self.assertAlmostEqual(self.parts["tread"], 28.0)
        self.assertLessEqual(self.parts["riser"], it.MAX_RISER)

    def test_climbable_all_the_way_up(self):
        self.assertTrue(it.stair_climb_ok(self.parts))
        self.assertAlmostEqual(self.parts["mid_landing"][5], it.FLOOR_HEIGHT / 2)
        self.assertAlmostEqual(self.parts["top_landing"][5], it.FLOOR_HEIGHT)
        self.assertEqual(len(self.parts["steps"]), 20)

    def test_a_broken_stair_is_not_climbable(self):
        broken = copy.deepcopy(self.parts)
        step = list(broken["steps"][4])
        step[5] += 30.0
        broken["steps"][4] = tuple(step)
        self.assertFalse(it.stair_climb_ok(broken))

    def test_flights_stay_in_the_well(self):
        room = self.layout.by_id["stair"]
        for x0, y0, z0, x1, y1, z1 in self.parts["steps"]:
            self.assertTrue(room.x0 <= x0 < x1 <= room.x1 and room.y0 <= y0 < y1 <= room.y1)

    def test_a_short_well_is_an_error(self):
        raw = sample_raw()
        [r for r in raw["rooms"] if r["id"] == "stair"][0]["rect"] = [500, 600, 800, 1000]
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn("treads", str(ctx.exception))

    def test_a_narrow_well_is_an_error(self):
        raw = sample_raw()
        [s for s in raw["stairs"]][0]["flight"] = 160
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn("spine", str(ctx.exception))

    def test_a_one_floor_well_is_an_error(self):
        raw = two_rooms()
        raw["stairs"] = [{"id": "s", "room": "a"}]
        with self.assertRaises(it.LayoutError) as ctx:
            it.parse(raw)
        self.assertIn("two", str(ctx.exception))


if __name__ == "__main__":
    unittest.main(verbosity=1)
