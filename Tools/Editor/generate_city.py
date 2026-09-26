"""Generate the greybox East Village district from OpenStreetMap data.

    /Game/Maps/L_District_EastVillage          the map (World Partition off for one block)
    /Game/City/EastVillage/Meshes/SM_City_*    one static mesh per building, road, sidewalk

Input is what Tools/fetch-osm.ps1 wrote: Tools/Data/osm/east_village.buildings.json and
east_village.streets.json (lat/lon; this script projects them with _geo.py).

What gets built, all with Geometry Script from Python (the GeometryScripting plugin):

* every building: the footprint extruded to its height (append_simple_extrude_polygon, ear
  clipped so concave footprints work; courtyard holes are ignored), a 30 x 90 cm parapet ring
  just inside the roof edge, per-face normals, complex-as-simple collision so the player can
  stand on roofs and walk into walls. Actor ``City_Bldg_<id>``, M_Greybox.
* one ground slab under the whole district at z = 0 (``City_Ground``, M_Greybox_Floor).
* per named road way: carriageway at z = +2 cm (``City_Road_<id>``, M_Asphalt) and the 4 m
  sidewalks either side at z = +15 cm (``City_Sidewalk_<id>``, M_ConcreteFloor). Sidewalks are
  the road's buffer minus every carriageway, so they stop at the kerb of each crossing street.
* the park at z = +5 cm (``City_Park_<id>``, M_Grass), minus the carriageways.
* lighting (winter evening), a PlayerStart on East 7th Street facing the tenement row across
  from Tompkins Square Park, a NavMeshBoundsVolume.
* street lamps every 30 m on both sidewalks of every road (``City_Lamp_<n>`` spot light plus
  ``City_LampPole_<n>`` and ``City_LampHead_<n>``); only the ones around the park cast shadows.
  Lit windows are not built yet.
* chapter 1: the BP_GameMode_EastVillage override (starts DA_CH01_Rooftops) and three
  ``City_Obj_<objective>`` trigger volumes on roofs picked from the records.
* chapter 1's first fight: four ``City_Thug_<n>`` (BP_Thug). A Fists and a Bat thug face each
  other on the cross_block roof (tag RoofPair, counted by ``City_ThugGroup_clear_roof``, which
  completes ``clear_roof``); a Bat thug and the gunner patrol the Avenue A sidewalk beside the
  park between ``City_Patrol_0`` and ``City_Patrol_1`` (ATargetPoints 40 m apart, tag StreetPair).
* grapple anchors (``City_Anchor_<n>``, BP_GrappleAnchor): on every building over 8 m, one on
  the parapet at each roof corner and one mid-edge on edges over 25 m, none within 4 m of
  another, each with its landing point on the roof clear of the parapet.
* traversable ledges (``City_Ledge_<id>_<edge>``): a hidden Game Animation Sample
  LevelBlock_Traversable along every roof edge of 1 m or more, its Ledge_1 spline on the
  parapet's outer top edge, so the sample's vault and mantle see the tenements. Parkour test
  blocks: ``City_Test_Vault`` (90 cm) and ``City_Test_Mantle`` (150 cm) on the street by the
  PlayerStart, and three low ``City_ParkWall_<n>`` in the park.

Idempotent: each mesh carries a ``CityHash`` metadata tag (hash of its source record and the
generator version); a mesh is rebuilt only when that hash changes. Actors are found by label
and compared before anything is set. The level is saved only when something changed. City
actors whose record disappeared are deleted.

    UnrealEditor-Cmd.exe Castle.uproject -run=pythonscript ^
        -script="Tools\\Editor\\generate_city.py" -unattended -nullrhi -nosplash -nop4 -stdout

or Tools\\generate-city.ps1.
"""

import io
import json
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402
import _geo as geo  # noqa: E402
import _materials as m  # noqa: E402

DISTRICT = "east_village"
MAP_PATH = "/Game/Maps/L_District_EastVillage"
MESH_DIR = "/Game/City/EastVillage/Meshes"
KIT_MATERIALS = "/Game/Kit/Materials"
PLAYER_PATH = "/Game/Blueprints/Player"

# Bump when the mesh recipe changes, so every mesh regenerates once.
GENERATOR_VERSION = 1

LABEL_PREFIX = "City_"
BUILDING_PREFIX = "City_Bldg_"
ROAD_PREFIX = "City_Road_"
SIDEWALK_PREFIX = "City_Sidewalk_"
PARK_PREFIX = "City_Park_"
GROUND_LABEL = "City_Ground"
MESH_PREFIX = "SM_City_"
HASH_TAG = "CityHash"

PARAPET_HEIGHT = 90.0      # cm
PARAPET_THICK = 30.0       # cm
PARAPET_MIN_HEIGHT_M = 6.0  # no parapets on sheds and garages
PARAPET_MIN_EDGE = 50.0    # cm; shorter footprint edges get no parapet segment

ROAD_TOP = 2.0             # cm above the ground
SIDEWALK_TOP = 15.0
PARK_TOP = 5.0
SLAB_THICK = 20.0          # every flat surface is a slab this thick, so kerbs have faces
GROUND_THICK = 50.0
SIDEWALK_WIDTH = 400.0

NAV_Z_MIN = -500.0
NAV_Z_MAX = 7500.0

# --------------------------------------------------------------------------------------
# data
# --------------------------------------------------------------------------------------


def data_dir():
    return os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Data", "osm")


def data_paths():
    base = os.path.join(data_dir(), DISTRICT)
    return base + ".buildings.json", base + ".streets.json"


def data_available():
    return all(os.path.exists(p) for p in data_paths())


def load_json(path):
    with io.open(path, "r", encoding="utf-8") as f:
        return json.load(f)


class District(object):
    """The parsed records, projected into Unreal centimetres about the district origin."""

    def __init__(self):
        buildings_path, streets_path = data_paths()
        b = load_json(buildings_path)
        s = load_json(streets_path)
        self.meta = b["_meta"]
        self.lat0 = self.meta["origin"]["lat"]
        self.lon0 = self.meta["origin"]["lon"]
        self.buildings = b["buildings"]
        self.roads = s["roads"]
        self.parks = s["parks"]

    def cm(self, latlon):
        return geo.latlon_to_unreal_cm(latlon[0], latlon[1], self.lat0, self.lon0)

    def ring_cm(self, ring):
        return [self.cm(p) for p in ring]


# --------------------------------------------------------------------------------------
# small Geometry Script wrappers (Python returns out-params as tuples)
# --------------------------------------------------------------------------------------

GS_PRIM = unreal.GeometryScript_Primitives
GS_QUERY = unreal.GeometryScript_MeshQueries
GS_NORMALS = unreal.GeometryScript_Normals
GS_POLY = unreal.GeometryScript_PolygonList


def first(result):
    return result[0] if isinstance(result, tuple) else result


def v2(points):
    return [unreal.Vector2D(float(x), float(y)) for x, y in points]


def new_mesh():
    return unreal.DynamicMesh()


def mesh_volume(mesh):
    """Signed volume; negative means the triangles wind inside out."""
    res = GS_QUERY.get_mesh_volume_area(mesh)
    # (surface_area, volume)
    if isinstance(res, tuple):
        return float(res[1])
    return float(res)


def mesh_bounds(mesh):
    return first(GS_QUERY.get_mesh_bounding_box(mesh))


def fix_orientation(mesh):
    """Flip a closed mesh whose signed volume came out negative. Returns True if flipped."""
    if mesh_volume(mesh) < 0.0:
        GS_NORMALS.flip_normals(mesh)
        return True
    return False


def translate(mesh, offset):
    unreal.GeometryScript_MeshTransforms.translate_mesh(mesh, offset)


def polygon_list_from_path(path_cm, half_width):
    options = unreal.GeometryScriptOpenPathOffsetOptions()
    # Butt ends: a street that stops at an avenue stops at its centre line (the avenue covers
    # the rest), and nothing hangs past the edge of the ground slab.
    end = getattr(unreal.GeometryScriptPathOffsetEndType, "BUTT", None)
    if end is not None:
        options.set_editor_property("end_type", end)
    options.set_editor_property("join_type", unreal.GeometryScriptPolyOffsetJoinType.MITER)
    return first(GS_POLY.create_polygons_from_path_offset(v2(path_cm), options, float(half_width)))


def polygon_list_from_ring(ring_cm):
    simple = unreal.GeometryScript_SimplePolygon.conv_array_of_vector2d_to_geometry_script_simple_polygon(
        v2(ring_cm))
    return unreal.GeometryScript_PolygonList.create_polygon_list_from_simple_polygons([simple])


def polygon_list_area(polys):
    return float(GS_POLY.get_polygon_list_area(polys))


def slab_from_polygons(polys, top_z, thickness):
    """Triangulate a (holed) polygon list and extrude it down into a closed slab."""
    mesh = new_mesh()
    options = unreal.GeometryScriptPolygonsTriangulationOptions()
    first(GS_PRIM.append_polygon_list_triangulation(
        mesh, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(), polys, options))
    if GS_QUERY.get_num_triangle_i_ds(mesh) == 0:
        return None
    # The triangulation is an open patch, so its signed volume means nothing: look at a face.
    # It has to face up before it is pushed up, or the top of every slab is a back face.
    normal = first(GS_QUERY.get_triangle_face_normal(mesh, 0))
    if normal.z < 0.0:
        GS_NORMALS.flip_normals(mesh)
    extrude = unreal.GeometryScriptMeshLinearExtrudeOptions()
    extrude.set_editor_property("distance", float(thickness))
    extrude.set_editor_property("direction", unreal.Vector(0.0, 0.0, 1.0))
    extrude.set_editor_property("solids_to_shells", True)
    unreal.GeometryScript_MeshModeling.apply_mesh_linear_extrude_faces(
        mesh, extrude, unreal.GeometryScriptMeshSelection())
    box = mesh_bounds(mesh)
    translate(mesh, unreal.Vector(0.0, 0.0, top_z - box.max.z))
    GS_NORMALS.set_per_face_normals(mesh)
    return mesh


# --------------------------------------------------------------------------------------
# mesh recipes
# --------------------------------------------------------------------------------------


def building_mesh(ring_local, height_cm, parapet):
    """Extruded footprint (local cm, origin at the base centroid) plus the parapet ring."""
    mesh = new_mesh()
    GS_PRIM.append_simple_extrude_polygon(
        mesh, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(), v2(ring_local),
        float(height_cm), 0, True, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    if GS_QUERY.get_num_triangle_i_ds(mesh) == 0:
        return None
    fix_orientation(mesh)

    if parapet:
        # Interior is to the left of each edge for a counter-clockwise ring (numeric x/y).
        inward = 1.0 if geo.is_ccw(ring_local) else -1.0
        n = len(ring_local)
        for i in range(n):
            ax, ay = ring_local[i]
            bx, by = ring_local[(i + 1) % n]
            dx, dy = bx - ax, by - ay
            length = math.hypot(dx, dy)
            if length < PARAPET_MIN_EDGE:
                continue
            nx, ny = -dy / length * inward, dx / length * inward
            half = PARAPET_THICK * 0.5
            cx = (ax + bx) * 0.5 + nx * half
            cy = (ay + by) * 0.5 + ny * half
            yaw = math.degrees(math.atan2(dy, dx))
            xf = unreal.Transform(
                location=unreal.Vector(cx, cy, height_cm),
                rotation=unreal.Rotator(0.0, 0.0, yaw))
            # Long enough to close the corner with the next segment.
            GS_PRIM.append_box(
                mesh, unreal.GeometryScriptPrimitiveOptions(), xf,
                length + PARAPET_THICK, PARAPET_THICK, PARAPET_HEIGHT, 0, 0, 0,
                unreal.GeometryScriptPrimitiveOriginMode.BASE)

    GS_NORMALS.set_per_face_normals(mesh)
    return mesh


def write_static_mesh(mesh, name, spec_hash):
    """Create or overwrite /Game/City/.../<name> from ``mesh``. Returns the StaticMesh."""
    full = c.asset_path(MESH_DIR, name)
    existing = c.load_or_none(full)
    if existing is None:
        options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.set_editor_property("enable_recompute_normals", False)
        options.set_editor_property("enable_recompute_tangents", True)
        options.set_editor_property("enable_nanite", False)
        options.set_editor_property("enable_collision", True)
        options.set_editor_property("collision_mode", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        static_mesh = first(unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
            mesh, full, options))
        if static_mesh is None:
            c.log("FAILED", full, "create_new_static_mesh_asset_from_mesh returned None")
            return None
        action = "created"
    else:
        static_mesh = existing
        options = unreal.GeometryScriptCopyMeshToAssetOptions()
        options.set_editor_property("enable_recompute_normals", False)
        options.set_editor_property("enable_recompute_tangents", True)
        unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
            mesh, static_mesh, options, unreal.GeometryScriptMeshWriteLOD())
        action = "updated"

    ensure_complex_collision(static_mesh)
    unreal.EditorAssetLibrary.set_metadata_tag(static_mesh, HASH_TAG, spec_hash)
    c.save(static_mesh)
    return static_mesh, action


