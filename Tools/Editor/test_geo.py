"""Unit tests for _geo.py. Plain unittest, no editor:

    Tools/test-geo.ps1
    (or) python Tools/Editor/test_geo.py
"""

import math
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _geo as geo  # noqa: E402

LAT0, LON0 = 40.7264398, -73.9816394  # the East Village district origin


class ProjectionTest(unittest.TestCase):
    def test_origin_maps_to_zero(self):
        self.assertEqual(geo.project(LAT0, LON0, LAT0, LON0), (0.0, 0.0))

    def test_one_millidegree_north_is_110_m(self):
        x, y = geo.project(LAT0 + 0.001, LON0, LAT0, LON0)
        self.assertAlmostEqual(x, 0.0, places=6)
        self.assertAlmostEqual(y, 110.54, places=2)

    def test_one_millidegree_east_is_shrunk_by_cos_lat(self):
        x, y = geo.project(LAT0, LON0 + 0.001, LAT0, LON0)
        self.assertAlmostEqual(x, 111.32 * math.cos(math.radians(LAT0)), places=3)
        self.assertAlmostEqual(x, 84.4, delta=0.1)
        self.assertAlmostEqual(y, 0.0, places=6)

    def test_round_trip(self):
        for lat, lon in ((40.7245, -73.9840), (40.7285, -73.9790), (LAT0, LON0)):
            x, y = geo.project(lat, lon, LAT0, LON0)
            la, lo = geo.unproject(x, y, LAT0, LON0)
            self.assertAlmostEqual(la, lat, places=10)
            self.assertAlmostEqual(lo, lon, places=10)

    def test_unreal_axes_east_is_plus_x_north_is_minus_y(self):
        x_cm, y_cm = geo.latlon_to_unreal_cm(LAT0 + 0.001, LON0 + 0.001, LAT0, LON0)
        self.assertGreater(x_cm, 0.0)   # east -> +X
        self.assertLess(y_cm, 0.0)      # north -> -Y
        self.assertAlmostEqual(x_cm, 8440.0, delta=10.0)
        self.assertAlmostEqual(y_cm, -11054.0, delta=0.5)
        self.assertEqual(geo.from_unreal_cm(*geo.to_unreal_cm(3.0, 4.0)), (3.0, 4.0))

    def test_unreal_mapping_is_not_mirrored(self):
        """Flipping one axis turns a right-handed ENU frame into Unreal's left-handed one.

        A counter-clockwise ring seen from above in ENU must come out clockwise in (X, Y)
        numbers, which is exactly what reads as counter-clockwise on Unreal's top view
        (X right, Y down). If both axes were flipped, or neither, the ring would keep its
        numeric winding and the map would be mirrored.
        """
        ring_enu = [(0, 0), (10, 0), (10, 10), (0, 10)]
        self.assertTrue(geo.is_ccw(ring_enu))
        ring_ue = [geo.to_unreal_cm(x, y) for x, y in ring_enu]
        self.assertFalse(geo.is_ccw(ring_ue))

    def test_scale_sanity_manhattan_street_spacing(self):
        """Tompkins Square: E 7th to E 10th is three street blocks, ~230 m centre to centre."""
        # Avenue A at E 7th and at E 10th St (centre-line intersections fitted from OSM).
        a = geo.project(40.726053, -73.983535, LAT0, LON0)
        b = geo.project(40.727847, -73.982231, LAT0, LON0)
        self.assertAlmostEqual(math.hypot(b[0] - a[0], b[1] - a[1]), 230.0, delta=15.0)


class PolygonTest(unittest.TestCase):
    SQUARE = [(0.0, 0.0), (4.0, 0.0), (4.0, 4.0), (0.0, 4.0)]
    L_SHAPE = [(0, 0), (6, 0), (6, 2), (2, 2), (2, 6), (0, 6)]

    def test_area_and_winding(self):
        self.assertEqual(geo.signed_area(self.SQUARE), 16.0)
        self.assertEqual(geo.signed_area(list(reversed(self.SQUARE))), -16.0)
        self.assertTrue(geo.is_ccw(self.SQUARE))
        self.assertFalse(geo.is_ccw(geo.oriented(self.SQUARE, ccw=False)))
        self.assertEqual(geo.signed_area(self.L_SHAPE), 20.0)

    def test_centroid(self):
        self.assertEqual(geo.centroid(self.SQUARE), (2.0, 2.0))
        cx, cy = geo.centroid(self.L_SHAPE)
        # L = 6x2 bar (centroid 3,1, area 12) + 2x4 bar (centroid 1,4, area 8)
        self.assertAlmostEqual(cx, (3 * 12 + 1 * 8) / 20.0)
        self.assertAlmostEqual(cy, (1 * 12 + 4 * 8) / 20.0)

    def test_point_in_concave_polygon(self):
        self.assertTrue(geo.point_in_polygon((1, 1), self.L_SHAPE))
        self.assertTrue(geo.point_in_polygon((1, 5), self.L_SHAPE))
        self.assertFalse(geo.point_in_polygon((4, 4), self.L_SHAPE))  # the notch
        self.assertFalse(geo.point_in_polygon((7, 1), self.L_SHAPE))

    def test_clean_ring_drops_closing_duplicate_and_collinear(self):
        ring = [(0, 0), (2, 0), (4, 0), (4, 4), (4, 4.001), (0, 4), (0, 0)]
        self.assertEqual(geo.clean_ring(ring), [(0, 0), (4, 0), (4, 4), (0, 4)])

    def test_clean_ring_rejects_slivers(self):
        self.assertEqual(geo.clean_ring([(0, 0), (1, 0), (2, 0)]), [])

    def test_bounds(self):
        self.assertEqual(geo.bounds(self.L_SHAPE), (0, 0, 6, 6))


