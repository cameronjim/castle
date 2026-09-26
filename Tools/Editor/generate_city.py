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
  from Tompkins Square Park, a NavMeshBoundsVolume, and the BP_CastleGameMode override.

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
SUN_TEMPERATURE = 3600.0
SKY_INTENSITY = 1.0
FOG_DENSITY = 0.008
EXPOSURE_EV = 0.0


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
                            ("auto_exposure_min_brightness", EXPOSURE_EV),
                            ("auto_exposure_max_brightness", EXPOSURE_EV)):
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


def ensure_game_mode():
    game_mode = c.load_generated_class(PLAYER_PATH, "BP_CastleGameMode")
    if game_mode is None:
        c.log("skipped", MAP_PATH, "BP_CastleGameMode_C not found; GameMode override unset")
        return 0
    ws = c.world_settings()
    if ws is None:
        return 0
    if ws.get_editor_property("default_game_mode") == game_mode:
        return 0
    ws.set_editor_property("default_game_mode", game_mode)
    c.log("updated", MAP_PATH, "GameMode override = BP_CastleGameMode_C")
    return 1


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