def ensure_complex_collision(static_mesh):
    try:
        body = static_mesh.get_editor_property("body_setup")
        if body is None:
            return False
        want = unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
        if body.get_editor_property("collision_trace_flag") != want:
            body.set_editor_property("collision_trace_flag", want)
            return True
    except Exception as exc:  # noqa: BLE001
        c.log_error("collision " + c.safe_name(static_mesh), exc)
    return False


def stored_hash(name):
    asset = c.load_or_none(c.asset_path(MESH_DIR, name))
    if asset is None:
        return None, None
    try:
        return asset, unreal.EditorAssetLibrary.get_metadata_tag(asset, HASH_TAG)
    except Exception:  # noqa: BLE001
        return asset, None


# --------------------------------------------------------------------------------------
# the pieces: each is (label, mesh name, spec hash, location, build_fn, material, tags)
# --------------------------------------------------------------------------------------


class Piece(object):
    def __init__(self, label, mesh_name, spec_hash, location, build, material, tags=()):
        self.label = label
        self.mesh_name = mesh_name
        self.spec_hash = spec_hash
        self.location = location
        self.build = build
        self.material = material
        self.tags = list(tags)


def building_pieces(district, material):
    pieces = []
    skipped_holes = 0
    for rec in district.buildings:
        ring = geo.clean_ring(district.ring_cm(rec["outer"]), min_edge=5.0, collinear_tol=2.0)
        if len(ring) < 3:
            c.log("skipped", BUILDING_PREFIX + rec["id"], "degenerate footprint")
            continue
        if rec.get("holes"):
            skipped_holes += 1
        cx, cy = geo.centroid(ring)
        local = [(x - cx, y - cy) for x, y in ring]
        height_cm = rec["height_m"] * 100.0
        parapet = rec["height_m"] >= PARAPET_MIN_HEIGHT_M
        spec = geo.record_hash(GENERATOR_VERSION, "bldg", [list(p) for p in local], height_cm, parapet)

        def build(local=local, height_cm=height_cm, parapet=parapet):
            return building_mesh(local, height_cm, parapet)

        tags = ["City", "CityBuilding", "osm:" + rec["id"],
                "height_cm:{0:.1f}".format(height_cm), "parapet:{0}".format(int(parapet))]
        street = rec.get("tags", {}).get("addr:street")
        if street:
            tags.append("street:" + street)
        pieces.append(Piece(BUILDING_PREFIX + rec["id"], MESH_PREFIX + rec["id"], spec,
                            unreal.Vector(cx, cy, 0.0), build, material, tags))
    if skipped_holes:
        c.log("skipped", "courtyard holes", "{0} building(s) had inner rings; extruded solid".format(skipped_holes))
    return pieces


def ground_ring_cm(district):
    return district.ring_cm(district.meta["ground_corners_latlon"])


def road_paths(district):
    """[(record, [path_cm, ...])] for every road record."""
    out = []
    for rec in district.roads:
        paths = [district.ring_cm(piece) for piece in rec["pieces"] if len(piece) >= 2]
        if paths:
            out.append((rec, paths))
    return out


def polys_for_paths(paths, half_width):
    polys = None
    for path in paths:
        p = polygon_list_from_path(path, half_width)
        if polys is None:
            polys = p
        else:
            GS_POLY.append_polygon_list(polys, p)
    return GS_POLY.polygons_union(polys) if polys is not None else None


def centre_of(points):
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    return (min(xs) + max(xs)) * 0.5, (min(ys) + max(ys)) * 0.5


def shifted(mesh, cx, cy):
    """Move a mesh built in world centimetres so its origin sits at (cx, cy, 0)."""
    if mesh is not None:
        translate(mesh, unreal.Vector(-cx, -cy, 0.0))
    return mesh


def street_pieces(district, materials):
    """Ground, roads, sidewalks and parks. Built in world cm, then shifted to a local origin."""
    pieces = []

    ground = ground_ring_cm(district)
    gx, gy = centre_of(ground)
    pieces.append(Piece(
        GROUND_LABEL, MESH_PREFIX + "Ground",
        geo.record_hash(GENERATOR_VERSION, "ground", ground),
        unreal.Vector(gx, gy, 0.0),
        lambda: shifted(slab_from_polygons(polygon_list_from_ring(ground), 0.0, GROUND_THICK), gx, gy),
        materials["floor"], ["City", "CityGround"]))

    roads = road_paths(district)
    all_road_spec = geo.record_hash([[r["id"], r["width_m"], p] for r, p in roads])

    # Every carriageway at once, for cutting the sidewalks and the park. Built lazily, once,
    # and only if some sidewalk or park actually needs rebuilding.
    cache = {}

    def carriageways():
        if "all" not in cache:
            polys = None
            for rec, paths in roads:
                p = polys_for_paths(paths, rec["width_m"] * 50.0)
                if p is None:
                    continue
                if polys is None:
                    polys = p
                else:
                    GS_POLY.append_polygon_list(polys, p)
            cache["all"] = GS_POLY.polygons_union(polys)
        return cache["all"]

    for rec, paths in roads:
        cx, cy = centre_of([p for path in paths for p in path])
        half = rec["width_m"] * 50.0
        loc = unreal.Vector(cx, cy, 0.0)
        name_tag = "street:" + (rec.get("name") or "")

        def build_road(paths=paths, half=half, cx=cx, cy=cy):
            return shifted(slab_from_polygons(polys_for_paths(paths, half), ROAD_TOP, SLAB_THICK), cx, cy)

        pieces.append(Piece(
            ROAD_PREFIX + rec["id"], MESH_PREFIX + "Road_" + rec["id"],
            geo.record_hash(GENERATOR_VERSION, "road", paths, half), loc, build_road,
            materials["asphalt"], ["City", "CityRoad", "osm:" + rec["id"], name_tag]))

        def build_sidewalk(paths=paths, half=half, cx=cx, cy=cy):
            outer = polys_for_paths(paths, half + SIDEWALK_WIDTH)
            walk = GS_POLY.polygons_difference(outer, carriageways())
            if polygon_list_area(walk) < 1.0:
                return None
            return shifted(slab_from_polygons(walk, SIDEWALK_TOP, SLAB_THICK), cx, cy)

        pieces.append(Piece(
            SIDEWALK_PREFIX + rec["id"], MESH_PREFIX + "Sidewalk_" + rec["id"],
            geo.record_hash(GENERATOR_VERSION, "sidewalk", paths, half, all_road_spec), loc,
            build_sidewalk, materials["sidewalk"],
            ["City", "CitySidewalk", "osm:" + rec["id"], name_tag]))

    for park in district.parks:
        ring = geo.clean_ring(district.ring_cm(park["outer"]), min_edge=5.0, collinear_tol=2.0)
        if len(ring) < 3:
            continue
        cx, cy = geo.centroid(ring)

        def build_park(ring=ring, cx=cx, cy=cy):
            grass = GS_POLY.polygons_difference(polygon_list_from_ring(ring), carriageways())
            return shifted(slab_from_polygons(grass, PARK_TOP, SLAB_THICK), cx, cy)

        pieces.append(Piece(
            PARK_PREFIX + park["id"], MESH_PREFIX + "Park_" + park["id"],
            geo.record_hash(GENERATOR_VERSION, "park", ring, all_road_spec),
            unreal.Vector(cx, cy, 0.0), build_park, materials["grass"],
            ["City", "CityPark", "osm:" + park["id"], "name:" + (park.get("name") or "")]))

    return pieces


# --------------------------------------------------------------------------------------
# actors
# --------------------------------------------------------------------------------------


def actors_by_label():
    out = {}
    for actor in c.all_level_actors():
        try:
            out[actor.get_actor_label()] = actor
        except Exception:  # noqa: BLE001
            continue
    return out


def same_vector(a, b, tol=0.05):
    return abs(a.x - b.x) <= tol and abs(a.y - b.y) <= tol and abs(a.z - b.z) <= tol


def ensure_piece_actor(piece, static_mesh, existing):
    """Place or fix one StaticMeshActor. Returns the number of changes made."""
    changes = 0
    actor = existing.get(piece.label)
    if actor is not None and not isinstance(actor, unreal.StaticMeshActor):
        actor.destroy_actor()
        actor = None
        changes += 1
    if actor is None:
        actor = c.spawn_actor(unreal.StaticMeshActor, piece.location, label=piece.label)
        if actor is None:
            c.log("FAILED", piece.label, "spawn_actor returned None")
            return 0
        existing[piece.label] = actor
        changes += 1

    component = actor.get_editor_property("static_mesh_component")
    if component.get_editor_property("mobility") != unreal.ComponentMobility.STATIC:
        component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        changes += 1
    if not same_vector(actor.get_actor_location(), piece.location):
        actor.set_actor_location(piece.location, False, True)
        changes += 1
    rot = actor.get_actor_rotation()
    if abs(rot.pitch) > 1e-3 or abs(rot.yaw) > 1e-3 or abs(rot.roll) > 1e-3:
        actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
        changes += 1
    if not same_vector(actor.get_actor_scale3d(), unreal.Vector(1.0, 1.0, 1.0), 1e-4):
        actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
        changes += 1
    if component.get_editor_property("static_mesh") != static_mesh:
        component.set_static_mesh(static_mesh)
        changes += 1
    if piece.material is not None:
        overrides = component.get_editor_property("override_materials")
        if len(overrides) < 1 or overrides[0] != piece.material:
            component.set_material(0, piece.material)
            changes += 1
    want_tags = [unreal.Name(t) for t in piece.tags]
    have = [str(t) for t in actor.get_editor_property("tags")]
    if have != piece.tags:
        actor.set_editor_property("tags", want_tags)
        changes += 1
    return changes


def build_pieces(pieces, existing):
    """Rebuild changed meshes, place actors. Returns (mesh changes, actor changes)."""
    c.ensure_directory(MESH_DIR)
    mesh_changes = 0
    actor_changes = 0
    unchanged = 0
    for piece in pieces:
        try:
            asset, current = stored_hash(piece.mesh_name)
            static_mesh = asset
            if asset is None or current != piece.spec_hash:
                mesh = piece.build()
                if mesh is None:
                    c.log("skipped", piece.label, "empty geometry")
                    continue
                result = write_static_mesh(mesh, piece.mesh_name, piece.spec_hash)
                if result is None:
                    continue
                static_mesh, action = result
                mesh_changes += 1
                c.log(action, c.asset_path(MESH_DIR, piece.mesh_name))
            else:
                if ensure_complex_collision(asset):
                    c.save(asset)
                    mesh_changes += 1
                    c.log("updated", c.asset_path(MESH_DIR, piece.mesh_name), "collision -> complex as simple")
                unchanged += 1
            n = ensure_piece_actor(piece, static_mesh, existing)
            if n:
                actor_changes += 1
                c.log("updated", piece.label, "{0} actor change(s)".format(n))
        except Exception as exc:  # noqa: BLE001
            c.log_error("piece " + piece.label, exc)
    c.log("exists", "{0} city meshes".format(unchanged), "hash unchanged, not rebuilt")
    return mesh_changes, actor_changes