class LineAndClipTest(unittest.TestCase):
    def test_fit_line_and_intersection(self):
        p1, d1 = geo.fit_line([(0, 1), (1, 2), (2, 3), (3, 4)])   # y = x + 1
        p2, d2 = geo.fit_line([(0, 5), (1, 4), (2, 3)])           # y = 5 - x
        x, y = geo.intersect_lines(p1, d1, p2, d2)
        self.assertAlmostEqual(x, 2.0)
        self.assertAlmostEqual(y, 3.0)
        self.assertIsNone(geo.intersect_lines((0, 0), (1, 0), (0, 1), (1, 0)))

    def test_frame_round_trip(self):
        f = geo.Frame((10.0, 5.0), (math.cos(0.5), math.sin(0.5)))
        q = f.to_local((13.0, 9.0))
        p = f.to_world(q)
        self.assertAlmostEqual(p[0], 13.0)
        self.assertAlmostEqual(p[1], 9.0)
        # v is u rotated +90 degrees
        self.assertAlmostEqual(f.u[0] * f.v[0] + f.u[1] * f.v[1], 0.0)
        self.assertAlmostEqual(f.u[0] * f.v[1] - f.u[1] * f.v[0], 1.0)

    def test_clip_segment(self):
        box = (0, 0, 10, 10)
        a, b = geo.clip_segment_to_box((-5, 5), (15, 5), box)
        self.assertEqual((a, b), ((0.0, 5.0), (10.0, 5.0)))
        self.assertIsNone(geo.clip_segment_to_box((-5, -5), (-1, 20), box))

    def test_clip_polyline_splits_when_it_leaves_and_reenters(self):
        box = (0, 0, 10, 10)
        pieces = geo.clip_polyline_to_box([(2, 2), (2, 15), (8, 15), (8, 2)], box)
        self.assertEqual(len(pieces), 2)
        self.assertEqual(pieces[0], [(2.0, 2.0), (2.0, 10.0)])
        self.assertEqual(pieces[1], [(8.0, 10.0), (8.0, 2.0)])

    def test_clip_polyline_inside_is_unchanged(self):
        pts = [(1, 1), (5, 5), (9, 1)]
        self.assertEqual(geo.clip_polyline_to_box(pts, (0, 0, 10, 10)), [pts])

    def test_polyline_length(self):
        self.assertEqual(geo.polyline_length([(0, 0), (3, 4), (3, 10)]), 11.0)


class TagTest(unittest.TestCase):
    def test_parse_length(self):
        self.assertEqual(geo.parse_length_m("15"), 15.0)
        self.assertEqual(geo.parse_length_m("15.5 m"), 15.5)
        self.assertEqual(geo.parse_length_m("21,3"), 21.3)
        self.assertAlmostEqual(geo.parse_length_m("50'"), 15.24)
        self.assertAlmostEqual(geo.parse_length_m("50 ft"), 15.24)
        self.assertAlmostEqual(geo.parse_length_m("12'6\""), 3.81)
        self.assertIsNone(geo.parse_length_m(None))
        self.assertIsNone(geo.parse_length_m("tall"))

    def test_height_priority(self):
        self.assertEqual(geo.building_height_m({"height": "18.2", "building:levels": "9"}),
                         (18.2, "height"))
        h, src = geo.building_height_m({"building": "apartments", "building:levels": "5"})
        self.assertEqual(src, "levels")
        self.assertAlmostEqual(h, 5 * 3.2 + 1.5)
        self.assertEqual(geo.building_height_m({"building": "yes"}), (15.0, "default"))
        self.assertEqual(geo.building_height_m({"building": "garage"}), (5.0, "small"))
        self.assertEqual(geo.building_height_m({"building": "shed", "height": "0"}), (5.0, "small"))

    def test_median(self):
        self.assertEqual(geo.median([3, 1, 2]), 2)
        self.assertEqual(geo.median([4, 1, 2, 3]), 2.5)
        self.assertIsNone(geo.median([]))


class HashTest(unittest.TestCase):
    def test_hash_is_stable_and_sensitive(self):
        a = geo.record_hash([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0]], 18.0)
        self.assertEqual(a, geo.record_hash([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0]], 18.0))
        self.assertEqual(a, geo.record_hash([[0.00001, 0.0], [1.0, 0.0], [1.0, 1.0]], 18.0))
        self.assertNotEqual(a, geo.record_hash([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0]], 18.5))
        self.assertEqual(len(a), 16)


if __name__ == "__main__":
    unittest.main(verbosity=1)