def remove_stale(pieces, existing):
    wanted = {p.label for p in pieces}
    removed = 0
    for label, actor in list(existing.items()):
        if label.startswith(LABEL_PREFIX) and label not in wanted and label.split("_")[1] in (
                "Bldg", "Road", "Sidewalk", "Park", "Ground"):
            actor.destroy_actor()
            existing.pop(label, None)
            removed += 1
            c.log("updated", label, "removed; no longer in the OSM records")
    return removed


# --------------------------------------------------------------------------------------
# lighting, player start, nav, game mode
# --------------------------------------------------------------------------------------

SUN_ROTATION = unreal.Rotator(0.0, -12.0, -30.0)   # roll, pitch, yaw: low WSW sun, light heading ENE
SUN_LUX = 3.0
SUN_TEMPERATURE = 4800.0
SKY_INTENSITY = 2.5
FOG_DENSITY = 0.008
# Auto exposure is off project-wide (Config/DefaultEngine.ini), so exposure is fixed: the
# min/max brightness pair pins it the way L_Sandbox does, and the bias is the knob.
EXPOSURE_BRIGHTNESS = 1.0
EXPOSURE_BIAS = 1.5


def set_if_different(obj, prop, value, context, tol=1e-3):
    try:
        current = obj.get_editor_property(prop)
    except Exception as exc:  # noqa: BLE001
        c.log_error("{0}.{1}".format(context, prop), exc)
        return 0
    same = False
    if isinstance(value, float) and isinstance(current, (float, int)):
        same = abs(float(current) - value) <= tol
    elif isinstance(value, unreal.Rotator):
        same = (abs(current.pitch - value.pitch) <= tol and abs(current.yaw - value.yaw) <= tol
                and abs(current.roll - value.roll) <= tol)
    else:
        same = current == value
    if same:
        return 0
    obj.set_editor_property(prop, value)
    return 1


def ensure_lighting(existing):
    changes = 0
    sun, created = ensure_labelled(existing, unreal.DirectionalLight, "Sun", unreal.Vector(0, 0, 3000), SUN_ROTATION)
    changes += created
    if sun is not None:
        comp = sun.get_editor_property("directional_light_component")
        changes += set_if_different(comp, "mobility", unreal.ComponentMobility.MOVABLE, "Sun")
        changes += set_if_different(comp, "intensity", SUN_LUX, "Sun")
        changes += set_if_different(comp, "atmosphere_sun_light", True, "Sun")
        changes += set_if_different(comp, "use_temperature", True, "Sun")
        changes += set_if_different(comp, "temperature", SUN_TEMPERATURE, "Sun")
        rot = sun.get_actor_rotation()
        if abs(rot.pitch - SUN_ROTATION.pitch) > 0.01 or abs(rot.yaw - SUN_ROTATION.yaw) > 0.01:
            sun.set_actor_rotation(SUN_ROTATION, False)
            changes += 1

    sky, created = ensure_labelled(existing, unreal.SkyLight, "SkyLight", unreal.Vector(0, 0, 2000))
    changes += created
    if sky is not None:
        comp = sky.get_editor_property("light_component")
        changes += set_if_different(comp, "mobility", unreal.ComponentMobility.MOVABLE, "SkyLight")
        changes += set_if_different(comp, "real_time_capture", True, "SkyLight")
        changes += set_if_different(comp, "source_type", unreal.SkyLightSourceType.SLS_CAPTURED_SCENE, "SkyLight")
        changes += set_if_different(comp, "intensity", SKY_INTENSITY, "SkyLight")

    atmosphere = c.find_class("SkyAtmosphere", "/Script/Engine.SkyAtmosphere")
    _a, created = ensure_labelled(existing, atmosphere, "SkyAtmosphere", unreal.Vector(0, 0, 0))
    changes += created

    fog, created = ensure_labelled(existing, unreal.ExponentialHeightFog, "HeightFog", unreal.Vector(0, 0, 0))
    changes += created
    if fog is not None:
        comp = fog.get_editor_property("component")
        changes += set_if_different(comp, "fog_density", FOG_DENSITY, "HeightFog", 1e-5)

    pp, created = ensure_labelled(existing, unreal.PostProcessVolume, "PP_Global", unreal.Vector(0, 0, 0))
    changes += created
    if pp is not None:
        changes += set_if_different(pp, "unbound", True, "PP_Global")
        settings = pp.get_editor_property("settings")
        dirty = False
        for prop, value in (("override_auto_exposure_min_brightness", True),
                            ("override_auto_exposure_max_brightness", True),
                            ("auto_exposure_min_brightness", EXPOSURE_BRIGHTNESS),
                            ("auto_exposure_max_brightness", EXPOSURE_BRIGHTNESS),
                            ("override_auto_exposure_bias", True),
                            ("auto_exposure_bias", EXPOSURE_BIAS)):
            current = settings.get_editor_property(prop)
            if (isinstance(value, float) and abs(float(current) - value) > 1e-4) or (
                    not isinstance(value, float) and current != value):
                settings.set_editor_property(prop, value)
                dirty = True
        if dirty:
            pp.set_editor_property("settings", settings)
            changes += 1

    # Nothing in this map may bake: every light is movable (Lumen, no lightmass).
    for actor in c.all_level_actors():
        if isinstance(actor, (unreal.Light, unreal.SkyLight)):
            root = actor.get_editor_property("root_component")
            if root is not None and root.get_editor_property("mobility") != unreal.ComponentMobility.MOVABLE:
                root.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
                changes += 1
    return changes


def ensure_labelled(existing, actor_class, label, location, rotation=None):
    """(actor, 1 if spawned else 0)."""
    actor = existing.get(label)
    if actor is not None:
        return actor, 0
    if actor_class is None:
        c.log("skipped", label, "class not available")
        return None, 0
    actor = c.spawn_actor(actor_class, location, rotation, label=label)
    if actor is None:
        c.log("FAILED", label, "spawn_actor returned None")
        return None, 0
    existing[label] = actor
    c.log("created", label)
    return actor, 1


def closest_point_on_polyline(p, path):
    best = None
    for i in range(len(path) - 1):
        ax, ay = path[i]
        bx, by = path[i + 1]
        dx, dy = bx - ax, by - ay
        L2 = dx * dx + dy * dy
        t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / L2))
        q = (ax + t * dx, ay + t * dy)
        d = math.hypot(q[0] - p[0], q[1] - p[1])
        if best is None or d < best[0]:
            best = (d, q)
    return best


PLAYER_START_STREET = "East 7th Street"


def player_start_transform(district):
    """On East 7th Street across from the park, looking at the tenement row to the south."""
    park = district.parks[0] if district.parks else None
    target = geo.centroid(district.ring_cm(park["outer"])) if park else (0.0, 0.0)
    best = None
    for rec in district.roads:
        if rec.get("name") != PLAYER_START_STREET:
            continue
        for piece in rec["pieces"]:
            hit = closest_point_on_polyline(target, district.ring_cm(piece))
            if hit and (best is None or hit[0] < best[0]):
                best = hit
    if best is None:
        return unreal.Vector(0.0, 0.0, 100.0), unreal.Rotator(0.0, 0.0, 0.0)
    q = best[1]
    away = (q[0] - target[0], q[1] - target[1])
    yaw = math.degrees(math.atan2(away[1], away[0]))
    # Stand 3 m on the park side of the centre line so the row fills the view.
    L = math.hypot(*away) or 1.0
    pos = (q[0] - away[0] / L * 300.0, q[1] - away[1] / L * 300.0)
    return unreal.Vector(pos[0], pos[1], 100.0), unreal.Rotator(0.0, 0.0, yaw)


def ensure_player_start(district, existing):
    loc, rot = player_start_transform(district)
    actor, created = ensure_labelled(existing, unreal.PlayerStart, "PlayerStart", loc, rot)
    changes = created
    if actor is not None and not created:
        if not same_vector(actor.get_actor_location(), loc, 1.0):
            actor.set_actor_location(loc, False, True)
            changes += 1
        if abs(actor.get_actor_rotation().yaw - rot.yaw) > 0.1:
            actor.set_actor_rotation(rot, False)
            changes += 1
    return changes


def ensure_nav_volume(district, existing):
    ground = ground_ring_cm(district)
    xmin, ymin, xmax, ymax = geo.bounds(ground)
    centre = unreal.Vector((xmin + xmax) * 0.5, (ymin + ymax) * 0.5, (NAV_Z_MIN + NAV_Z_MAX) * 0.5)
    # A brush volume is 200 units per side at scale 1.
    scale = unreal.Vector((xmax - xmin) / 200.0, (ymax - ymin) / 200.0, (NAV_Z_MAX - NAV_Z_MIN) / 200.0)
    nav_class = c.find_class("NavMeshBoundsVolume", "/Script/NavigationSystem.NavMeshBoundsVolume")
    actor, created = ensure_labelled(existing, nav_class, "NavMeshBounds", centre)
    changes = created
    if actor is not None:
        if not same_vector(actor.get_actor_location(), centre, 1.0):
            actor.set_actor_location(centre, False, True)
            changes += 1
        if not same_vector(actor.get_actor_scale3d(), scale, 1e-3):
            actor.set_actor_scale3d(scale)
            changes += 1
    return changes


# --------------------------------------------------------------------------------------
# chapter 1: the district's own game mode, and the objective volumes on three roofs
# --------------------------------------------------------------------------------------

DISTRICT_GAME_MODE = "BP_GameMode_EastVillage"
MISSION_ASSET = "/Game/Missions/DA_CH01_Rooftops"
OBJECTIVE_PREFIX = "City_Obj_"
OBJECTIVE_IDS = ("reach_roof", "cross_block", "find_arrow")
OBJECTIVE_START_RADIUS = 6000.0   # cm: reach_roof is the tallest building this close to the start
OBJECTIVE_MIN_HEIGHT_M = 8.0      # cross_block and find_arrow skip sheds and garages
OBJECTIVE_ABOVE_ROOF = 50.0       # cm between the roof and the bottom of the volume
OBJECTIVE_HALF_HEIGHT = 150.0     # cm; tall enough to hold Kate's whole capsule

# Every objective in DA_CH01_Rooftops, in order; clear_roof has no volume (a thug group completes it).
MISSION_OBJECTIVE_IDS = ("reach_roof", "cross_block", "clear_roof", "find_arrow")

# Actors the prison build put in its maps. None belong in the district, except the chapter's own
# thugs, which carry THUG_PREFIX labels.
PRISON_CLASS_WORDS = ("Thug", "Guard", "Keycard", "Pickup", "Door")


def ensure_district_game_mode():
    """BP_GameMode_EastVillage (child of BP_CastleGameMode) starting DA_CH01_Rooftops.

    Returns (generated class or None, number of changes)."""
    parent = c.load_generated_class(PLAYER_PATH, "BP_CastleGameMode")
    if parent is None:
        c.log("skipped", DISTRICT_GAME_MODE, "BP_CastleGameMode_C not found; run create_blueprints.py")
        return None, 0
    full = c.asset_path(PLAYER_PATH, DISTRICT_GAME_MODE)
    changes = 0
    bp = c.load_or_none(full)
    if bp is None:
        factory = c.new_factory("BlueprintFactory")
        if factory is None:
            c.log("FAILED", full, "BlueprintFactory unavailable")
            return None, 0
        c.set_props(factory, [("parent_class", parent)], DISTRICT_GAME_MODE + " factory")
        bp, _created = c.create_asset(DISTRICT_GAME_MODE, PLAYER_PATH, unreal.Blueprint, factory, quiet=True)
        if bp is None:
            return None, 0
        c.log("created", full, "parent BP_CastleGameMode_C")
        changes += 1

    mission = c.load_or_none(MISSION_ASSET)
    cdo = c.blueprint_cdo(bp)
    if mission is None:
        c.log("skipped", full, "DA_CH01_Rooftops missing; run create_mission_data.py")
    elif cdo is not None and cdo.get_editor_property("starting_mission") != mission:
        if c.set_props(cdo, [("starting_mission", mission)], DISTRICT_GAME_MODE):
            changes += 1
            c.log("updated", full, "starting_mission = DA_CH01_Rooftops")
    if changes:
        c.compile_blueprint(bp)
        c.save(bp)
    else:
        c.log("exists", full)
    return c.load_generated_class(PLAYER_PATH, DISTRICT_GAME_MODE), changes


def ensure_game_mode():
    """The map's GameMode override: BP_GameMode_EastVillage, or BP_CastleGameMode as a fallback."""
    game_mode, _changes = ensure_district_game_mode()
    name = DISTRICT_GAME_MODE + "_C"
    if game_mode is None:
        game_mode = c.load_generated_class(PLAYER_PATH, "BP_CastleGameMode")
        name = "BP_CastleGameMode_C"
    if game_mode is None:
        c.log("skipped", MAP_PATH, "no game mode class found; GameMode override unset")
        return 0
    ws = c.world_settings()
    if ws is None:
        return 0
    if ws.get_editor_property("default_game_mode") == game_mode:
        return 0
    ws.set_editor_property("default_game_mode", game_mode)
    c.log("updated", MAP_PATH, "GameMode override = " + name)
    return 1


def actor_class_name(actor):
    try:
        return actor.get_class().get_name()
    except Exception:  # noqa: BLE001
        return c.class_name(type(actor))


def is_chapter_actor(label):
    """True for the fight's own thugs and their group, which share class words with the prison's."""
    return label.startswith(THUG_PREFIX) or label == THUG_GROUP_LABEL


def remove_prison_actors(existing):
    """Delete anything the prison build would have placed (thugs, keycards, doors, pickups)."""
    removed = 0
    for label, actor in list(existing.items()):
        cls = actor_class_name(actor)
        if is_chapter_actor(label):
            continue
        if any(word in cls for word in PRISON_CLASS_WORDS):
            actor.destroy_actor()
            existing.pop(label, None)
            removed += 1
            c.log("updated", label, "removed prison-build actor ({0})".format(cls))
    if not removed:
        c.log("exists", MAP_PATH, "no thug, keycard, door or pickup actors")
    return removed


def _segments_cross(a, b, p, q):
    def orient(o, s, t):
        return (s[0] - o[0]) * (t[1] - o[1]) - (s[1] - o[1]) * (t[0] - o[0])
    d1, d2 = orient(p, q, a), orient(p, q, b)
    d3, d4 = orient(a, b, p), orient(a, b, q)
    return (d1 > 0) != (d2 > 0) and (d3 > 0) != (d4 > 0)


def streets_crossed(district, a, b):
    """Names of the road centre lines the segment a-b crosses."""
    names = set()
    for rec in district.roads:
        for piece in rec["pieces"]:
            path = district.ring_cm(piece)
            if any(_segments_cross(a, b, p, q) for p, q in zip(path, path[1:])):
                names.add(rec.get("name") or rec["id"])
    return names


def ring_distance(pt, ring):
    """0 inside the ring, else the distance to its nearest edge."""
    if geo.point_in_polygon(pt, ring):
        return 0.0
    return closest_point_on_polyline(pt, list(ring) + [ring[0]])[0]


def oriented_rect(ring):
    """Smallest rectangle around the ring with a side along one of its edges:
    (centre x, centre y, half length, half width, yaw degrees)."""
    best = None
    n = len(ring)
    for i in range(n):
        ax, ay = ring[i]
        bx, by = ring[(i + 1) % n]
        L = math.hypot(bx - ax, by - ay)
        if L < 1.0:
            continue
        ux, uy = (bx - ax) / L, (by - ay) / L
        us = [x * ux + y * uy for x, y in ring]
        vs = [-x * uy + y * ux for x, y in ring]
        area = (max(us) - min(us)) * (max(vs) - min(vs))
        if best is None or area < best[0]:
            cu, cv = (max(us) + min(us)) * 0.5, (max(vs) + min(vs)) * 0.5
            best = (area, cu * ux - cv * uy, cu * uy + cv * ux,
                    (max(us) - min(us)) * 0.5, (max(vs) - min(vs)) * 0.5, math.degrees(math.atan2(uy, ux)))
    return best[1:] if best else None


def objective_roofs(district):
    """{objective id: building record plus 'ring' and 'centre'} for the three chapter-1 roofs.

    reach_roof: the tallest building within OBJECTIVE_START_RADIUS of the PlayerStart.
    cross_block: the building on the same block (no street centre line between the two
    centroids) farthest from it. find_arrow: the building nearest cross_block that is exactly
    one street away from it and off the first block.
    """
    start, _rot = player_start_transform(district)
    start = (start.x, start.y)
    buildings = []
    for rec in district.buildings:
        ring = geo.clean_ring(district.ring_cm(rec["outer"]), min_edge=5.0, collinear_tol=2.0)
        if len(ring) >= 3:
            buildings.append(dict(rec, ring=ring, centre=geo.centroid(ring)))

    near = [b for b in buildings if ring_distance(start, b["ring"]) <= OBJECTIVE_START_RADIUS]
    if not near:
        return {}
    first = max(near, key=lambda b: (b["height_m"], b["id"]))

    def dist(a, b):
        return math.hypot(a["centre"][0] - b["centre"][0], a["centre"][1] - b["centre"][1])

    tall = [b for b in buildings if b["height_m"] >= OBJECTIVE_MIN_HEIGHT_M and b is not first]
    same_block = [b for b in tall if not streets_crossed(district, first["centre"], b["centre"])]
    if not same_block:
        return {"reach_roof": first}
    second = max(same_block, key=lambda b: (dist(first, b), b["id"]))

    out = {"reach_roof": first, "cross_block": second}
    candidates = sorted((b for b in tall if b is not second), key=lambda b: (dist(second, b), b["id"]))
    for b in candidates:
        if streets_crossed(district, first["centre"], b["centre"]) and \
                len(streets_crossed(district, second["centre"], b["centre"])) == 1:
            out["find_arrow"] = b
            break
    return out


def ensure_objective_volumes(district, existing):
    volume_class = c.find_class("ObjectiveTriggerVolume", "/Script/Castle.ObjectiveTriggerVolume")
    if volume_class is None:
        c.log("skipped", OBJECTIVE_PREFIX + "*", "AObjectiveTriggerVolume not exposed; build the module")
        return 0
    roofs = objective_roofs(district)
    changes = 0
    wanted = set()
    for oid in OBJECTIVE_IDS:
        rec = roofs.get(oid)
        label = OBJECTIVE_PREFIX + oid
        if rec is None:
            c.log("FAILED", label, "no suitable roof found")
            continue
        wanted.add(label)
        cx, cy, half_l, half_w, yaw = oriented_rect(rec["ring"])
        roof_z = rec["height_m"] * 100.0
        loc = unreal.Vector(cx, cy, roof_z + OBJECTIVE_ABOVE_ROOF + OBJECTIVE_HALF_HEIGHT)
        rot = unreal.Rotator(0.0, 0.0, yaw)
        extent = unreal.Vector(half_l, half_w, OBJECTIVE_HALF_HEIGHT)
        tags = ["City", "CityObjective", "osm:" + rec["id"], "objective:" + oid]

        actor = existing.get(label)
        n = 0
        if actor is not None and not isinstance(actor, volume_class):
            actor.destroy_actor()
            actor = None
            n += 1
        if actor is None:
            actor = c.spawn_actor(volume_class, loc, rot, label=label)
            if actor is None:
                c.log("FAILED", label, "spawn_actor returned None")
                continue
            existing[label] = actor
            n += 1
        if not same_vector(actor.get_actor_location(), loc, 1.0):
            actor.set_actor_location(loc, False, True)
            n += 1
        if abs(((actor.get_actor_rotation().yaw - yaw) + 180.0) % 360.0 - 180.0) > 0.1:
            actor.set_actor_rotation(rot, False)
            n += 1
        if not same_vector(actor.get_actor_scale3d(), unreal.Vector(1.0, 1.0, 1.0), 1e-4):
            actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
            n += 1
        box = actor.get_editor_property("collision_component")
        if box is not None and not same_vector(box.get_editor_property("box_extent"), extent, 1.0):
            box.set_editor_property("box_extent", extent)
            n += 1
        if str(actor.get_editor_property("objective_id")) != oid:
            actor.set_editor_property("objective_id", unreal.Name(oid))
            n += 1
        if [str(t) for t in actor.get_editor_property("tags")] != tags:
            actor.set_editor_property("tags", [unreal.Name(t) for t in tags])
            n += 1
        changes += n
        tags_rec = rec.get("tags", {})
        address = "{0} {1}".format(tags_rec.get("addr:housenumber") or "", tags_rec.get("addr:street") or "").strip()
        c.log("updated" if n else "exists", label, "osm {0}, {1:.1f} m, {2}, roof box {3:.0f} x {4:.0f} m".format(
            rec["id"], rec["height_m"], address or "no address", half_l / 50.0, half_w / 50.0))

    for label, actor in list(existing.items()):
        if isinstance(actor, volume_class) and label not in wanted:
            actor.destroy_actor()
            existing.pop(label, None)
            changes += 1
            c.log("updated", label, "removed stray objective volume")
    return changes


# --------------------------------------------------------------------------------------
# chapter 1's first fight: four Tracksuit thugs
# --------------------------------------------------------------------------------------

THUG_PREFIX = "City_Thug_"
PATROL_PREFIX = "City_Patrol_"
THUG_GROUP_LABEL = "City_ThugGroup_clear_roof"
THUG_BP_PATH = "/Game/Blueprints/AI"
THUG_BP_NAME = "BP_Thug"
THUG_HALF_HEIGHT = 96.0           # cm; BP_Thug's capsule
ROOF_PAIR_TAG = "RoofPair"
STREET_PAIR_TAG = "StreetPair"
ROOF_PAIR_GAP = 300.0             # cm between the two arguing on the roof
ROOF_EDGE_CLEARANCE = 200.0       # cm from any roof edge, so neither stands in the parapet
PATROL_STREET = "Avenue A"
PATROL_LENGTH = 4000.0            # cm between the two patrol points
PATROL_POINT_HEIGHT = 100.0       # cm above the sidewalk
STREET_PAIR_SPACING = 150.0       # cm; the second walks this far ahead of the first
CROSSING_CLEARANCE = 300.0        # cm; a patrol point this far from any other road's carriageway

# (weapon, group tag) per thug, in label order: the roof pair (one fists, one bat), then the
# street pair (one bat, one gunner). One gunner in four.
THUG_LOADOUT = (("FISTS", ROOF_PAIR_TAG), ("BAT", ROOF_PAIR_TAG),
                ("BAT", STREET_PAIR_TAG), ("PISTOL", STREET_PAIR_TAG))


def _edge_clearance(pt, ring):
    """Distance inside the ring to its nearest edge; negative outside."""
    d = closest_point_on_polyline(pt, list(ring) + [ring[0]])[0]
    return d if geo.point_in_polygon(pt, ring) else -d


def roof_pair_spots(district):
    """((x, y, z, yaw), (x, y, z, yaw), building record) for the two on the cross_block roof.

    They stand ROOF_PAIR_GAP apart along the roof's long axis through the centre of its oriented
    rectangle, facing each other; the short axis and then a smaller gap are tried if the long
    one puts either of them within ROOF_EDGE_CLEARANCE of an edge."""
    rec = objective_roofs(district).get("cross_block")
    if rec is None:
        return None
    cx, cy, _half_l, _half_w, yaw = oriented_rect(rec["ring"])
    z = rec["height_m"] * 100.0 + THUG_HALF_HEIGHT + 2.0
    for gap in (ROOF_PAIR_GAP, ROOF_PAIR_GAP * 0.6):
        for axis_yaw in (yaw, yaw + 90.0):
            ux, uy = math.cos(math.radians(axis_yaw)), math.sin(math.radians(axis_yaw))
            a = (cx - ux * gap * 0.5, cy - uy * gap * 0.5)
            b = (cx + ux * gap * 0.5, cy + uy * gap * 0.5)
            if min(_edge_clearance(a, rec["ring"]), _edge_clearance(b, rec["ring"])) >= ROOF_EDGE_CLEARANCE:
                return (a[0], a[1], z, axis_yaw), (b[0], b[1], z, axis_yaw + 180.0), rec
    return None


def _clear_of_crossings(pt, district, own):
    for rec in district.roads:
        if rec.get("name") == own:
            continue
        for piece in rec["pieces"]:
            hit = closest_point_on_polyline(pt, district.ring_cm(piece))
            if hit and hit[0] < rec["width_m"] * 50.0 + CROSSING_CLEARANCE:
                return False
    return True


def street_patrol(district):
    """(P0, P1, direction yaw) on the Avenue A sidewalk on the park side, PATROL_LENGTH apart.

    Centred on the point of the Avenue A centre line nearest the park's centroid, moved along
    the avenue until both points are clear of every crossing street."""
    park = district.parks[0] if district.parks else None
    if park is None:
        return None
    target = geo.centroid(district.ring_cm(park["outer"]))
    best = None
    for rec in district.roads:
        if rec.get("name") != PATROL_STREET:
            continue
        path = district.ring_cm(rec["pieces"][0]) if rec["pieces"] else []
        for i in range(len(path) - 1):
            hit = closest_point_on_polyline(target, path[i:i + 2])
            if hit and (best is None or hit[0] < best[0]):
                best = (hit[0], hit[1], path[i], path[i + 1], rec["width_m"])
    if best is None:
        return None
    _d, q, a, b, width_m = best
    length = math.hypot(b[0] - a[0], b[1] - a[1]) or 1.0
    d = ((b[0] - a[0]) / length, (b[1] - a[1]) / length)
    n = (-d[1], d[0])
    if (target[0] - q[0]) * n[0] + (target[1] - q[1]) * n[1] < 0.0:
        n = (-n[0], -n[1])
    offset = width_m * 50.0 + SIDEWALK_WIDTH * 0.5
    centre = (q[0] + n[0] * offset, q[1] + n[1] * offset)
    half = PATROL_LENGTH * 0.5
    for shift in (0.0, 500.0, -500.0, 1000.0, -1000.0, 1500.0, -1500.0, 2000.0, -2000.0):
        c0 = (centre[0] + d[0] * shift, centre[1] + d[1] * shift)
        p0 = (c0[0] - d[0] * half, c0[1] - d[1] * half)
        p1 = (c0[0] + d[0] * half, c0[1] + d[1] * half)
        if _clear_of_crossings(p0, district, PATROL_STREET) and _clear_of_crossings(p1, district, PATROL_STREET):
            return p0, p1, math.degrees(math.atan2(d[1], d[0]))
    return None


def thug_placements(district):
    """([(label, x, y, z, yaw, weapon, tags, [patrol labels])], [(label, x, y, z)], roof record)."""
    roof = roof_pair_spots(district)
    patrol = street_patrol(district)
    thugs, points = [], []
    roof_rec = None
    if roof is not None:
        a, b, roof_rec = roof
        for i, spot in enumerate((a, b)):
            weapon, tag = THUG_LOADOUT[i]
            thugs.append((THUG_PREFIX + str(i), spot[0], spot[1], spot[2], spot[3], weapon,
                          ["City", "CityThug", tag, "osm:" + roof_rec["id"]], []))
    if patrol is not None:
        p0, p1, yaw = patrol
        z_walk = SIDEWALK_TOP
        points = [(PATROL_PREFIX + "0", p0[0], p0[1], z_walk + PATROL_POINT_HEIGHT),
                  (PATROL_PREFIX + "1", p1[0], p1[1], z_walk + PATROL_POINT_HEIGHT)]
        ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        for j in range(2):
            weapon, tag = THUG_LOADOUT[2 + j]
            along = STREET_PAIR_SPACING * j
            thugs.append((THUG_PREFIX + str(2 + j), p0[0] + ux * along, p0[1] + uy * along,
                          z_walk + THUG_HALF_HEIGHT + 2.0, yaw, weapon,
                          ["City", "CityThug", tag, "street:" + PATROL_STREET],
                          [PATROL_PREFIX + "1", PATROL_PREFIX + "0"]))
    return thugs, points, roof_rec


def _ensure_located(existing, label, cls, loc, yaw):
    """(actor, changes): spawned if missing or of the wrong class, moved and turned if off."""
    changes = 0
    # A Python wrapper type (unreal.TargetPoint) never equals the UClass get_class() returns.
    if isinstance(cls, type):
        cls = cls.static_class()
    actor = existing.get(label)
    if actor is not None and actor.get_class() != cls:
        actor.destroy_actor()
        actor = None
    if actor is None:
        actor = c.spawn_actor(cls, loc, unreal.Rotator(0.0, 0.0, yaw), label=label)
        if actor is None:
            c.log("FAILED", label, "spawn_actor returned None")
            return None, 0
        existing[label] = actor
        changes += 1
    if not same_vector(actor.get_actor_location(), loc, 0.5):
        actor.set_actor_location(loc, False, True)
        changes += 1
    if abs(((actor.get_actor_rotation().yaw - yaw) + 180.0) % 360.0 - 180.0) > 0.05:
        actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
        changes += 1
    return actor, changes


def _ensure_thug_props(actor, weapon, patrol_actors):
    changes = 0
    wanted = getattr(unreal.ThugWeapon, weapon)
    if actor.get_editor_property("weapon") != wanted:
        actor.set_editor_property("weapon", wanted)
        changes += 1
    current = [a.get_actor_label() if a else "" for a in actor.get_editor_property("patrol_points")]
    if current != [a.get_actor_label() for a in patrol_actors]:
        actor.set_editor_property("patrol_points", patrol_actors)
        changes += 1
    return changes


def ensure_thugs(district, existing):
    """City_Thug_<n>, City_Patrol_<n> and City_ThugGroup_clear_roof. Idempotent by label."""
    thug_cls = c.load_generated_class(THUG_BP_PATH, THUG_BP_NAME)
    group_cls = c.find_class("ThugGroupObjective", "/Script/Castle.ThugGroupObjective")
    if thug_cls is None or group_cls is None or not hasattr(unreal, "ThugWeapon"):
        c.log("FAILED", THUG_PREFIX + "*", "BP_Thug or AThugGroupObjective missing; build and run create_world_blueprints.py")
        return 0
    thugs, points, roof_rec = thug_placements(district)
    if len(thugs) != len(THUG_LOADOUT) or len(points) != 2:
        c.log("FAILED", THUG_PREFIX + "*", "found {0} thug spots and {1} patrol points".format(len(thugs), len(points)))
    changes = 0
    wanted = set()
    patrol_actors = {}
    for label, x, y, z in points:
        actor, n = _ensure_located(existing, label, unreal.TargetPoint, unreal.Vector(x, y, z), 0.0)
        if actor is None:
            continue
        wanted.add(label)
        patrol_actors[label] = actor
        changes += n + _ensure_tags(actor, ["City", "CityPatrol", STREET_PAIR_TAG])
    for label, x, y, z, yaw, weapon, tags, patrol in thugs:
        actor, n = _ensure_located(existing, label, thug_cls, unreal.Vector(x, y, z), yaw)
        if actor is None:
            continue
        wanted.add(label)
        n += _ensure_thug_props(actor, weapon, [patrol_actors[p] for p in patrol if p in patrol_actors])
        n += _ensure_tags(actor, tags)
        changes += n
        c.log("updated" if n else "exists", label, "{0} {1} at ({2:.0f}, {3:.0f}, {4:.0f}) yaw {5:.0f}".format(
            weapon.lower(), tags[2], x, y, z, yaw))
    if roof_rec is not None:
        cx, cy = geo.centroid(roof_rec["ring"])
        loc = unreal.Vector(cx, cy, roof_rec["height_m"] * 100.0 + 100.0)
        actor, n = _ensure_located(existing, THUG_GROUP_LABEL, group_cls, loc, 0.0)
        if actor is not None:
            wanted.add(THUG_GROUP_LABEL)
            if str(actor.get_editor_property("objective_id")) != "clear_roof":
                actor.set_editor_property("objective_id", unreal.Name("clear_roof"))
                n += 1
            if str(actor.get_editor_property("group_tag")) != ROOF_PAIR_TAG:
                actor.set_editor_property("group_tag", unreal.Name(ROOF_PAIR_TAG))
                n += 1
            n += _ensure_tags(actor, ["City", "CityObjective", "osm:" + roof_rec["id"], "objective:clear_roof"])
            changes += n
    for label, actor in list(existing.items()):
        if (label.startswith(THUG_PREFIX) or label.startswith(PATROL_PREFIX)) and label not in wanted:
            actor.destroy_actor()
            existing.pop(label, None)
            changes += 1
            c.log("updated", label, "removed stray")
    c.log("updated" if changes else "exists", "chapter 1 thugs",
          "{0} thugs, {1} patrol points, roof osm {2}".format(
              len(thugs), len(points), roof_rec["id"] if roof_rec else "none"))
    return changes


# --------------------------------------------------------------------------------------
# street lamps
# --------------------------------------------------------------------------------------

LAMP_PREFIX = "City_Lamp_"          # the light; what verify counts
LAMP_POLE_PREFIX = "City_LampPole_"
LAMP_HEAD_PREFIX = "City_LampHead_"
LAMP_SPACING = 3000.0               # cm along each road, both sidewalks
LAMP_MIN_GAP = 800.0                # no two lamps closer than this (corners where roads meet)
LAMP_KERB_INSET = 60.0              # cm from the kerb into the sidewalk
LAMP_POLE_HEIGHT = 700.0
LAMP_POLE_DIAMETER = 14.0
LAMP_ARM = 90.0                     # cm the head reaches out over the road
LAMP_LUMENS = 2500.0
LAMP_RADIUS = 1800.0
LAMP_OUTER_CONE = 70.0              # degrees from straight down; the pool edge
LAMP_INNER_CONE = 35.0
LAMP_COLOR = (1.0, 0.75, 0.45)      # warm sodium
LAMP_EMISSIVE = 2.0                 # before m.EMISSIVE_INTENSITY_FACTOR; higher clips the head to white
LAMP_SHADOW_DISTANCE = 3500.0       # lamps this close to the park cast shadows; the rest don't
MI_STREET_LAMP = m.MATERIALS_PATH + "/MI_StreetLamp"
CYLINDER = "/Engine/BasicShapes/Cylinder"
CUBE = "/Engine/BasicShapes/Cube"


def lamp_spots(district):
    """[(x, y, yaw toward the road, casts shadows)] in a stable order."""
    roads = sorted(road_paths(district), key=lambda rp: rp[0]["id"])
    carriageway = [(p, q, rec["width_m"] * 50.0) for rec, paths in roads
                   for path in paths for p, q in zip(path, path[1:])]
    rings = []
    for rec in district.buildings:
        ring = district.ring_cm(rec["outer"])
        if len(ring) >= 3:
            rings.append((geo.bounds(ring), ring))
    ground = ground_ring_cm(district)
    parks = [district.ring_cm(p["outer"]) for p in district.parks]

    def in_carriageway(pt):
        return any(closest_point_on_polyline(pt, [p, q])[0] < half + 30.0 for p, q, half in carriageway)

    def in_building(pt):
        for (x0, y0, x1, y1), ring in rings:
            if x0 - 50 <= pt[0] <= x1 + 50 and y0 - 50 <= pt[1] <= y1 + 50 and ring_distance(pt, ring) < 50.0:
                return True
        return False

    spots = []
    for rec, paths in roads:
        offset = rec["width_m"] * 50.0 + LAMP_KERB_INSET
        for path in paths:
            s_next = LAMP_SPACING * 0.5
            walked = 0.0
            for (ax, ay), (bx, by) in zip(path, path[1:]):
                L = math.hypot(bx - ax, by - ay)
                if L < 1.0:
                    continue
                ux, uy = (bx - ax) / L, (by - ay) / L
                nx, ny = -uy, ux
                while s_next <= walked + L:
                    t = s_next - walked
                    px, py = ax + ux * t, ay + uy * t
                    for side in (1.0, -1.0):
                        pt = (px + nx * offset * side, py + ny * offset * side)
                        if not geo.point_in_polygon(pt, ground) or in_carriageway(pt) or in_building(pt):
                            continue
                        if any(math.hypot(pt[0] - s[0], pt[1] - s[1]) < LAMP_MIN_GAP for s in spots):
                            continue
                        yaw = math.degrees(math.atan2(-ny * side, -nx * side))
                        shadows = any(ring_distance(pt, park) <= LAMP_SHADOW_DISTANCE for park in parks)
                        spots.append((pt[0], pt[1], yaw, shadows))
                    s_next += LAMP_SPACING
                walked += L
    return spots


def ensure_lamp_materials():
    emissive = m.ensure_material(m.M_EMISSIVE, m._build_emissive)
    head = m.ensure_material_instance(
        MI_STREET_LAMP, emissive,
        vectors=[(m.EMISSIVE_COLOR_PARAM, LAMP_COLOR)],
        scalars=[(m.EMISSIVE_INTENSITY_PARAM, LAMP_EMISSIVE * m.EMISSIVE_INTENSITY_FACTOR)])
    pole = m.ensure_material(m.M_STEEL_PAINTED, m._build_steel_painted)
    return pole, head


def _ensure_tags(actor, tags):
    if [str(t) for t in actor.get_editor_property("tags")] != tags:
        actor.set_editor_property("tags", [unreal.Name(t) for t in tags])
        return 1
    return 0


def _ensure_mesh_actor(existing, label, mesh, material, loc, rot, scale, tags):
    changes = 0
    actor = existing.get(label)
    if actor is not None and not isinstance(actor, unreal.StaticMeshActor):
        actor.destroy_actor()
        actor = None
    if actor is None:
        actor = c.spawn_actor(unreal.StaticMeshActor, loc, rot, label=label)
        if actor is None:
            c.log("FAILED", label, "spawn_actor returned None")
            return 0
        existing[label] = actor
        changes += 1
    comp = actor.get_editor_property("static_mesh_component")
    if comp.get_editor_property("mobility") != unreal.ComponentMobility.STATIC:
        comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        changes += 1
    if comp.get_editor_property("static_mesh") != mesh:
        comp.set_static_mesh(mesh)
        changes += 1
    overrides = comp.get_editor_property("override_materials")
    if material is not None and (len(overrides) < 1 or overrides[0] != material):
        comp.set_material(0, material)
        changes += 1
    if not same_vector(actor.get_actor_location(), loc, 0.5):
        actor.set_actor_location(loc, False, True)
        changes += 1
    if abs(((actor.get_actor_rotation().yaw - rot.yaw) + 180.0) % 360.0 - 180.0) > 0.05:
        actor.set_actor_rotation(rot, False)
        changes += 1
    if not same_vector(actor.get_actor_scale3d(), scale, 1e-4):
        actor.set_actor_scale3d(scale)
        changes += 1
    return changes + _ensure_tags(actor, tags)


LAMP_DOWN = unreal.Rotator(0.0, -90.0, 0.0)   # roll, pitch, yaw: the spot points at the pavement


def _ensure_lamp_light(existing, label, loc, shadows):
    """A downward spot, not a point light: a point light under the head lights the wall above it
    and the head throws a hard dark wedge up the facade. UE rates spot lumens as if the light
    were a point light, so the pool is as bright as the 2500 lm point light it replaced."""
    changes = 0
    actor = existing.get(label)
    if actor is not None and not isinstance(actor, unreal.SpotLight):
        actor.destroy_actor()
        actor = None
    if actor is None:
        actor = c.spawn_actor(unreal.SpotLight, loc, LAMP_DOWN, label=label)
        if actor is None:
            c.log("FAILED", label, "spawn_actor returned None")
            return 0
        existing[label] = actor
        changes += 1
    if not same_vector(actor.get_actor_location(), loc, 0.5):
        actor.set_actor_location(loc, False, True)
        changes += 1
    if abs(actor.get_actor_rotation().pitch - LAMP_DOWN.pitch) > 0.05:
        actor.set_actor_rotation(LAMP_DOWN, False)
        changes += 1
    comp = actor.get_editor_property("spot_light_component")
    changes += set_if_different(comp, "mobility", unreal.ComponentMobility.MOVABLE, label)
    changes += set_if_different(comp, "intensity_units", unreal.LightUnits.LUMENS, label)
    changes += set_if_different(comp, "intensity", LAMP_LUMENS, label, 0.5)
    changes += set_if_different(comp, "attenuation_radius", LAMP_RADIUS, label, 0.5)
    changes += set_if_different(comp, "outer_cone_angle", LAMP_OUTER_CONE, label, 0.01)
    changes += set_if_different(comp, "inner_cone_angle", LAMP_INNER_CONE, label, 0.01)
    changes += set_if_different(comp, "cast_shadows", bool(shadows), label)
    changes += set_if_different(comp, "use_temperature", False, label)
    want = unreal.Color(r=int(LAMP_COLOR[0] * 255), g=int(LAMP_COLOR[1] * 255), b=int(LAMP_COLOR[2] * 255), a=255)
    have = comp.get_editor_property("light_color")
    if (have.r, have.g, have.b) != (want.r, want.g, want.b):
        comp.set_editor_property("light_color", want)
        changes += 1
    return changes + _ensure_tags(actor, ["City", "CityLamp", "shadows:{0}".format(int(bool(shadows)))])


def ensure_street_lamps(district, existing):
    """A lamp every LAMP_SPACING along both sidewalks of every road: pole, emissive head, and a
    movable warm spot light pointing down. Only the lamps around the park cast shadows."""
    cylinder = c.load_or_none(CYLINDER)
    cube = c.load_or_none(CUBE)
    if cylinder is None or cube is None:
        c.log("FAILED", "street lamps", "engine basic shapes not found")
        return 0
    pole_mat, head_mat = ensure_lamp_materials()
    spots = lamp_spots(district)
    changes = 0
    changed_lamps = 0
    tags = ["City", "CityLamp"]
    for i, (x, y, yaw, shadows) in enumerate(spots):
        rad = math.radians(yaw)
        ux, uy = math.cos(rad), math.sin(rad)
        rot = unreal.Rotator(0.0, 0.0, yaw)
        n = _ensure_mesh_actor(
            existing, LAMP_POLE_PREFIX + str(i), cylinder, pole_mat,
            unreal.Vector(x, y, SIDEWALK_TOP + LAMP_POLE_HEIGHT * 0.5), rot,
            unreal.Vector(LAMP_POLE_DIAMETER / 100.0, LAMP_POLE_DIAMETER / 100.0, LAMP_POLE_HEIGHT / 100.0), tags)
        n += _ensure_mesh_actor(
            existing, LAMP_HEAD_PREFIX + str(i), cube, head_mat,
            unreal.Vector(x + ux * LAMP_ARM * 0.5, y + uy * LAMP_ARM * 0.5, SIDEWALK_TOP + LAMP_POLE_HEIGHT - 7.5),
            rot, unreal.Vector((LAMP_ARM + LAMP_POLE_DIAMETER) / 100.0, 0.35, 0.15), tags)
        n += _ensure_lamp_light(
            existing, LAMP_PREFIX + str(i),
            unreal.Vector(x + ux * LAMP_ARM * 0.75, y + uy * LAMP_ARM * 0.75, SIDEWALK_TOP + LAMP_POLE_HEIGHT - 30.0),
            shadows)
        if n:
            changed_lamps += 1
        changes += n

    # Lamps past the end of the list (the roads changed) go.
    removed = 0
    for label, actor in list(existing.items()):
        for prefix in (LAMP_PREFIX, LAMP_POLE_PREFIX, LAMP_HEAD_PREFIX):
            rest = label[len(prefix):] if label.startswith(prefix) else ""
            if rest.isdigit() and int(rest) >= len(spots):
                actor.destroy_actor()
                existing.pop(label, None)
                removed += 1
                break
    changes += removed
    c.log("updated" if (changed_lamps or removed) else "exists", "street lamps",
          "{0} lamps, {1} casting shadows near the park, {2} changed, {3} actor(s) removed".format(
              len(spots), sum(1 for s in spots if s[3]), changed_lamps, removed))
    return changes


# --------------------------------------------------------------------------------------
# grapple anchors
# --------------------------------------------------------------------------------------

ANCHOR_PREFIX = "City_Anchor_"
ANCHOR_BP_PATH = "/Game/Blueprints/World"
ANCHOR_BP_NAME = "BP_GrappleAnchor"
ANCHOR_MIN_HEIGHT_M = 8.0         # only buildings taller than this get anchors
ANCHOR_MIN_GAP = 400.0            # cm; an anchor this close to one already placed is skipped
ANCHOR_LONG_EDGE = 2500.0         # cm; edges longer than this also get one at the middle
ANCHOR_CORNER_TURN = 30.0         # degrees the outline has to turn for a vertex to be a corner
ANCHOR_MIN_INTERIOR = 45.0        # degrees; sharper corners put the landing too far in
ANCHOR_LANDING_INBOARD = 60.0     # cm from the anchor to the landing point, square to each edge
ANCHOR_LANDING_CLEARANCE = 70.0   # cm from any roof edge: the parapet (30) plus Kate's capsule (34)
ANCHOR_BURIED_DISTANCE = 40.0     # cm; an anchor this close to a taller neighbour is in its wall


def _edge_distance(pt, ring):
    return closest_point_on_polyline(pt, list(ring) + [ring[0]])[0]


def anchor_spots(district):
    """[(x, y, z, yaw, landing_forward, landing_drop, osm id)] in a stable order.

    One anchor per roof corner and one mid-edge on edges over ANCHOR_LONG_EDGE, for every
    building over ANCHOR_MIN_HEIGHT_M. Each sits centred on the parapet (half its thickness in
    from the edge) on the parapet top; +X points inboard along the corner bisector or the edge
    normal. The landing point is ANCHOR_LANDING_INBOARD further in, square to the edges (so on
    a corner it is 60 / sin(half the corner angle) along the bisector), down on the roof.
    Anchors closer than ANCHOR_MIN_GAP to an earlier one, buried in a taller neighbour's wall,
    or whose landing point is not clear of the roof edges, are skipped.
    """
    buildings = []
    for rec in sorted(district.buildings, key=lambda r: r["id"]):
        ring = geo.clean_ring(district.ring_cm(rec["outer"]), min_edge=5.0, collinear_tol=2.0)
        if len(ring) >= 3:
            buildings.append((rec, ring, geo.bounds(ring)))

    inset = PARAPET_THICK * 0.5
    placed = []
    grid = {}

    def near_placed(x, y):
        gx, gy = int(math.floor(x / ANCHOR_MIN_GAP)), int(math.floor(y / ANCHOR_MIN_GAP))
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for px, py in grid.get((gx + dx, gy + dy), ()):
                    if math.hypot(px - x, py - y) < ANCHOR_MIN_GAP:
                        return True
        return False

    def buried(x, y, roof_top, own_id):
        for rec, ring, (x0, y0, x1, y1) in buildings:
            if rec["id"] == own_id or rec["height_m"] * 100.0 <= roof_top:
                continue
            pad = ANCHOR_BURIED_DISTANCE
            if x0 - pad <= x <= x1 + pad and y0 - pad <= y <= y1 + pad and ring_distance((x, y), ring) < pad:
                return True
        return False

    for rec, ring, _box in buildings:
        if rec["height_m"] <= ANCHOR_MIN_HEIGHT_M:
            continue
        height_cm = rec["height_m"] * 100.0
        parapet = rec["height_m"] >= PARAPET_MIN_HEIGHT_M
        drop = PARAPET_HEIGHT if parapet else 0.0
        top = height_cm + drop
        inward = 1.0 if geo.is_ccw(ring) else -1.0
        n = len(ring)
        candidates = []   # (anchor x, y, unit inboard x, y, distance anchor->landing)
        for i in range(n):
            px, py = ring[i - 1]
            cx, cy = ring[i]
            nx_, ny_ = ring[(i + 1) % n]
            ax, ay = cx - px, cy - py
            bx, by = nx_ - cx, ny_ - cy
            la, lb = math.hypot(ax, ay), math.hypot(bx, by)
            if la < 1.0 or lb < 1.0:
                continue
            ax, ay, bx, by = ax / la, ay / la, bx / lb, by / lb
            turn = math.degrees(math.atan2(ax * by - ay * bx, ax * bx + ay * by)) * inward
            interior = 180.0 - turn
            if turn >= ANCHOR_CORNER_TURN and interior >= ANCHOR_MIN_INTERIOR:
                # Inward normals of both edges; their sum is the bisector.
                ux, uy = -ay * inward - by * inward, ax * inward + bx * inward
                ul = math.hypot(ux, uy)
                s = math.sin(math.radians(interior) * 0.5)
                if ul > 1e-6 and s > 1e-3:
                    ux, uy = ux / ul, uy / ul
                    candidates.append((cx + ux * inset / s, cy + uy * inset / s, ux, uy,
                                       ANCHOR_LANDING_INBOARD / s))
        for i in range(n):
            ax, ay = ring[i]
            bx, by = ring[(i + 1) % n]
            length = math.hypot(bx - ax, by - ay)
            if length <= ANCHOR_LONG_EDGE:
                continue
            ux, uy = -(by - ay) / length * inward, (bx - ax) / length * inward
            mx, my = (ax + bx) * 0.5, (ay + by) * 0.5
            candidates.append((mx + ux * inset, my + uy * inset, ux, uy, ANCHOR_LANDING_INBOARD))

        for x, y, ux, uy, forward in candidates:
            lx, ly = x + ux * forward, y + uy * forward
            if not geo.point_in_polygon((lx, ly), ring) or _edge_distance((lx, ly), ring) < ANCHOR_LANDING_CLEARANCE:
                continue
            if near_placed(x, y) or buried(x, y, top, rec["id"]):
                continue
            placed.append((x, y, top, math.degrees(math.atan2(uy, ux)), forward, drop, rec["id"]))
            key = (int(math.floor(x / ANCHOR_MIN_GAP)), int(math.floor(y / ANCHOR_MIN_GAP)))
            grid.setdefault(key, []).append((x, y))
    return placed


def anchor_class():
    cls = c.load_generated_class(ANCHOR_BP_PATH, ANCHOR_BP_NAME)
    if cls is None:
        c.log("skipped", ANCHOR_BP_NAME, "not found; falling back to AGrappleAnchor (run create_world_blueprints.py)")
        cls = c.find_class("GrappleAnchor", "/Script/Castle.GrappleAnchor")
    return cls


def _ensure_anchor(existing, label, cls, spot):
    x, y, z, yaw, forward, drop, osm = spot
    loc = unreal.Vector(x, y, z)
    rot = unreal.Rotator(0.0, 0.0, yaw)
    changes = 0
    actor = existing.get(label)
    if actor is not None and actor.get_class() != cls:
        actor.destroy_actor()
        actor = None
    if actor is None:
        actor = c.spawn_actor(cls, loc, rot, label=label)
        if actor is None:
            c.log("FAILED", label, "spawn_actor returned None")
            return 0
        existing[label] = actor
        changes += 1
    if not same_vector(actor.get_actor_location(), loc, 0.5):
        actor.set_actor_location(loc, False, True)
        changes += 1
    if abs(((actor.get_actor_rotation().yaw - yaw) + 180.0) % 360.0 - 180.0) > 0.05:
        actor.set_actor_rotation(rot, False)
        changes += 1
    landing = actor.get_landing_point()
    want = unreal.Vector(forward, 0.0, -drop)
    if landing is not None and not same_vector(landing.get_editor_property("relative_location"), want, 0.5):
        landing.set_editor_property("relative_location", want)
        changes += 1
    return changes + _ensure_tags(actor, ["City", "CityAnchor", "osm:" + osm])


def ensure_grapple_anchors(district, existing):
    """City_Anchor_<n>: BP_GrappleAnchor on every tall roof's corners and long edges."""
    cls = anchor_class()
    if cls is None:
        c.log("FAILED", ANCHOR_PREFIX + "*", "no grapple anchor class; build the module")
        return 0
    spots = anchor_spots(district)
    changes = 0
    changed_anchors = 0
    for i, spot in enumerate(spots):
        n = _ensure_anchor(existing, ANCHOR_PREFIX + str(i), cls, spot)
        if n:
            changed_anchors += 1
        changes += n

    removed = 0
    for label, actor in list(existing.items()):
        rest = label[len(ANCHOR_PREFIX):] if label.startswith(ANCHOR_PREFIX) else ""
        if rest.isdigit() and int(rest) >= len(spots):
            actor.destroy_actor()
            existing.pop(label, None)
            removed += 1
    changes += removed
    c.log("updated" if (changed_anchors or removed) else "exists", "grapple anchors",
          "{0} anchors on {1} roofs over {2:.0f} m, {3} changed, {4} removed".format(
              len(spots), len({s[6] for s in spots}), ANCHOR_MIN_HEIGHT_M, changed_anchors, removed))
    return changes


# --------------------------------------------------------------------------------------
# traversable ledges and parkour test obstacles
# --------------------------------------------------------------------------------------

# The Game Animation Sample's traversal (AC_TraversalLogic on BP_Kate) sweeps forward on its
# Traversable trace channel - our first custom channel - and only acts on a LevelBlock_Traversable
# it hits. That block is a 1 m cube (pivot at a bottom corner) with four ledge splines on its top
# edges, Ledge_1 along local y = 0 facing -Y and Ledge_2 opposite it along y = 100; a spline's up
# vector is the ledge's outward normal. So each roof edge gets one, scaled into a slab: local X
# along the edge, local -Y out of the facade, the top at the parapet top. Ledge_1 is then the
# outer top edge of the parapet (corner to corner), Ledge_2 its inner edge, and the short ends are
# narrower than the sample's 60 cm minimum ledge width. The slab is hidden, stands LEDGE_OUTSET
# proud of the facade so the sweep meets it before the wall, and blocks only that channel.
LEDGE_PREFIX = "City_Ledge_"
# BP_TraversableBlock (create_world_blueprints.py) is LevelBlock_Traversable with the sample's
# level-style lookup switched off; the sample's cast accepts it.
TRAVERSABLE_PATH = "/Game/Blueprints/World"
TRAVERSABLE_NAME = "BP_TraversableBlock"
LEDGE_MIN_EDGE = 100.0      # cm; shorter roof edges get no ledge (the sample wants 60 cm of it)
LEDGE_OUTSET = 2.0          # cm the slab stands out from the facade
LEDGE_SLAB_HEIGHT = 400.0   # cm down from the top; covers a step up from a lower neighbour's roof
LEDGE_SPLINE = "Ledge_1"

TEST_VAULT_LABEL = "City_Test_Vault"
TEST_MANTLE_LABEL = "City_Test_Mantle"
PARK_WALL_PREFIX = "City_ParkWall_"
TEST_BLOCKS = (
    # label, tag, along the street from the PlayerStart (cm), width, depth, height
    (TEST_VAULT_LABEL, "CityTestVault", 700.0, 250.0, 60.0, 90.0),
    (TEST_MANTLE_LABEL, "CityTestMantle", -700.0, 250.0, 150.0, 150.0),
)
PARK_WALLS = (
    # in from the park edge nearest the PlayerStart (cm), width, depth, height
    (600.0, 400.0, 40.0, 70.0),
    (1200.0, 400.0, 40.0, 90.0),
    (1800.0, 400.0, 40.0, 100.0),
)


def traversable_class():
    return c.load_generated_class(TRAVERSABLE_PATH, TRAVERSABLE_NAME)


def traversable_channel():
    """ECC_GameTraceChannel1 as Python names it: by its display name, which in this project is
    Weapon (the sample calls the same channel Traversable)."""
    for name in ("ECC_WEAPON", "ECC_GAME_TRACE_CHANNEL1", "ECC_TRAVERSABLE"):
        if hasattr(unreal.CollisionChannel, name):
            return getattr(unreal.CollisionChannel, name)
    return None


def ledge_spots(district):
    """[(label, osm id, origin, yaw, scale, outer corner a, outer corner b, hash)] in a stable order.

    One per roof edge of LEDGE_MIN_EDGE or more, on every building. ``origin`` is the slab's
    bottom corner; ``a`` and ``b`` are where Ledge_1 should end: on the parapet top, LEDGE_OUTSET
    outside the facade line, at the two ends of the edge.
    """
    spots = []
    for rec in sorted(district.buildings, key=lambda r: r["id"]):
        ring = geo.clean_ring(district.ring_cm(rec["outer"]), min_edge=5.0, collinear_tol=2.0)
        if len(ring) < 3:
            continue
        height_cm = rec["height_m"] * 100.0
        parapet = rec["height_m"] >= PARAPET_MIN_HEIGHT_M
        top = height_cm + (PARAPET_HEIGHT if parapet else 0.0)
        slab = min(top, LEDGE_SLAB_HEIGHT)
        depth = PARAPET_THICK + LEDGE_OUTSET
        inward = 1.0 if geo.is_ccw(ring) else -1.0
        n = len(ring)
        for i in range(n):
            ax, ay = ring[i]
            bx, by = ring[(i + 1) % n]
            length = math.hypot(bx - ax, by - ay)
            if length < LEDGE_MIN_EDGE:
                continue
            nx, ny = -(by - ay) / length * inward, (bx - ax) / length * inward
            ux, uy = ny, -nx                       # local X: turning it +90 degrees gives the inward normal
            if (ax * ux + ay * uy) > (bx * ux + by * uy):
                ax, ay, bx, by = bx, by, ax, ay
            ox, oy = ax - nx * LEDGE_OUTSET, ay - ny * LEDGE_OUTSET
            corner_a = (ox, oy, top)
            corner_b = (bx - nx * LEDGE_OUTSET, by - ny * LEDGE_OUTSET, top)
            yaw = math.degrees(math.atan2(uy, ux))
            scale = (length / 100.0, depth / 100.0, slab / 100.0)
            origin = (ox, oy, top - slab)
            spec = geo.record_hash(GENERATOR_VERSION, "ledge", rec["id"], i, list(origin), yaw, list(scale))
            spots.append((LEDGE_PREFIX + "{0}_{1}".format(rec["id"], i), rec["id"], origin, yaw, scale,
                          corner_a, corner_b, spec))
    return spots


def _set_components(actor, visible, traversable_only, context):
    """Hide the block's height labels (and, for ledges, the block), and for ledges block only the
    Traversable channel so nothing but the sample's sweep ever meets them."""
    changes = 0
    for text in actor.get_components_by_class(unreal.TextRenderComponent):
        changes += set_if_different(text, "visible", False, context)
    channel = traversable_channel()
    for mesh in actor.get_components_by_class(unreal.StaticMeshComponent):
        changes += set_if_different(mesh, "visible", bool(visible), context)
        if not traversable_only or channel is None:
            continue
        if mesh.get_collision_enabled() != unreal.CollisionEnabled.QUERY_ONLY:
            mesh.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
            changes += 1
        pawn = mesh.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)
        trace = mesh.get_collision_response_to_channel(channel)
        seen = mesh.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY)
        if pawn != unreal.CollisionResponseType.ECR_IGNORE or seen != unreal.CollisionResponseType.ECR_IGNORE \
                or trace != unreal.CollisionResponseType.ECR_BLOCK:
            _set_trace_only_body(mesh, context)
            changes += 1
    return changes


LEDGE_IGNORED_CHANNELS = ("WorldStatic", "WorldDynamic", "Pawn", "Visibility", "Camera", "PhysicsBody",
                          "Vehicle", "Destructible")
LEDGE_BLOCKED_CHANNEL = "Weapon"   # ECC_GameTraceChannel1, the sample's Traversable


def _response_channel(name, response):
    entry = unreal.ResponseChannel()
    entry.set_editor_property("channel", name)
    entry.set_editor_property("response", response)
    return entry


def _set_trace_only_body(mesh, context):
    """Query only, ignore everything but the Traversable channel. Written through the body
    instance property (not the runtime setters) so it is recorded as an instance edit and
    survives the construction script re-running when the map loads."""
    try:
        body = mesh.get_editor_property("body_instance")
        body.set_editor_property("collision_profile_name", "Custom")
        body.set_editor_property("collision_enabled", unreal.CollisionEnabled.QUERY_ONLY)
        body.set_editor_property("object_type", unreal.CollisionChannel.ECC_WORLD_STATIC)
        responses = body.get_editor_property("collision_responses")
        ignore = unreal.CollisionResponseType.ECR_IGNORE
        array = [_response_channel(name, ignore) for name in LEDGE_IGNORED_CHANNELS]
        array.append(_response_channel(LEDGE_BLOCKED_CHANNEL, unreal.CollisionResponseType.ECR_BLOCK))
        responses.set_editor_property("response_array", array)
        body.set_editor_property("collision_responses", responses)
        mesh.set_editor_property("body_instance", body)
    except Exception as exc:  # noqa: BLE001
        c.log_error("collision " + context, exc)
    # And the live component, so the check above reads the new responses straight away.
    mesh.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
    mesh.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    mesh.set_collision_response_to_channel(traversable_channel(), unreal.CollisionResponseType.ECR_BLOCK)


def _ensure_block(existing, label, cls, origin, yaw, scale, tags, visible, traversable_only):
    """A BP_TraversableBlock at origin (its bottom corner), turned yaw, scaled. Returns changes."""
    loc = unreal.Vector(*origin)
    rot = unreal.Rotator(0.0, 0.0, yaw)
    want_scale = unreal.Vector(*scale)
    changes = 0
    actor = existing.get(label)
    if actor is not None and actor.get_class() != cls:
        actor.destroy_actor()
        actor = None
    if actor is None:
        actor = c.spawn_actor(cls, loc, rot, label=label)
        if actor is None:
            c.log("FAILED", label, "spawn_actor returned None")
            return 0
        existing[label] = actor
        changes += 1
    if not same_vector(actor.get_actor_location(), loc, 0.05):
        actor.set_actor_location(loc, False, True)
        changes += 1
    if abs(((actor.get_actor_rotation().yaw - yaw) + 180.0) % 360.0 - 180.0) > 0.01:
        actor.set_actor_rotation(rot, False)
        changes += 1
    if not same_vector(actor.get_actor_scale3d(), want_scale, 1e-4):
        actor.set_actor_scale3d(want_scale)
        changes += 1
    changes += _set_components(actor, visible, traversable_only, label)
    if actor.get_actor_label() != label:
        actor.set_actor_label(label)
        changes += 1
    return changes + _ensure_tags(actor, tags)


def ensure_ledges(district, existing):
    """City_Ledge_<osm id>_<edge>: a hidden BP_TraversableBlock along every roof edge."""
    cls = traversable_class()
    if cls is None:
        c.log("FAILED", LEDGE_PREFIX + "*",
              "BP_TraversableBlock not found; run Tools\\create-content.ps1 (import_gasp, world blueprints)")
        return 0
    if traversable_channel() is None:
        c.log("FAILED", LEDGE_PREFIX + "*", "no Python name for ECC_GameTraceChannel1")
        return 0
    spots = ledge_spots(district)
    wanted = set()
    changes = 0
    changed = 0
    for label, osm, origin, yaw, scale, _a, _b, spec in spots:
        wanted.add(label)
        n = _ensure_block(existing, label, cls, origin, yaw, scale,
                          ["City", "CityLedge", "osm:" + osm, "ledgehash:" + spec], False, True)
        if n:
            changed += 1
        changes += n
    removed = 0
    for label, actor in list(existing.items()):
        if label.startswith(LEDGE_PREFIX) and label not in wanted:
            actor.destroy_actor()
            existing.pop(label, None)
            removed += 1
    c.log("updated" if (changed or removed) else "exists", "traversable ledges",
          "{0} ledges on {1} buildings, {2} changed, {3} removed".format(
              len(spots), len({s[1] for s in spots}), changed, removed))
    return changes + removed


_GROUND_PREFIXES = (ROAD_PREFIX, SIDEWALK_PREFIX, PARK_PREFIX, GROUND_LABEL)
_GROUND_MESHES = {}


def _mesh_top_under(actor, x, y):
    """World Z where a ray straight down at (x, y) first meets actor's static mesh, or None.

    A commandlet's editor world has no physics scene, so a collision trace always misses; this
    casts against the mesh triangles with Geometry Script, like verify_city's MeshProbe."""
    static_mesh = actor.get_editor_property("static_mesh_component").get_editor_property("static_mesh")
    if static_mesh is None:
        return None
    key = static_mesh.get_path_name()
    if key not in _GROUND_MESHES:
        mesh = unreal.DynamicMesh()
        unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(
            static_mesh, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD(), True)
        result = unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(mesh)
        bvh = next((r for r in (result if isinstance(result, tuple) else (result,))
                    if isinstance(r, unreal.GeometryScriptDynamicMeshBVH)), None)
        _GROUND_MESHES[key] = (mesh, bvh)
    mesh, bvh = _GROUND_MESHES[key]
    if bvh is None:
        return None
    top = 5000.0
    origin = unreal.Vector(x, y, top) - actor.get_actor_location()
    result = unreal.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(
        mesh, bvh, origin, unreal.Vector(0.0, 0.0, -1.0), unreal.GeometryScriptSpatialQueryOptions())
    hit = next((r for r in (result if isinstance(result, tuple) else (result,))
                if isinstance(r, unreal.GeometryScriptRayHitResult)), None)
    if hit is None or not hit.hit:
        return None
    return top - hit.ray_parameter


def ground_z(x, y, fallback, existing):
    """Top of the street, sidewalk, park or ground slab under (x, y); fallback when none is. The
    kerbs make a guessed height wrong by the 13 cm between road and sidewalk."""
    best = None
    for label, actor in existing.items():
        if not label.startswith(_GROUND_PREFIXES):
            continue
        origin, extent = actor.get_actor_bounds(False)
        if abs(x - origin.x) > extent.x or abs(y - origin.y) > extent.y:
            continue
        z = _mesh_top_under(actor, x, y)
        if z is not None and (best is None or z > best):
            best = z
    return best if best is not None else fallback


def test_block_spots(district, existing=None):
    """[(label, tag, origin, yaw, scale)] for the street test blocks and the park walls. With
    ``existing`` (label -> actor) each block stands on the ground traced under its centre."""
    loc, rot = player_start_transform(district)
    yaw = rot.yaw
    across = (math.cos(math.radians(yaw)), math.sin(math.radians(yaw)))    # out of the park, across the street
    along = (-across[1], across[0])                                          # along the street: local +Y
    def base(x, y, fallback):
        return ground_z(x, y, fallback, existing) if existing is not None else fallback

    spots = []
    for label, tag, offset, width, depth, height in TEST_BLOCKS:
        cx, cy = loc.x + along[0] * offset, loc.y + along[1] * offset
        ox = cx - across[0] * width * 0.5 - along[0] * depth * 0.5
        oy = cy - across[1] * width * 0.5 - along[1] * depth * 0.5
        spots.append((label, tag, (ox, oy, base(cx, cy, ROAD_TOP)), yaw,
                      (width / 100.0, depth / 100.0, height / 100.0)))

    park = district.parks[0] if district.parks else None
    if park is not None:
        ring = district.ring_cm(park["outer"])
        edge = closest_point_on_polyline((loc.x, loc.y), list(ring) + [ring[0]])[1]
        cx, cy = geo.centroid(ring)
        dx, dy = cx - edge[0], cy - edge[1]
        dl = math.hypot(dx, dy) or 1.0
        inward = (dx / dl, dy / dl)                                          # local +Y
        xaxis = (inward[1], -inward[0])
        wall_yaw = math.degrees(math.atan2(xaxis[1], xaxis[0]))
        for i, (distance, width, depth, height) in enumerate(PARK_WALLS):
            px, py = edge[0] + inward[0] * distance, edge[1] + inward[1] * distance
            ox = px - xaxis[0] * width * 0.5 - inward[0] * depth * 0.5
            oy = py - xaxis[1] * width * 0.5 - inward[1] * depth * 0.5
            spots.append((PARK_WALL_PREFIX + str(i), "CityParkWall", (ox, oy, base(px, py, PARK_TOP)), wall_yaw,
                          (width / 100.0, depth / 100.0, height / 100.0)))
    return spots


def ensure_test_blocks(district, existing):
    """City_Test_Vault (90 cm) and City_Test_Mantle (150 cm) on the street by the PlayerStart, and
    City_ParkWall_<n> low walls in the park: visible BP_TraversableBlocks to vault and mantle."""
    cls = traversable_class()
    if cls is None:
        c.log("FAILED", "parkour test blocks", "BP_TraversableBlock not found")
        return 0
    spots = test_block_spots(district, existing)
    changes = 0
    for label, tag, origin, yaw, scale in spots:
        changes += _ensure_block(existing, label, cls, origin, yaw, scale, ["City", "CityParkour", tag], True, False)
    c.log("updated" if changes else "exists", "parkour test blocks",
          "{0} blocks: {1}".format(len(spots), ", ".join(
              "{0} {1:.0f} cm on z {2:.0f}".format(s[0], s[4][2] * 100.0, s[2][2]) for s in spots)))
    return changes


# --------------------------------------------------------------------------------------
# level
# --------------------------------------------------------------------------------------


def open_or_create_map():
    """(ok, created)."""
    subsystem = c.level_editor_subsystem()
    if c.exists(MAP_PATH):
        return bool(subsystem.load_level(MAP_PATH)), False
    c.ensure_directory("/Game/Maps")
    try:
        ok = subsystem.new_level(MAP_PATH, False)   # is_partitioned_world = False
    except TypeError:
        ok = subsystem.new_level(MAP_PATH)
    if not ok:
        c.log("FAILED", MAP_PATH, "new_level returned false")
    return bool(ok), bool(ok)


def ensure_materials():
    materials = {
        "greybox": c.ensure_constant_color_material("M_Greybox", KIT_MATERIALS, (0.20, 0.21, 0.22), 0.9),
        "floor": c.ensure_constant_color_material("M_Greybox_Floor", KIT_MATERIALS, (0.12, 0.13, 0.14), 0.9),
        "asphalt": c.ensure_constant_color_material("M_Asphalt", m.MATERIALS_PATH, (0.05, 0.05, 0.05), 0.9),
        "grass": c.ensure_constant_color_material("M_Grass", m.MATERIALS_PATH, (0.05, 0.12, 0.04), 0.95),
        "sidewalk": m.ensure_material(m.M_CONCRETE_FLOOR, m._build_concrete_floor),
    }
    return materials


def run():
    if not data_available():
        c.log("skipped", MAP_PATH, "no OSM data; run Tools\\fetch-osm.ps1 first")
        return False

    district = District()
    materials = ensure_materials()

    ok, created = open_or_create_map()
    if not ok:
        return False

    pieces = building_pieces(district, materials["greybox"]) + street_pieces(district, materials)
    existing = actors_by_label()

    changes = remove_stale(pieces, existing)
    mesh_changes, actor_changes = build_pieces(pieces, existing)
    changes += actor_changes
    changes += ensure_lighting(existing)
    changes += ensure_player_start(district, existing)
    changes += ensure_nav_volume(district, existing)
    changes += ensure_game_mode()
    changes += remove_prison_actors(existing)
    changes += ensure_objective_volumes(district, existing)
    changes += ensure_thugs(district, existing)
    changes += ensure_street_lamps(district, existing)
    changes += ensure_grapple_anchors(district, existing)
    changes += ensure_ledges(district, existing)
    changes += ensure_test_blocks(district, existing)

    if created or changes:
        c.level_editor_subsystem().save_current_level()
        c.log("created" if created else "updated", MAP_PATH,
              "{0} buildings, {1} pieces total, {2} mesh rebuild(s), {3} level change(s)".format(
                  len(district.buildings), len(pieces), mesh_changes, changes))
    else:
        c.log("exists", MAP_PATH, "nothing changed ({0} mesh rebuild(s)); level not saved".format(mesh_changes))
    return True


if __name__ == "__main__":
    run()
    c.print_summary("city")
