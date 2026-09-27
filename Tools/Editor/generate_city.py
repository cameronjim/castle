"""Generate the greybox East Village district from OpenStreetMap data.

    /Game/Maps/L_District_EastVillage          the map (World Partition off for one block)
    /Game/City/EastVillage/Meshes/SM_City_*    one static mesh per building, road, sidewalk

Input is what Tools/fetch-osm.ps1 wrote: Tools/Data/osm/east_village.buildings.json and
east_village.streets.json (lat/lon; this script projects them with _geo.py).

What gets built, all with Geometry Script from Python (the GeometryScripting plugin):

* every building: the footprint extruded to its height (append_simple_extrude_polygon, ear
  clipped so concave footprints work; courtyard holes are ignored), a 30 x 90 cm parapet ring
  just inside the roof edge, per-face normals, complex-as-simple collision so the player can
  stand on roofs and walk into walls. Actor ``City_Bldg_<id>`` in the MI_Facade_<style> that
  facade_style picks (brick or brownstone tenements, stone over 30 m, painted grey garages): M_Facade
  draws the window grid, storefronts, cornice and lit windows from world position, snow on top.
* one ground slab under the whole district at z = 0 (``City_Ground``, snowy MI_Prop_Ground).
* per named road way: carriageway at z = +2 cm (``City_Road_<id>``, wet M_StreetAsphalt) and the 4 m
  sidewalks either side at z = +15 cm (``City_Sidewalk_<id>``, M_SidewalkSnow). Sidewalks are
  the road's buffer minus every carriageway, so they stop at the kerb of each crossing street.
* the park at z = +5 cm (``City_Park_<id>``, M_ParkSnow), minus the carriageways.
* a January night: a 0.3 lux blue moon (``Moon``, the sky atmosphere's light) 30 degrees up, the sky
  light, the height fog as night haze, stars on ``City_NightSky`` (M_NightStars on the engine
  sphere), fixed exposure and a purple-shadow, cream-highlight grade with a vignette; a PlayerStart on East 7th Street facing the tenement row across
  from Tompkins Square Park, a NavMeshBoundsVolume.
* street lamps every 30 m on both sidewalks of every road (``City_Lamp_<n>`` spot light plus
  ``City_LampPole_<n>`` and ``City_LampHead_<n>``); only the ones around the park cast shadows.
  The 20 nearest the park hum: ``City_LampBuzz_<n>`` (AAmbientSound, MS_Amb_LampBuzz, tag CityLamp so
  the EMP silences it too), 3 m up the pole.
* ``City_Ambience`` (AHawkeyeAmbience) at street level: rooftop wind and street hum, crossfaded by
  the player's height above it.
  The lit windows are in the facade material.
* chapter 1: the BP_GameMode_EastVillage override (starts DA_CH01_Rooftops) and three
  ``City_Obj_<objective>`` trigger volumes on roofs picked from the records, each with a 1 m
  ``City_Beacon_<objective>`` (pole, emissive purple ``City_BeaconTop_``, and a movable 300 lm
  purple ``City_BeaconLight_``) at the end of the roof nearest the start.
* Barney's archers: two ``City_Archer_<n>`` (BP_Archer, tag ArcherPair) on the roofs of two other
  buildings 15 to 25 m from the find_arrow roof, each with a clear line to it (building footprints
  and heights, parapets included) and as far apart around it as the roofs allow. Their roofs, and
  every roof those lines cross, get no rooftop clutter. Aggressive only when Kate is within 30 m.
* chapter 1's first fight: five ``City_Thug_<n>``. A Fists and a Bat thug (BP_Thug) face each
  other on the cross_block roof (tag RoofPair, counted by ``City_ThugGroup_clear_roof``, which
  completes ``clear_roof``); a Bat thug and the gunner patrol the Avenue A sidewalk beside the
  park between ``City_Patrol_0`` and ``City_Patrol_1`` (ATargetPoints 40 m apart, tag StreetPair);
  ``City_Thug_4``, the heavy (BP_Thug_Heavy, shield), walks his own 20 m of the same sidewalk
  between ``City_Patrol_2`` and ``City_Patrol_3`` near the park's south corner. The three street
  thugs are also tagged StreetGroup.
* the partner: ``City_ClintStart`` (an ATargetPoint 5 m behind the PlayerStart, toward the park)
  and ``City_Clint`` (BP_Clint) standing on it; BP_PartnerController possesses him at load.
* grapple anchors (``City_Anchor_<n>``, BP_GrappleAnchor): on every building over 8 m, one on
  the parapet at each roof corner and one mid-edge on edges over 25 m, none within 4 m of
  another, each with its landing point on the roof clear of the parapet.
* traversable ledges (``City_Ledge_<id>_<edge>``): a hidden Game Animation Sample
  LevelBlock_Traversable along every roof edge of 1 m or more, its Ledge_1 spline on the
  parapet's outer top edge, so the sample's vault and mantle see the tenements.
  Neither the anchors nor the ledges are saved in the map (5,400 actors made it 72 MB): they go
  into ``/Game/City/EastVillage/DA_EastVillage_CityProps`` (UCityLedgeData) and one
  ``City_LedgeSpawner`` (ACityLedgeSpawner) spawns them at BeginPlay. Parkour test
  blocks: ``City_Test_Vault`` (90 cm) and ``City_Test_Mantle`` (150 cm) on the street by the
  PlayerStart, and three low ``City_ParkWall_<n>`` in the park.
* fire escapes (``City_FireEscape_<id>_<floor>``): on every building 10 to 30 m tall, on the
  longest of its edges nearest a road, a landing per floor (330 cm apart from the second floor to
  at least 180 cm under the roof): a 240 x 90 x 8 cm slab 5 cm off the facade, 90 cm rails on
  its outer three sides with a traversable ledge on the outer rail, and a ladder down to the
  landing below, alternating ends. Black iron (M_SteelPainted). A building whose escape would
  hit another building, a street lamp or another escape gets none. Data in the same props
  asset, spawned by the same spawner.
* clutter (see clutter_plan): water towers, HVAC boxes and chimneys on the roofs, hydrants, bins and
  bags, scaffolding and parked cars on the street; data in the props asset, drawn by the spawner as
  one instanced mesh per kind.

Idempotent: each mesh carries a ``CityHash`` metadata tag (hash of its source record and the
generator version); a mesh is rebuilt only when that hash changes. Actors are found by label
and compared before anything is set. The level is saved only when something changed. City
actors whose record disappeared are deleted.

    UnrealEditor-Cmd.exe Hawkeye.uproject -run=pythonscript ^
        -script="Tools\\Editor\\generate_city.py" -unattended -nullrhi -nosplash -nop4 -stdout

or Tools\\generate-city.ps1.
"""

import io
import json
import math
import os
import sys
import zlib

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


PAINTED_BUILDINGS = ("garage", "garages", "shed", "roof", "warehouse", "parking", "service")
STONE_BUILDINGS = ("church", "civic", "school", "synagogue", "religious", "public", "university")
# Tenements (10 to 30 m) pick a brick or brownstone by a stable hash of their id, in these shares.
TENEMENT_STYLES = (("BrickRed", 0.32), ("BrickBrown", 0.26), ("BrickPurple", 0.2), ("Brownstone", 0.22))


def facade_style(rec):
    """MI_Facade_<style> for a building record: painted grey for sheds and garages (and anything
    under 6 m), stone for churches, civic buildings and anything over 30 m, brownstone from 6 to
    10 m, and a brick or brownstone by hash for the 10 to 30 m tenements."""
    kind = (rec.get("tags") or {}).get("building", "")
    h = rec["height_m"]
    if kind in PAINTED_BUILDINGS or h < PARAPET_MIN_HEIGHT_M:
        return "Painted"
    if kind in STONE_BUILDINGS or h > FIRE_ESCAPE_MAX_HEIGHT_M:
        return "Stone"
    if h < FIRE_ESCAPE_MIN_HEIGHT_M:
        return "Brownstone"
    pick = stable_hash(rec["id"], "facade")
    for style, share in TENEMENT_STYLES:
        if pick < share:
            return style
        pick -= share
    return TENEMENT_STYLES[-1][0]


def building_pieces(district, facades):
    """``facades`` maps a facade style to its MI_Facade_ instance."""
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

        style = facade_style(rec)
        material = facades.get(style)
        tags = ["City", "CityBuilding", "osm:" + rec["id"],
                "height_cm:{0:.1f}".format(height_cm), "parapet:{0}".format(int(parapet)), "facade:" + style]
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
        materials["ground"], ["City", "CityGround"]))

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

# A January night: the sun is down, a dim cool moon (the only directional light, and the sky
# atmosphere's light, so the atmosphere draws a moonlit sky and the moon's disc) lights the roofs,
# and the lamps and lit windows carry the streets.
MOON_LABEL = "Moon"
RETIRED_SUN_LABEL = "Sun"
MOON_ROTATION = unreal.Rotator(0.0, -30.0, -30.0)   # roll, pitch, yaw: 30 degrees up in the WSW
MOON_LUX = 0.3
MOON_COLOR = (0.55, 0.65, 1.0)
MOON_DISK_SCALE = (0.9, 0.93, 1.0)
SKY_INTENSITY = 3.0
FOG_DENSITY = 0.008
FOG_INSCATTERING = (0.035, 0.035, 0.06)   # night haze, a touch of purple
FOG_CUTOFF = 300000.0                     # cm; the star sphere (400 m further out) stays clear of fog
# Auto exposure is off project-wide (Config/DefaultEngine.ini), so exposure is fixed: the
# min/max brightness pair pins it at 1.0, and the bias is the knob.
EXPOSURE_BRIGHTNESS = 1.0
EXPOSURE_BIAS = 2.0
# Grading: purple in the shadows, cream in the highlights, a light vignette.
GAIN_SHADOWS = (0.92, 0.86, 1.10, 1.0)
GAIN_HIGHLIGHTS = (1.05, 1.0, 0.88, 1.0)
VIGNETTE = 0.3
STARS_LABEL = "City_NightSky"
STARS_RADIUS = 340000.0                   # cm
SPHERE = "/Engine/BasicShapes/Sphere"     # 100 cm across


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
    elif isinstance(value, (unreal.LinearColor, unreal.Vector4, unreal.Color)):
        same = m.same_value(current, value, tol)
    else:
        same = current == value
    if same:
        return 0
    obj.set_editor_property(prop, value)
    return 1


def _color(rgb):
    return unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)


def ensure_lighting(existing):
    changes = 0
    old_sun = existing.pop(RETIRED_SUN_LABEL, None)
    if old_sun is not None:
        old_sun.destroy_actor()
        changes += 1
        c.log("updated", RETIRED_SUN_LABEL, "removed; the moon lights the night")
    moon, created = ensure_labelled(existing, unreal.DirectionalLight, MOON_LABEL, unreal.Vector(0, 0, 3000),
                                    MOON_ROTATION)
    changes += created
    if moon is not None:
        comp = moon.get_editor_property("directional_light_component")
        changes += set_if_different(comp, "mobility", unreal.ComponentMobility.MOVABLE, MOON_LABEL)
        changes += set_if_different(comp, "intensity", MOON_LUX, MOON_LABEL)
        changes += set_if_different(comp, "atmosphere_sun_light", True, MOON_LABEL)
        changes += set_if_different(comp, "use_temperature", False, MOON_LABEL)
        want = unreal.Color(r=int(MOON_COLOR[0] * 255), g=int(MOON_COLOR[1] * 255), b=int(MOON_COLOR[2] * 255), a=255)
        have = comp.get_editor_property("light_color")
        if (have.r, have.g, have.b) != (want.r, want.g, want.b):
            comp.set_editor_property("light_color", want)
            changes += 1
        changes += set_if_different(comp, "atmosphere_sun_disk_color_scale", _color(MOON_DISK_SCALE), MOON_LABEL)
        rot = moon.get_actor_rotation()
        if abs(rot.pitch - MOON_ROTATION.pitch) > 0.01 or abs(rot.yaw - MOON_ROTATION.yaw) > 0.01:
            moon.set_actor_rotation(MOON_ROTATION, False)
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
        changes += set_if_different(comp, "fog_inscattering_luminance", _color(FOG_INSCATTERING), "HeightFog", 1e-4)
        changes += set_if_different(comp, "fog_cutoff_distance", FOG_CUTOFF, "HeightFog", 1.0)

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
                            ("auto_exposure_bias", EXPOSURE_BIAS),
                            ("override_color_gain_shadows", True),
                            ("color_gain_shadows", unreal.Vector4(*GAIN_SHADOWS)),
                            ("override_color_gain_highlights", True),
                            ("color_gain_highlights", unreal.Vector4(*GAIN_HIGHLIGHTS)),
                            ("override_vignette_intensity", True),
                            ("vignette_intensity", VIGNETTE)):
            current = settings.get_editor_property(prop)
            if not m.same_value(current, value):
                settings.set_editor_property(prop, value)
                dirty = True
        if dirty:
            pp.set_editor_property("settings", settings)
            changes += 1

    changes += ensure_night_sky(existing)

    # Nothing in this map may bake: every light is movable (Lumen, no lightmass).
    for actor in c.all_level_actors():
        if isinstance(actor, (unreal.Light, unreal.SkyLight)):
            root = actor.get_editor_property("root_component")
            if root is not None and root.get_editor_property("mobility") != unreal.ComponentMobility.MOVABLE:
                root.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
                changes += 1
    return changes


def ensure_night_sky(existing):
    """City_NightSky: the engine sphere scaled round the district, seen from inside, in M_NightStars.
    Not the engine's own starry sky (BP_Sky_Sphere): that replaces the sky atmosphere rather than
    adding to it."""
    sphere = c.load_or_none(SPHERE)
    stars = c.load_or_none(m.M_NIGHT_STARS)
    if sphere is None or stars is None:
        c.log("FAILED", STARS_LABEL, "no engine sphere or M_NightStars")
        return 0
    scale = STARS_RADIUS / 50.0
    changes = _ensure_mesh_actor(existing, STARS_LABEL, sphere, stars, unreal.Vector(0.0, 0.0, 0.0),
                                 unreal.Rotator(0.0, 0.0, 0.0), unreal.Vector(scale, scale, scale),
                                 ["City", "CityNightSky"])
    actor = existing.get(STARS_LABEL)
    if actor is not None:
        comp = actor.get_editor_property("static_mesh_component")
        changes += set_if_different(comp, "cast_shadow", False, STARS_LABEL)
        # No collision at all: everything in the district is inside this sphere, and its simple
        # sphere collider pushed Kate, the thugs and every ground trace out through the floor.
        if str(comp.get_collision_profile_name()) != "NoCollision":
            comp.set_collision_profile_name("NoCollision")
            changes += 1
        changes += set_if_different(comp, "affect_distance_field_lighting", False, STARS_LABEL)
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


# Clint (docs/DESIGN.md, "The partner"): BP_Clint placed CLINT_BEHIND_START behind the PlayerStart,
# on the park side of East 7th Street, standing on whatever slab is under him. His AIController
# (BP_PartnerController) possesses him at load and he follows whoever the player is playing.
CLINT_START_LABEL = "City_ClintStart"
CLINT_LABEL = "City_Clint"
CLINT_BP_PATH = "/Game/Blueprints/Player"
CLINT_BP_NAME = "BP_Clint"
CLINT_BEHIND_START = 500.0         # cm
CLINT_HALF_HEIGHT = 92.5           # cm; BP_Clint's capsule
CLINT_MARKER_HEIGHT = 100.0        # cm above the pavement for the start point


def clint_start_transform(district):
    """(x, y, ground z, yaw): CLINT_BEHIND_START behind the PlayerStart, facing the way it faces."""
    loc, rot = player_start_transform(district)
    yaw = math.radians(rot.yaw)
    x = loc.x - math.cos(yaw) * CLINT_BEHIND_START
    y = loc.y - math.sin(yaw) * CLINT_BEHIND_START
    width_m = next((rec["width_m"] for rec in district.roads if rec.get("name") == PLAYER_START_STREET), 12.0)
    # A first guess only (ensure_clint traces the slab): the start stands 3 m off the centre line
    # toward the park, and behind it is further that way.
    off_road = 300.0 + CLINT_BEHIND_START > width_m * 50.0
    return x, y, SIDEWALK_TOP if off_road else ROAD_TOP, rot.yaw


def ensure_clint(district, existing):
    """City_ClintStart (a TargetPoint) and City_Clint (BP_Clint) on it. Idempotent by label."""
    clint_cls = c.load_generated_class(CLINT_BP_PATH, CLINT_BP_NAME)
    if clint_cls is None:
        c.log("FAILED", CLINT_LABEL, "BP_Clint missing; run create_partner.py")
        return 0
    x, y, guess, yaw = clint_start_transform(district)
    # The kerb makes a guess 13 cm out; the slab under him says where the pavement really is.
    ground = ground_z(x, y, guess, existing)
    marker, changes = _ensure_located(existing, CLINT_START_LABEL, unreal.TargetPoint,
                                      unreal.Vector(x, y, ground + CLINT_MARKER_HEIGHT), yaw)
    if marker is not None:
        changes += _ensure_tags(marker, ["City", "CityClintStart"])
    clint, n = _ensure_located(existing, CLINT_LABEL, clint_cls,
                               unreal.Vector(x, y, ground + CLINT_HALF_HEIGHT + 2.0), yaw)
    if clint is not None:
        n += _ensure_tags(clint, ["City", "CityPartner"])
    changes += n
    c.log("updated" if changes else "exists", CLINT_LABEL, "at ({0:.0f}, {1:.0f}, {2:.0f}) yaw {3:.0f}, {4:.0f} cm behind the start".format(
        x, y, ground, yaw, CLINT_BEHIND_START))
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
    """BP_GameMode_EastVillage (child of BP_HawkeyeGameMode) starting DA_CH01_Rooftops.

    Returns (generated class or None, number of changes)."""
    parent = c.load_generated_class(PLAYER_PATH, "BP_HawkeyeGameMode")
    if parent is None:
        c.log("skipped", DISTRICT_GAME_MODE, "BP_HawkeyeGameMode_C not found; run create_blueprints.py")
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
        c.log("created", full, "parent BP_HawkeyeGameMode_C")
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
    """The map's GameMode override: BP_GameMode_EastVillage, or BP_HawkeyeGameMode as a fallback."""
    game_mode, _changes = ensure_district_game_mode()
    name = DISTRICT_GAME_MODE + "_C"
    if game_mode is None:
        game_mode = c.load_generated_class(PLAYER_PATH, "BP_HawkeyeGameMode")
        name = "BP_HawkeyeGameMode_C"
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
    volume_class = c.find_class("ObjectiveTriggerVolume", "/Script/Hawkeye.ObjectiveTriggerVolume")
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


# Each objective roof also gets a beacon: a 1 m pole with an emissive purple cap and a faint
# purple point light, at the end of the roof nearest the start, so the roof reads from the street
# at night. Labels City_Beacon_<id> (pole), City_BeaconTop_<id> (cap), City_BeaconLight_<id>.
BEACON_PREFIX = "City_Beacon_"
BEACON_TOP_PREFIX = "City_BeaconTop_"
BEACON_LIGHT_PREFIX = "City_BeaconLight_"
BEACON_HEIGHT = 100.0             # cm, roof to the top of the cap
BEACON_CAP = 10.0                 # cm of the height that is the emissive cap
BEACON_POLE_DIAMETER = 6.0
BEACON_CAP_DIAMETER = 12.0
BEACON_INSET = 150.0              # cm in from the end of the roof nearest the start
BEACON_MIN_CLEARANCE = 100.0      # cm from any roof edge, else the beacon goes to the roof centre
BEACON_LUMENS = 300.0
BEACON_RADIUS = 1000.0
BEACON_COLOR = (0.62, 0.25, 1.0)  # Kate purple
BEACON_EMISSIVE = 4.0             # before m.EMISSIVE_INTENSITY_FACTOR
MI_BEACON = m.MATERIALS_PATH + "/MI_ObjectiveBeacon"


def beacon_spots(district):
    """{objective id: (x, y, roof z, how it was placed)} for every objective roof.

    On the roof's long axis, BEACON_INSET in from the end nearer the start: the side seen first,
    and clear of the RoofPair, who stand either side of the centre. The other end if that one is
    not BEACON_MIN_CLEARANCE inside the outline, the roof's centre if neither is."""
    start, _rot = player_start_transform(district)
    start = (start.x, start.y)
    out = {}
    for oid, rec in objective_roofs(district).items():
        ring = rec["ring"]
        cx, cy, half_l, _hw, yaw = oriented_rect(ring)
        ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        reach = max(half_l - BEACON_INSET, 0.0)
        ends = [(cx + ux * reach, cy + uy * reach), (cx - ux * reach, cy - uy * reach)]
        ends.sort(key=lambda p: math.hypot(p[0] - start[0], p[1] - start[1]))
        how, spot = next((("end", p) for p in ends if _edge_clearance(p, ring) >= BEACON_MIN_CLEARANCE),
                         ("centre", (cx, cy)))
        out[oid] = (spot[0], spot[1], rec["height_m"] * 100.0, how)
    return out


def _ensure_no_collision(actor, label):
    comp = actor.get_editor_property("static_mesh_component")
    if str(comp.get_collision_profile_name()) != "NoCollision":
        comp.set_collision_profile_name("NoCollision")
        return 1
    return 0


def _ensure_beacon_light(existing, label, loc, tags):
    changes = 0
    actor = existing.get(label)
    if actor is not None and not isinstance(actor, unreal.PointLight):
        actor.destroy_actor()
        actor = None
    if actor is None:
        actor = c.spawn_actor(unreal.PointLight, loc, label=label)
        if actor is None:
            c.log("FAILED", label, "spawn_actor returned None")
            return 0
        existing[label] = actor
        changes += 1
    if not same_vector(actor.get_actor_location(), loc, 0.5):
        actor.set_actor_location(loc, False, True)
        changes += 1
    comp = actor.get_editor_property("point_light_component")
    changes += set_if_different(comp, "mobility", unreal.ComponentMobility.MOVABLE, label)
    changes += set_if_different(comp, "intensity_units", unreal.LightUnits.LUMENS, label)
    changes += set_if_different(comp, "intensity", BEACON_LUMENS, label, 0.5)
    changes += set_if_different(comp, "attenuation_radius", BEACON_RADIUS, label, 0.5)
    changes += set_if_different(comp, "cast_shadows", False, label)
    changes += set_if_different(comp, "use_temperature", False, label)
    want = unreal.Color(r=int(BEACON_COLOR[0] * 255), g=int(BEACON_COLOR[1] * 255), b=int(BEACON_COLOR[2] * 255), a=255)
    have = comp.get_editor_property("light_color")
    if (have.r, have.g, have.b) != (want.r, want.g, want.b):
        comp.set_editor_property("light_color", want)
        changes += 1
    return changes + _ensure_tags(actor, tags)


def ensure_objective_beacons(district, existing):
    cylinder = c.load_or_none(CYLINDER)
    if cylinder is None:
        c.log("FAILED", BEACON_PREFIX + "*", "engine cylinder not found")
        return 0
    emissive = m.ensure_material(m.M_EMISSIVE, m._build_emissive)
    cap_mat = m.ensure_material_instance(
        MI_BEACON, emissive,
        vectors=[(m.EMISSIVE_COLOR_PARAM, BEACON_COLOR)],
        scalars=[(m.EMISSIVE_INTENSITY_PARAM, BEACON_EMISSIVE * m.EMISSIVE_INTENSITY_FACTOR)])
    pole_mat = m.ensure_steel_painted()
    spots = beacon_spots(district)
    changes = 0
    wanted = set()
    no_rot = unreal.Rotator(0.0, 0.0, 0.0)
    for oid in OBJECTIVE_IDS:
        spot = spots.get(oid)
        if spot is None:
            c.log("FAILED", BEACON_PREFIX + oid, "no objective roof")
            continue
        x, y, roof_z, how = spot
        tags = ["City", "CityBeacon", "objective:" + oid]
        pole_label, top_label, light_label = BEACON_PREFIX + oid, BEACON_TOP_PREFIX + oid, BEACON_LIGHT_PREFIX + oid
        wanted.update((pole_label, top_label, light_label))
        pole_h = BEACON_HEIGHT - BEACON_CAP
        n = _ensure_mesh_actor(
            existing, pole_label, cylinder, pole_mat, unreal.Vector(x, y, roof_z + pole_h * 0.5), no_rot,
            unreal.Vector(BEACON_POLE_DIAMETER / 100.0, BEACON_POLE_DIAMETER / 100.0, pole_h / 100.0), tags)
        n += _ensure_mesh_actor(
            existing, top_label, cylinder, cap_mat, unreal.Vector(x, y, roof_z + pole_h + BEACON_CAP * 0.5), no_rot,
            unreal.Vector(BEACON_CAP_DIAMETER / 100.0, BEACON_CAP_DIAMETER / 100.0, BEACON_CAP / 100.0), tags)
        for label in (pole_label, top_label):
            if label in existing:
                n += _ensure_no_collision(existing[label], label)
        n += _ensure_beacon_light(existing, light_label, unreal.Vector(x, y, roof_z + BEACON_HEIGHT + 20.0), tags)
        changes += n
        c.log("updated" if n else "exists", pole_label, "at {0:.0f}, {1:.0f} on a {2:.1f} m roof ({3})".format(
            x, y, roof_z / 100.0, how))

    for label, actor in list(existing.items()):
        if label.startswith((BEACON_PREFIX, BEACON_TOP_PREFIX, BEACON_LIGHT_PREFIX)) and label not in wanted:
            actor.destroy_actor()
            existing.pop(label, None)
            changes += 1
            c.log("updated", label, "removed stray beacon")
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
STREET_GROUP_TAG = "StreetGroup"  # the pair and the heavy: the street fight
HEAVY_BP_NAME = "BP_Thug_Heavy"
HEAVY_PATROL_LENGTH = 2000.0      # cm between the heavy's two patrol points
HEAVY_CORNER_REACH = 4000.0       # cm from the sidewalk line a park vertex may be and still be its corner
HEAVY_PAIR_CLEAR = 500.0          # cm the heavy's patrol keeps from the pair's
HEAVY_CORNER_START = 600.0        # cm along the sidewalk from the corner the heavy's patrol starts at least
ROOF_PAIR_GAP = 300.0             # cm between the two arguing on the roof
ROOF_EDGE_CLEARANCE = 200.0       # cm from any roof edge, so neither stands in the parapet
PATROL_STREET = "Avenue A"
PATROL_LENGTH = 4000.0            # cm between the two patrol points
PATROL_POINT_HEIGHT = 100.0       # cm above the sidewalk
STREET_PAIR_SPACING = 150.0       # cm; the second walks this far ahead of the first
CROSSING_CLEARANCE = 300.0        # cm; a patrol point this far from any other road's carriageway

# Every thug keeps AThugCharacter's own "Thug" tag first: the takedown looks for it and a thug's
# swing skips anyone carrying it. The labels' other tags come after.
# (weapon, group tag) per thug, in label order: the roof pair (one fists, one bat), the street
# pair (one bat, one gunner), then the heavy on his own patrol.
THUG_LOADOUT = (("FISTS", ROOF_PAIR_TAG), ("BAT", ROOF_PAIR_TAG),
                ("BAT", STREET_PAIR_TAG), ("PISTOL", STREET_PAIR_TAG),
                ("SHIELD", STREET_GROUP_TAG))


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


def _park_sidewalk(district):
    """(q, d, n, offset): the Avenue A centre-line point nearest the park's centroid, the avenue's
    direction there, the normal toward the park, and how far the park-side sidewalk's middle is
    from the centre line. None without a park or the avenue."""
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
    return q, d, n, width_m * 50.0 + SIDEWALK_WIDTH * 0.5


def street_patrol(district):
    """(P0, P1, direction yaw) on the Avenue A sidewalk on the park side, PATROL_LENGTH apart.

    Centred on the point of the Avenue A centre line nearest the park's centroid, moved along
    the avenue until both points are clear of every crossing street."""
    walk = _park_sidewalk(district)
    if walk is None:
        return None
    q, d, n, offset = walk
    centre = (q[0] + n[0] * offset, q[1] + n[1] * offset)
    half = PATROL_LENGTH * 0.5
    for shift in (0.0, 500.0, -500.0, 1000.0, -1000.0, 1500.0, -1500.0, 2000.0, -2000.0):
        c0 = (centre[0] + d[0] * shift, centre[1] + d[1] * shift)
        p0 = (c0[0] - d[0] * half, c0[1] - d[1] * half)
        p1 = (c0[0] + d[0] * half, c0[1] + d[1] * half)
        if _clear_of_crossings(p0, district, PATROL_STREET) and _clear_of_crossings(p1, district, PATROL_STREET):
            return p0, p1, math.degrees(math.atan2(d[1], d[0]))
    return None


def heavy_patrol(district):
    """(P0, P1, direction yaw) for the heavy: HEAVY_PATROL_LENGTH of the same park-side sidewalk,
    starting as close to the park's south corner (its southernmost vertex, largest Y, on the Avenue A
    side) as the crossing streets allow and heading back toward the pair, clear of their patrol."""
    walk = _park_sidewalk(district)
    pair = street_patrol(district)
    if walk is None or pair is None:
        return None
    q, d, n, offset = walk
    base = (q[0] + n[0] * offset, q[1] + n[1] * offset)
    ring = district.ring_cm(district.parks[0]["outer"])
    near = [v for v in ring if abs((v[0] - base[0]) * n[0] + (v[1] - base[1]) * n[1]) <= HEAVY_CORNER_REACH]
    if not near:
        return None
    corner = max(near, key=lambda v: v[1])
    t_corner = (corner[0] - base[0]) * d[0] + (corner[1] - base[1]) * d[1]
    sign = -1.0 if t_corner > 0.0 else 1.0
    pair_seg = [pair[0], pair[1]]

    def at(t):
        return (base[0] + d[0] * t, base[1] + d[1] * t)

    step = HEAVY_CORNER_START
    while step <= abs(t_corner):
        t0 = t_corner + sign * step
        p0, p1 = at(t0), at(t0 + sign * HEAVY_PATROL_LENGTH)
        clear = (_clear_of_crossings(p0, district, PATROL_STREET) and _clear_of_crossings(p1, district, PATROL_STREET)
                 and closest_point_on_polyline(p0, pair_seg)[0] >= HEAVY_PAIR_CLEAR
                 and closest_point_on_polyline(p1, pair_seg)[0] >= HEAVY_PAIR_CLEAR)
        if clear:
            yaw = math.degrees(math.atan2(d[1] * sign, d[0] * sign))
            return p0, p1, yaw
        step += 250.0
    return None


def thug_placements(district):
    """([(label, x, y, z, yaw, weapon, tags, [patrol labels], blueprint)], [(label, x, y, z, tag)], roof record)."""
    roof = roof_pair_spots(district)
    patrol = street_patrol(district)
    heavy = heavy_patrol(district)
    thugs, points = [], []
    roof_rec = None
    if roof is not None:
        a, b, roof_rec = roof
        for i, spot in enumerate((a, b)):
            weapon, tag = THUG_LOADOUT[i]
            thugs.append((THUG_PREFIX + str(i), spot[0], spot[1], spot[2], spot[3], weapon,
                          ["Thug", "City", "CityThug", tag, "osm:" + roof_rec["id"]], [], THUG_BP_NAME))
    if patrol is not None:
        p0, p1, yaw = patrol
        z_walk = SIDEWALK_TOP
        points = [(PATROL_PREFIX + "0", p0[0], p0[1], z_walk + PATROL_POINT_HEIGHT, STREET_PAIR_TAG),
                  (PATROL_PREFIX + "1", p1[0], p1[1], z_walk + PATROL_POINT_HEIGHT, STREET_PAIR_TAG)]
        ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        for j in range(2):
            weapon, tag = THUG_LOADOUT[2 + j]
            along = STREET_PAIR_SPACING * j
            thugs.append((THUG_PREFIX + str(2 + j), p0[0] + ux * along, p0[1] + uy * along,
                          z_walk + THUG_HALF_HEIGHT + 2.0, yaw, weapon,
                          ["Thug", "City", "CityThug", tag, "street:" + PATROL_STREET, STREET_GROUP_TAG],
                          [PATROL_PREFIX + "1", PATROL_PREFIX + "0"], THUG_BP_NAME))
    if patrol is not None and heavy is not None:
        h0, h1, h_yaw = heavy
        z_walk = SIDEWALK_TOP
        points += [(PATROL_PREFIX + "2", h0[0], h0[1], z_walk + PATROL_POINT_HEIGHT, STREET_GROUP_TAG),
                   (PATROL_PREFIX + "3", h1[0], h1[1], z_walk + PATROL_POINT_HEIGHT, STREET_GROUP_TAG)]
        weapon, tag = THUG_LOADOUT[4]
        thugs.append((THUG_PREFIX + "4", h0[0], h0[1], z_walk + THUG_HALF_HEIGHT + 2.0, h_yaw, weapon,
                      ["Thug", "City", "CityThug", tag, "street:" + PATROL_STREET],
                      [PATROL_PREFIX + "3", PATROL_PREFIX + "2"], HEAVY_BP_NAME))
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
    group_cls = c.find_class("ThugGroupObjective", "/Script/Hawkeye.ThugGroupObjective")
    if thug_cls is None or group_cls is None or not hasattr(unreal, "ThugWeapon"):
        c.log("FAILED", THUG_PREFIX + "*", "BP_Thug or AThugGroupObjective missing; build and run create_world_blueprints.py")
        return 0
    thugs, points, roof_rec = thug_placements(district)
    if len(thugs) != len(THUG_LOADOUT) or len(points) != 4:
        c.log("FAILED", THUG_PREFIX + "*", "found {0} thug spots and {1} patrol points".format(len(thugs), len(points)))
    classes = {THUG_BP_NAME: thug_cls, HEAVY_BP_NAME: c.load_generated_class(THUG_BP_PATH, HEAVY_BP_NAME)}
    if classes[HEAVY_BP_NAME] is None:
        c.log("FAILED", THUG_PREFIX + "4", HEAVY_BP_NAME + " missing; run create_enemies.py")
    changes = 0
    wanted = set()
    patrol_actors = {}
    for label, x, y, z, tag in points:
        actor, n = _ensure_located(existing, label, unreal.TargetPoint, unreal.Vector(x, y, z), 0.0)
        if actor is None:
            continue
        wanted.add(label)
        patrol_actors[label] = actor
        changes += n + _ensure_tags(actor, ["City", "CityPatrol", tag])
    for label, x, y, z, yaw, weapon, tags, patrol, bp_name in thugs:
        cls = classes.get(bp_name)
        if cls is None:
            continue
        actor, n = _ensure_located(existing, label, cls, unreal.Vector(x, y, z), yaw)
        if actor is None:
            continue
        wanted.add(label)
        n += _ensure_thug_props(actor, weapon, [patrol_actors[p] for p in patrol if p in patrol_actors])
        n += _ensure_tags(actor, tags)
        changes += n
        c.log("updated" if n else "exists", label, "{0} {1} at ({2:.0f}, {3:.0f}, {4:.0f}) yaw {5:.0f}".format(
            weapon.lower(), tags[3], x, y, z, yaw))
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
# Barney's archers: two BP_Archer on roofs facing the find_arrow roof
# --------------------------------------------------------------------------------------

ARCHER_PREFIX = "City_Archer_"
ARCHER_BP_PATH = "/Game/Blueprints/Bosses"
ARCHER_BP_NAME = "BP_Archer"
ARCHER_PAIR_TAG = "ArcherPair"
ARCHER_MIN_DISTANCE = 1500.0      # cm from the find_arrow beacon (where Kate stands): the archer's band
ARCHER_MAX_DISTANCE = 2500.0
ARCHER_IDEAL_DISTANCE = 2000.0
ARCHER_GRID = 150.0               # cm between candidate spots on a roof
ARCHER_EDGE_CLEARANCE = 250.0     # cm inside the roof edge, so he never stands in the parapet
ARCHER_EYE = THUG_HALF_HEIGHT + 60.0     # cm above his roof
ARCHER_TARGET_CHEST = 130.0       # cm above the find_arrow roof: Kate's chest standing
ARCHER_LINE_MARGIN = 20.0         # cm the line must clear a roof or parapet by
ARCHER_PARAPET = 90.0             # cm; generate's parapets (skipped under 6 m)
ARCHER_MIN_SPREAD_DEG = 35.0      # the two archers this far apart round her
ARCHER_MIN_APART = 800.0          # cm between the two archers
ARCHER_ROOF_PAIR_CLEAR = 3000.0   # preferred distance from the RoofPair, so the first fight stays two on one


def _building_tops(district):
    """[(rec, ring, bounds, top z cm)] for every building, parapet included."""
    out = []
    for rec in district.buildings:
        ring = geo.clean_ring(district.ring_cm(rec["outer"]), min_edge=5.0, collinear_tol=2.0)
        if len(ring) < 3:
            continue
        top = rec["height_m"] * 100.0 + (ARCHER_PARAPET if rec["height_m"] >= 6.0 else 0.0)
        out.append((rec, ring, geo.bounds(ring), top))
    return out


def _segment_params(a, b, ring):
    """Parameters (0..1) along a->b where it crosses the ring's edges, plus 0 and 1 if an end is inside."""
    ts = []
    if geo.point_in_polygon(a, ring):
        ts.append(0.0)
    if geo.point_in_polygon(b, ring):
        ts.append(1.0)
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = len(ring)
    for i in range(n):
        p, q = ring[i], ring[(i + 1) % n]
        ex, ey = q[0] - p[0], q[1] - p[1]
        den = dx * ey - dy * ex
        if abs(den) < 1e-9:
            continue
        t = ((p[0] - a[0]) * ey - (p[1] - a[1]) * ex) / den
        u = ((p[0] - a[0]) * dy - (p[1] - a[1]) * dx) / den
        if 0.0 <= t <= 1.0 and 0.0 <= u <= 1.0:
            ts.append(t)
    return ts


def archer_line_blockers(a3, b3, tops):
    """Ids of the buildings whose roof or parapet the straight line a3 -> b3 does not clear."""
    a, b = (a3[0], a3[1]), (b3[0], b3[1])
    x0, x1 = min(a[0], b[0]), max(a[0], b[0])
    y0, y1 = min(a[1], b[1]), max(a[1], b[1])
    blocked = []
    for rec, ring, box, top in tops:
        if box[0] > x1 or box[2] < x0 or box[1] > y1 or box[3] < y0:
            continue
        ts = _segment_params(a, b, ring)
        if not ts:
            continue
        # The line is straight, so its lowest point over the footprint is at an entry or exit.
        low = min(a3[2] + (b3[2] - a3[2]) * t for t in ts)
        if low < top + ARCHER_LINE_MARGIN:
            blocked.append(rec["id"])
    return blocked


def archer_crossed_buildings(a3, b3, tops):
    """Ids of every building whose footprint the line a3 -> b3 passes over."""
    a, b = (a3[0], a3[1]), (b3[0], b3[1])
    return [rec["id"] for rec, ring, _box, _top in tops if _segment_params(a, b, ring)]


def archer_target(district):
    """(x, y, z) of Kate's chest at the find_arrow beacon, or None."""
    spot = beacon_spots(district).get("find_arrow")
    if spot is None:
        return None
    x, y, roof_z, _how = spot
    return (x, y, roof_z + ARCHER_TARGET_CHEST)


_ARCHER_CACHE = {}


def archer_placements(district):
    """[(label, x, y, z, yaw, osm id, distance cm)] for the two archers, best first; [] if none fit."""
    key = id(district)
    if key not in _ARCHER_CACHE:
        _ARCHER_CACHE[key] = _archer_placements(district)
    return _ARCHER_CACHE[key]


def _archer_placements(district):
    target = archer_target(district)
    roofs = objective_roofs(district)
    if target is None or "find_arrow" not in roofs:
        return []
    taken = {rec["id"] for rec in roofs.values()}
    safehouse = safehouse_spot(district)
    if safehouse is not None:
        taken.add(safehouse["rec"]["id"])
    cross = roofs.get("cross_block")
    cross_centre = cross["centre"] if cross else None
    reach = ARCHER_MAX_DISTANCE + 100.0
    # Every line runs between two points within reach of the target: nothing farther can block it.
    tops = [t for t in _building_tops(district) if not (t[2][0] > target[0] + reach or t[2][2] < target[0] - reach
                                                        or t[2][1] > target[1] + reach or t[2][3] < target[1] - reach)]
    best = {}   # osm id -> (score, x, y, z, yaw, distance)
    for rec, ring, box, _top in tops:
        if rec["id"] in taken or rec["height_m"] < OBJECTIVE_MIN_HEIGHT_M:
            continue
        if box[0] > target[0] + reach or box[2] < target[0] - reach or box[1] > target[1] + reach or box[3] < target[1] - reach:
            continue
        roof_z = rec["height_m"] * 100.0
        gx = box[0] + ARCHER_GRID * 0.5
        while gx < box[2]:
            gy = box[1] + ARCHER_GRID * 0.5
            while gy < box[3]:
                pt = (gx, gy)
                gy += ARCHER_GRID
                d = math.hypot(pt[0] - target[0], pt[1] - target[1])
                if d < ARCHER_MIN_DISTANCE or d > ARCHER_MAX_DISTANCE or _edge_clearance(pt, ring) < ARCHER_EDGE_CLEARANCE:
                    continue
                eye = (pt[0], pt[1], roof_z + ARCHER_EYE)
                if archer_line_blockers(eye, target, tops):
                    continue
                score = abs(d - ARCHER_IDEAL_DISTANCE)
                if cross_centre is not None and math.hypot(pt[0] - cross_centre[0], pt[1] - cross_centre[1]) < ARCHER_ROOF_PAIR_CLEAR:
                    score += 1000.0
                if rec["id"] not in best or score < best[rec["id"]][0]:
                    yaw = math.degrees(math.atan2(target[1] - pt[1], target[0] - pt[0]))
                    best[rec["id"]] = (score, pt[0], pt[1], roof_z + THUG_HALF_HEIGHT + 2.0, yaw, d)
            gx += ARCHER_GRID
    ranked = sorted(best.items(), key=lambda kv: (kv[1][0], kv[0]))
    if not ranked:
        return []
    first_id, first = ranked[0]
    picks = [(first_id, first)]

    def bearing(p):
        return math.degrees(math.atan2(p[2] - target[1], p[1] - target[0]))

    for spread in (ARCHER_MIN_SPREAD_DEG, 15.0, 0.0):
        for osm, cand in ranked[1:]:
            apart = math.hypot(cand[1] - first[1], cand[2] - first[2])
            turn = abs((bearing(cand) - bearing(first) + 180.0) % 360.0 - 180.0)
            if apart >= ARCHER_MIN_APART and turn >= spread:
                picks.append((osm, cand))
                break
        if len(picks) == 2:
            break
    return [(ARCHER_PREFIX + str(i), p[1], p[2], p[3], p[4], osm, p[5]) for i, (osm, p) in enumerate(picks)]


def archer_quiet_roofs(district):
    """Ids of the archers' roofs and every roof their lines to the find_arrow roof cross: no roof clutter."""
    target = archer_target(district)
    placements = archer_placements(district)
    if target is None or not placements:
        return set()
    tops = _building_tops(district)
    quiet = set()
    for _label, x, y, z, _yaw, osm, _d in placements:
        quiet.add(osm)
        quiet.update(archer_crossed_buildings((x, y, z - THUG_HALF_HEIGHT + ARCHER_EYE), target, tops))
    return quiet


def ensure_archers(district, existing):
    """City_Archer_<n> (BP_Archer), tagged ArcherPair. Idempotent by label."""
    cls = c.load_generated_class(ARCHER_BP_PATH, ARCHER_BP_NAME)
    if cls is None or not hasattr(unreal, "ThugWeapon"):
        c.log("FAILED", ARCHER_PREFIX + "*", "BP_Archer missing; build and run create_enemies.py")
        return 0
    placements = archer_placements(district)
    if len(placements) != 2:
        c.log("FAILED", ARCHER_PREFIX + "*", "found {0} archer spots facing find_arrow".format(len(placements)))
    changes = 0
    wanted = set()
    for label, x, y, z, yaw, osm, distance in placements:
        actor, n = _ensure_located(existing, label, cls, unreal.Vector(x, y, z), yaw)
        if actor is None:
            continue
        wanted.add(label)
        if actor.get_editor_property("weapon") != unreal.ThugWeapon.BOW:
            actor.set_editor_property("weapon", unreal.ThugWeapon.BOW)
            n += 1
        n += _ensure_tags(actor, ["Thug", "City", "CityArcher", ARCHER_PAIR_TAG, "osm:" + osm])
        changes += n
        c.log("updated" if n else "exists", label, "on osm {0} at ({1:.0f}, {2:.0f}, {3:.0f}) yaw {4:.0f}, {5:.0f} cm from find_arrow".format(
            osm, x, y, z, yaw, distance))
    for label, actor in list(existing.items()):
        if label.startswith(ARCHER_PREFIX) and label not in wanted:
            actor.destroy_actor()
            existing.pop(label, None)
            changes += 1
            c.log("updated", label, "removed stray")
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
LAMP_LUMENS = 1100.0
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
    # M_LampHead: the glow on the underside only, snow on top; same parameters as M_Emissive.
    parent = m.ensure_look_material(m.M_LAMP_HEAD, m._build_lamp_head)
    head = m.ensure_material_instance(
        MI_STREET_LAMP, parent,
        vectors=[(m.EMISSIVE_COLOR_PARAM, LAMP_COLOR)],
        scalars=[(m.EMISSIVE_INTENSITY_PARAM, LAMP_EMISSIVE * m.EMISSIVE_INTENSITY_FACTOR)])
    pole = m.ensure_steel_painted()
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
# ambience: the wind and street bed, and the buzz on the lamps round the park
# --------------------------------------------------------------------------------------

AMBIENCE_LABEL = "City_Ambience"      # AHawkeyeAmbience at street level; the fade reads height above it
LAMP_BUZZ_PREFIX = "City_LampBuzz_"   # an AAmbientSound on each of the lamps nearest the park
LAMP_BUZZ_COUNT = 20
LAMP_BUZZ_HEIGHT = 300.0              # cm up the pole: ATT_Lamp's 400 cm reaches a head under the lamp
LAMP_BUZZ_TAGS = ["City", "CityLamp"]  # CityLamp: the EMP silences it with the light


def _audio_sound(name):
    import create_audio  # noqa: PLC0415 - the generator needs only the paths

    return c.load_or_none(create_audio.sound_path(name))


def _sound_key(value):
    return value.get_path_name().split(".")[0] if value is not None else ""


def _ensure_sound(obj, prop, sound, context):
    if sound is None:
        c.log("skipped", context + "." + prop, "sound not built; run create_audio")
        return 0
    if _sound_key(obj.get_editor_property(prop)) == _sound_key(sound):
        return 0
    c.set_props(obj, [(prop, sound)], context)
    return 1


def ensure_ambience(district, existing):
    """City_Ambience at the ground's centre, street level, with the wind and street loops."""
    cls = c.find_class("HawkeyeAmbience", "/Script/Hawkeye.HawkeyeAmbience")
    if cls is None:
        c.log("skipped", AMBIENCE_LABEL, "AHawkeyeAmbience not built")
        return 0
    cx, cy = geo.centroid(ground_ring_cm(district))
    actor, changes = _ensure_located(existing, AMBIENCE_LABEL, cls, unreal.Vector(cx, cy, 0.0), 0.0)
    if actor is None:
        return changes
    changes += _ensure_sound(actor, "wind_sound", _audio_sound("MS_Amb_Wind"), AMBIENCE_LABEL)
    changes += _ensure_sound(actor, "street_sound", _audio_sound("MS_Amb_Street"), AMBIENCE_LABEL)
    changes += _ensure_tags(actor, ["City"])
    c.log("updated" if changes else "exists", AMBIENCE_LABEL)
    return changes


def lamp_buzz_spots(district):
    """[(x, y)] of the sound on the LAMP_BUZZ_COUNT lamps nearest the park, nearest first."""
    if not district.parks:
        return []
    ring = district.ring_cm(district.parks[0]["outer"])
    ranked = sorted((ring_distance((x, y), ring), i, x, y) for i, (x, y, _yaw, _s) in enumerate(lamp_spots(district)))
    return [(x, y) for _d, _i, x, y in ranked[:LAMP_BUZZ_COUNT]]


def ensure_lamp_buzz(district, existing):
    buzz = _audio_sound("MS_Amb_LampBuzz")
    spots = lamp_buzz_spots(district)
    changes = 0
    for i, (x, y) in enumerate(spots):
        label = LAMP_BUZZ_PREFIX + str(i)
        loc = unreal.Vector(x, y, SIDEWALK_TOP + LAMP_BUZZ_HEIGHT)
        actor, n = _ensure_located(existing, label, unreal.AmbientSound, loc, 0.0)
        if actor is None:
            continue
        n += _ensure_sound(actor.get_editor_property("audio_component"), "sound", buzz, label)
        n += _ensure_tags(actor, LAMP_BUZZ_TAGS)
        changes += n
    removed = 0
    for label, actor in list(existing.items()):
        rest = label[len(LAMP_BUZZ_PREFIX):] if label.startswith(LAMP_BUZZ_PREFIX) else ""
        if rest.isdigit() and int(rest) >= len(spots):
            actor.destroy_actor()
            existing.pop(label, None)
            removed += 1
    changes += removed
    c.log("updated" if changes else "exists", "lamp buzz",
          "{0} lamps nearest the park, {1} removed".format(len(spots), removed))
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
        cls = c.find_class("GrappleAnchor", "/Script/Hawkeye.GrappleAnchor")
    return cls


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

# --------------------------------------------------------------------------------------
# fire escapes
# --------------------------------------------------------------------------------------

FIRE_ESCAPE_PREFIX = "City_FireEscape_"
FIRE_ESCAPE_MIN_HEIGHT_M = 10.0
FIRE_ESCAPE_MAX_HEIGHT_M = 30.0
FIRE_ESCAPE_FLOOR = 330.0          # cm between landings; the lowest is at the second floor
FIRE_ESCAPE_ROOF_CLEARANCE = 180.0  # cm; the top landing's slab is at least this far under the roof
FIRE_ESCAPE_SLAB = (240.0, 90.0, 8.0)
FIRE_ESCAPE_GAP = 5.0              # cm between the facade and the slab
FIRE_ESCAPE_RAIL = 90.0            # rail height above the slab
FIRE_ESCAPE_RAIL_THICK = 6.0       # the rails' collision; the visible bars are thinner
FIRE_ESCAPE_MIN_EDGE = 300.0       # cm; the landing plus 30 cm each side
FIRE_ESCAPE_STREET_SLACK = 200.0   # cm; edges this much further from a road than the nearest still face it
FIRE_ESCAPE_FACING = 0.5           # the edge's outward normal within 60 degrees of the road
FIRE_ESCAPE_LAMP_CLEARANCE = 30.0  # cm between a landing and a lamp pole (or head)
FIRE_ESCAPE_GAP_TO_OTHER = 10.0    # cm between two buildings' landings


def _landing_corners(x, y, yaw, margin=0.0):
    """The landing's footprint as four (x, y) corners: along +/- half the slab, out from the gap."""
    ox, oy = -math.sin(math.radians(yaw)), math.cos(math.radians(yaw))   # local +Y, out of the facade
    ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))    # local +X, along it
    half = FIRE_ESCAPE_SLAB[0] * 0.5 + margin
    near, far = FIRE_ESCAPE_GAP - margin, FIRE_ESCAPE_GAP + FIRE_ESCAPE_SLAB[1] + margin
    return [(x + ux * a + ox * b, y + uy * a + oy * b) for a, b in ((-half, near), (half, near), (half, far), (-half, far))]


def _rects_overlap(a, b):
    """Separating-axis test for two convex quads given as corner lists."""
    for quad in (a, b):
        for i in range(4):
            (x0, y0), (x1, y1) = quad[i], quad[(i + 1) % 4]
            nx, ny = y1 - y0, x0 - x1
            pa = [nx * px + ny * py for px, py in a]
            pb = [nx * px + ny * py for px, py in b]
            if max(pa) < min(pb) or max(pb) < min(pa):
                return False
    return True


def _point_quad_distance(pt, quad):
    if geo.point_in_polygon(pt, quad):
        return 0.0
    return closest_point_on_polyline(pt, list(quad) + [quad[0]])[0]


def fire_escape_spots(district):
    """[(osm id, floor, (x, y, z), yaw, ladder drop, ladder side)] in a stable order.

    One escape per building FIRE_ESCAPE_MIN_HEIGHT_M to FIRE_ESCAPE_MAX_HEIGHT_M tall, on the
    longest exterior edge whose outward normal faces a road and whose middle is within
    FIRE_ESCAPE_STREET_SLACK of the nearest such edge's road distance. (x, y) is the edge's middle
    on the facade line, z the slab top, yaw turns local +X along the facade and +Y out of it.
    Landings at FIRE_ESCAPE_FLOOR, 2 x, ... up to FIRE_ESCAPE_ROOF_CLEARANCE under the roof. A
    building whose landings would stand in another building (or its own), within
    FIRE_ESCAPE_LAMP_CLEARANCE of a lamp, or on an earlier building's escape, gets none.
    """
    roads = sorted(road_paths(district), key=lambda rp: rp[0]["id"])
    segments = [(p, q) for _rec, paths in roads for path in paths for p, q in zip(path, path[1:])]
    buildings = []
    for rec in sorted(district.buildings, key=lambda r: r["id"]):
        ring = geo.clean_ring(district.ring_cm(rec["outer"]), min_edge=5.0, collinear_tol=2.0)
        if len(ring) >= 3:
            buildings.append((rec, ring, geo.bounds(ring)))
    lamps = [(x, y, yaw) for x, y, yaw, _shadows in lamp_spots(district)]

    def inside_other(pt, own_id, below_z=None):
        for rec, ring, (x0, y0, x1, y1) in buildings:
            if rec["id"] == own_id:
                continue
            if below_z is not None and rec["height_m"] * 100.0 + PARAPET_HEIGHT < below_z:
                continue
            if x0 <= pt[0] <= x1 and y0 <= pt[1] <= y1 and geo.point_in_polygon(pt, ring):
                return True
        return False

    def nearest_road(pt):
        best = None
        for p, q in segments:
            d, c_pt = closest_point_on_polyline(pt, [p, q])
            if best is None or d < best[0]:
                best = (d, c_pt)
        return best

    placed = []   # (quads by z) of accepted landings
    spots = []
    for rec, ring, _box in buildings:
        height_m = rec["height_m"]
        if height_m < FIRE_ESCAPE_MIN_HEIGHT_M or height_m > FIRE_ESCAPE_MAX_HEIGHT_M:
            continue
        height_cm = height_m * 100.0
        inward = 1.0 if geo.is_ccw(ring) else -1.0
        n = len(ring)
        candidates = []   # (road distance, -length, index, mid, outward, along)
        for i in range(n):
            ax, ay = ring[i]
            bx, by = ring[(i + 1) % n]
            length = math.hypot(bx - ax, by - ay)
            if length < FIRE_ESCAPE_MIN_EDGE or not segments:
                continue
            ox, oy = (by - ay) / length * inward, -(bx - ax) / length * inward   # outward normal
            ux, uy = (bx - ax) / length, (by - ay) / length
            mx, my = (ax + bx) * 0.5, (ay + by) * 0.5
            dist, (qx, qy) = nearest_road((mx, my))
            if dist < 1.0 or ((qx - mx) * ox + (qy - my) * oy) / dist < FIRE_ESCAPE_FACING:
                continue
            outside = [(mx + ox * 150.0 + ux * t, my + oy * 150.0 + uy * t) for t in (-100.0, 0.0, 100.0)]
            if any(inside_other(pt, rec["id"]) or geo.point_in_polygon(pt, ring) for pt in outside):
                continue
            candidates.append((dist, -length, i, (mx, my), (ox, oy)))
        if not candidates:
            continue
        nearest = min(cand[0] for cand in candidates)
        facing = [cand for cand in candidates if cand[0] <= nearest + FIRE_ESCAPE_STREET_SLACK]
        facing.sort(key=lambda cand: (cand[1], cand[2]))
        _dist, _neg, _index, (mx, my), (ox, oy) = facing[0]
        yaw = math.degrees(math.atan2(-ox, oy))

        floors = []
        floor = 1
        while floor * FIRE_ESCAPE_FLOOR <= height_cm - FIRE_ESCAPE_ROOF_CLEARANCE:
            floors.append(floor)
            floor += 1
        if not floors:
            continue
        quad = _landing_corners(mx, my, yaw)
        probe = quad + [((quad[0][0] + quad[2][0]) * 0.5, (quad[0][1] + quad[2][1]) * 0.5)]
        blocked = any(geo.point_in_polygon(pt, ring) for pt in probe)
        for f in floors:
            if blocked:
                break
            z = f * FIRE_ESCAPE_FLOOR
            if any(inside_other(pt, rec["id"], z - FIRE_ESCAPE_SLAB[2]) for pt in probe):
                blocked = True
            for lx, ly, lyaw in lamps:
                if z - FIRE_ESCAPE_SLAB[2] <= LAMP_POLE_HEIGHT and _point_quad_distance((lx, ly), quad) < FIRE_ESCAPE_LAMP_CLEARANCE + LAMP_POLE_DIAMETER * 0.5:
                    blocked = True
                hx = lx + math.cos(math.radians(lyaw)) * LAMP_ARM
                hy = ly + math.sin(math.radians(lyaw)) * LAMP_ARM
                if abs(z - LAMP_POLE_HEIGHT) < FIRE_ESCAPE_RAIL + 50.0 and _point_quad_distance((hx, hy), quad) < FIRE_ESCAPE_LAMP_CLEARANCE + 20.0:
                    blocked = True
        padded = _landing_corners(mx, my, yaw, FIRE_ESCAPE_GAP_TO_OTHER * 0.5)
        if not blocked and any(_rects_overlap(padded, other) for other in placed):
            blocked = True
        if blocked:
            continue
        placed.append(padded)
        for f in floors:
            drop = FIRE_ESCAPE_FLOOR if f > 1 else 0.0
            side = 1.0 if f % 2 == 0 else -1.0
            spots.append((rec["id"], f, (mx, my, f * FIRE_ESCAPE_FLOOR), yaw, drop, side))
    return spots


def fire_escape_class():
    """AFireEscapeLanding's UClass (the spawner's property wants the class object, not the Python type)."""
    return c.find_class("/Script/Hawkeye.FireEscapeLanding")


# The ledges, the anchors and the fire escapes are not saved in the map: generate_city writes
# them into a UCityLedgeData and City_LedgeSpawner (ACityLedgeSpawner) spawns them at BeginPlay.
CITY_PROPS_PATH = "/Game/City/EastVillage"
CITY_PROPS_NAME = "DA_EastVillage_CityProps"
SPAWNER_LABEL = "City_LedgeSpawner"

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


def _ledge_edge_index(label):
    return int(label.rsplit("_", 1)[1])


def city_props_hash(ledges, anchors, ledge_cls, anchor_cls, fire_escapes=None, clutter=None):
    """What the props asset was written from: every ledge, anchor, fire-escape spot and clutter
    instance, and the classes, meshes and materials."""
    parts = [[[s[0], list(s[2]), s[3], list(s[4])] for s in ledges],
             [list(a) for a in anchors], c.safe_name(ledge_cls), c.safe_name(anchor_cls)]
    if fire_escapes:
        parts.append([[f[0], f[1], list(f[2]), f[3], f[4], f[5]] for f in fire_escapes])
        parts.append([list(FIRE_ESCAPE_SLAB), FIRE_ESCAPE_GAP, FIRE_ESCAPE_RAIL, FIRE_ESCAPE_RAIL_THICK])
    if clutter:
        parts.append([[kind, c.safe_name(mesh), c.safe_name(material), collision, shadow,
                       [[round(v, 1) for v in (x.translation.x, x.translation.y, x.translation.z,
                                               x.rotation.rotator().yaw, x.scale3d.x)] for x in xforms]]
                      for kind, mesh, material, collision, shadow, xforms in clutter])
    return geo.record_hash(GENERATOR_VERSION, "props", *parts)


def _ledge_record(spot):
    label, osm, origin, yaw, scale, _a, _b, _spec = spot
    rec = unreal.CityLedgeRecord()
    rec.set_editor_property("transform", unreal.Transform(
        unreal.Vector(*origin), unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(*scale)))
    rec.set_editor_property("edge_index", _ledge_edge_index(label))
    rec.set_editor_property("osm_id", osm)
    return rec


def _anchor_record(index, spot):
    x, y, z, yaw, forward, drop, osm = spot
    rec = unreal.CityAnchorRecord()
    rec.set_editor_property("transform", unreal.Transform(
        unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(1.0, 1.0, 1.0)))
    rec.set_editor_property("landing_offset", unreal.Vector(forward, 0.0, -drop))
    rec.set_editor_property("index", index)
    rec.set_editor_property("osm_id", osm)
    return rec


def _fire_escape_record(spot):
    osm, floor, (x, y, z), yaw, drop, side = spot
    rec = unreal.CityFireEscapeRecord()
    rec.set_editor_property("transform", unreal.Transform(
        unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(1.0, 1.0, 1.0)))
    rec.set_editor_property("slab_size", unreal.Vector(*FIRE_ESCAPE_SLAB))
    rec.set_editor_property("facade_gap", FIRE_ESCAPE_GAP)
    rec.set_editor_property("rail_height", FIRE_ESCAPE_RAIL)
    rec.set_editor_property("rail_thickness", FIRE_ESCAPE_RAIL_THICK)
    rec.set_editor_property("ladder_drop", drop)
    rec.set_editor_property("ladder_side", side)
    rec.set_editor_property("floor", floor)
    rec.set_editor_property("osm_id", osm)
    return rec


def ensure_city_props(district):
    """DA_EastVillage_CityProps: every roof-edge ledge and grapple anchor as data. Returns
    (asset, ledge class, anchor class), the asset None on failure. Saved only when its hash changes."""
    ledge_cls = traversable_class()
    anchor_cls = anchor_class()
    if ledge_cls is None:
        c.log("FAILED", LEDGE_PREFIX + "*",
              "BP_TraversableBlock not found; run Tools\\create-content.ps1 (import_gasp, world blueprints)")
    if anchor_cls is None:
        c.log("FAILED", ANCHOR_PREFIX + "*", "no grapple anchor class; build the module")
    data_cls = getattr(unreal, "CityLedgeData", None)
    if data_cls is None:
        c.log("FAILED", CITY_PROPS_NAME, "UCityLedgeData not found; build the module")
        return None, ledge_cls, anchor_cls

    ledges = ledge_spots(district)
    anchors = anchor_spots(district)
    escapes = fire_escape_spots(district)
    clutter, plan = clutter_groups(district, ensure_clutter_meshes(), ensure_clutter_materials())
    want_hash = city_props_hash(ledges, anchors, ledge_cls, anchor_cls, escapes, clutter)
    full = c.asset_path(CITY_PROPS_PATH, CITY_PROPS_NAME)
    asset = c.load_or_none(full)
    if asset is None:
        factory = c.new_factory("DataAssetFactory")
        c.set_props(factory, [("data_asset_class", data_cls)], "DataAssetFactory")
        asset, _created = c.create_asset(CITY_PROPS_NAME, CITY_PROPS_PATH, data_cls, factory, quiet=True)
        if asset is None:
            return None, ledge_cls, anchor_cls
    summary = "{0} ledges on {1} buildings, {2} anchors on {3} roofs, {4} fire-escape landings on {5} buildings, clutter: {6}".format(
        len(ledges), len({s[1] for s in ledges}), len(anchors), len({a[6] for a in anchors}),
        len(escapes), len({f[0] for f in escapes}),
        ", ".join("{0} {1}".format(len(v), k) for k, v in sorted(plan.items()) if v))
    if str(asset.get_editor_property("source_hash")) == want_hash \
            and len(asset.get_editor_property("ledges")) == len(ledges) \
            and len(asset.get_editor_property("anchors")) == len(anchors) \
            and len(asset.get_editor_property("fire_escapes")) == len(escapes) \
            and len(asset.get_editor_property("clutter")) == len(clutter):
        c.log("exists", full, summary + "; hash unchanged")
        return asset, ledge_cls, anchor_cls
    asset.set_editor_property("ledges", [_ledge_record(spot) for spot in ledges])
    asset.set_editor_property("anchors", [_anchor_record(i, spot) for i, spot in enumerate(anchors)])
    asset.set_editor_property("fire_escapes", [_fire_escape_record(spot) for spot in escapes])
    asset.set_editor_property("clutter", [_clutter_record(group) for group in clutter])
    asset.set_editor_property("source_hash", want_hash)
    c.save(asset)
    c.log("updated", full, summary)
    return asset, ledge_cls, anchor_cls


def ensure_ledge_spawner(district, existing):
    """City_LedgeSpawner (ACityLedgeSpawner) pointing at the props asset, and none of the ledges or
    anchors the map used to carry as saved actors. Returns changes."""
    asset, ledge_cls, anchor_cls = ensure_city_props(district)
    changes = 0
    removed = 0
    for label, actor in list(existing.items()):
        rest = label[len(ANCHOR_PREFIX):] if label.startswith(ANCHOR_PREFIX) else ""
        if label.startswith(LEDGE_PREFIX) or rest.isdigit():
            actor.destroy_actor()
            existing.pop(label, None)
            removed += 1
    changes += removed

    spawner_cls = c.find_class("CityLedgeSpawner", "/Script/Hawkeye.CityLedgeSpawner")
    if spawner_cls is None or asset is None:
        c.log("FAILED", SPAWNER_LABEL, "no ACityLedgeSpawner class or no props asset")
        return changes
    spawner, n = _ensure_located(existing, SPAWNER_LABEL, spawner_cls, unreal.Vector(0.0, 0.0, 0.0), 0.0)
    changes += n
    if spawner is not None:
        props = [("data", asset), ("ledge_class", ledge_cls), ("anchor_class", anchor_cls), ("spawn_in_editor", False),
                 ("fire_escape_cube", c.load_or_none(CUBE)), ("fire_escape_cylinder", c.load_or_none(CYLINDER)),
                 ("fire_escape_material", m.ensure_steel_painted())]
        escape_cls = fire_escape_class()
        if escape_cls is not None:
            props.append(("fire_escape_class", escape_cls))
        changed = []
        for prop, value in props:
            have = spawner.get_editor_property(prop)
            if have != value and not (have is not None and value is not None
                                      and hasattr(have, "get_path_name") and hasattr(value, "get_path_name")
                                      and have.get_path_name() == value.get_path_name()):
                spawner.set_editor_property(prop, value)
                changed.append(prop)
        changes += len(changed)
        changes += _ensure_tags(spawner, ["City", "CityLedgeSpawner"])
        if changed:
            removed_note = ", ".join(changed)
            c.log("updated", SPAWNER_LABEL + " properties", removed_note)
    c.log("updated" if changes else "exists", SPAWNER_LABEL,
          "spawns the props at load; {0} saved ledge/anchor actors removed".format(removed))
    return changes


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
# clutter: rooftop water towers, HVAC boxes and chimneys; hydrants, bins and bags, scaffolding and
# parked cars on the street. Placed here from the OSM records, not by a PCG graph: PCG graphs can be
# built from Python (UPCGGraph.AddNodeOfType / AddEdge), but a PCG component only schedules its
# generation for a later world tick, and this headless commandlet never ticks, so the output could
# neither be saved nor checked by verify_city. The rules need the records (heights, roads, anchors,
# landings, patrol points) anyway. Every instance goes into the props asset as FCityClutterGroup and
# City_LedgeSpawner draws each group as one instanced mesh at load.
# --------------------------------------------------------------------------------------

CLUTTER_VERSION = 1                    # bump when a clutter mesh recipe changes
WATER_TOWER_MIN_HEIGHT_M = 15.0
WATER_TOWER_ONE_IN = 4
HVAC_ONE_IN = 2
CHIMNEY_ONE_IN = 2
CLUTTER_ROOF_MIN_HEIGHT_M = 8.0        # sheds and garages get nothing on the roof
ROOF_GRID = 100.0                      # cm; candidate spots on a roof
ROOF_EDGE_CLEAR = 90.0                 # cm from any roof edge to the prop's footprint
CHIMNEY_EDGE = (60.0, 110.0)           # a chimney's centre this far in from an edge
ANCHOR_SAME_ROOF = 200.0               # cm; anchors within this height of a roof count for it
ANCHOR_CLEAR = 100.0                   # cm from an anchor or its landing point to a prop's footprint
PROP_GAP = 80.0                        # cm between two props on one roof
HYDRANT_SPACING = 6000.0               # cm along each road, alternating sides
HYDRANT_KERB_INSET = 45.0
CROSSING_CLEAR = 500.0                 # cm from another road's carriageway: no hydrant, car or bin there
LAMP_CLEAR = 150.0                     # cm from a lamp pole
PATROL_CLEAR = 150.0                   # cm from the thug patrol line, its points and the thugs
TEST_BLOCK_CLEAR = 300.0               # cm from the parkour test blocks and the PlayerStart
BIN_ONE_IN = 3
BIN_FACADE_OUT = 55.0                  # cm out from the facade to a bin's centre
BIN_CORNER_IN = 90.0                   # cm along the facade from the corner
SCAFFOLD_BUILDINGS = 3
SCAFFOLD_BAY = (200.0, 110.0, 200.0)   # width along the facade, depth, height per level
SCAFFOLD_MIN_EDGE = 500.0
SCAFFOLD_MAX_HEIGHT = 1200.0           # cm; the work is on the lower floors
CAR_SIZE = (450.0, 180.0, 140.0)
CAR_SPACING = 700.0
CAR_KERB_GAP = 20.0
CAR_GAP_FRACTION = 0.3                 # of car spaces left empty
HYDRANT_NO_PARKING = 460.0             # cm either side of a hydrant (15 ft)

# kind: (mesh name, material key, collision, casts shadow, footprint radius cm)
CLUTTER_KINDS = {
    "WaterTower": ("SM_City_WaterTower", "WaterTower", True, True, 170.0),
    "HVAC": ("SM_City_HVAC", "HVAC", True, True, 120.0),
    "Chimney": ("SM_City_Chimney", "Chimney", True, True, 60.0),
    "Hydrant": ("SM_City_Hydrant", "Hydrant", True, False, 25.0),
    "Bin": ("SM_City_Bin", "Bin", True, False, 32.0),
    "TrashBag": ("SM_City_TrashBag", "TrashBag", True, False, 36.0),
    "Scaffold": ("SM_City_ScaffoldBay", "Scaffold", False, True, 0.0),
    "ParkedCar_Black": ("SM_City_ParkedCar", "CarBlack", True, True, 245.0),
    "ParkedCar_Purple": ("SM_City_ParkedCar", "CarPurple", True, True, 245.0),
    "ParkedCar_Grey": ("SM_City_ParkedCar", "CarGrey", True, True, 245.0),
    "ParkedCar_Navy": ("SM_City_ParkedCar", "CarNavy", True, True, 245.0),
}
# cm from the base to the top, for the fire-escape landing check.
CLUTTER_HEIGHTS = {"WaterTower": 710.0, "HVAC": 130.0, "Chimney": 170.0, "Scaffold": SCAFFOLD_BAY[2]}
LANDING_CLEAR = 30.0                   # cm between a prop and a fire-escape landing it shares height with
CAR_KINDS = ("ParkedCar_Black", "ParkedCar_Purple", "ParkedCar_Grey", "ParkedCar_Navy")

# MI_Prop_<key>: (colour, roughness), from the palette: purple, cream, grey, black.
CLUTTER_MATERIALS = {
    "WaterTower": ((0.12, 0.08, 0.06), 0.9),
    "HVAC": ((0.30, 0.30, 0.31), 0.5),
    "Chimney": ((0.20, 0.07, 0.05), 0.9),
    "Hydrant": ((0.35, 0.05, 0.035), 0.45),
    "Bin": ((0.03, 0.06, 0.045), 0.6),
    "TrashBag": ((0.012, 0.012, 0.014), 0.25),
    "Scaffold": ((0.22, 0.22, 0.23), 0.5),
    "CarBlack": ((0.015, 0.015, 0.017), 0.3),
    "CarPurple": ((0.07, 0.025, 0.10), 0.3),
    "CarGrey": ((0.10, 0.10, 0.11), 0.35),
    "CarNavy": ((0.02, 0.03, 0.07), 0.3),
    "Ground": ((0.10, 0.10, 0.10), 0.9),
}


def stable_hash(*parts):
    """0..1 from the parts' text, the same on every run and machine."""
    text = "|".join(str(p) for p in parts)
    return (zlib.crc32(text.encode("utf-8")) & 0xffffffff) / 4294967296.0


def _bx(mesh, cx, cy, z0, sx, sy, sz, yaw=0.0):
    xf = unreal.Transform(location=unreal.Vector(cx, cy, z0), rotation=unreal.Rotator(0.0, 0.0, yaw))
    GS_PRIM.append_box(mesh, unreal.GeometryScriptPrimitiveOptions(), xf, sx, sy, sz, 0, 0, 0,
                       unreal.GeometryScriptPrimitiveOriginMode.BASE)


def _cyl(mesh, cx, cy, z0, radius, height, steps=12, roll=0.0):
    xf = unreal.Transform(location=unreal.Vector(cx, cy, z0), rotation=unreal.Rotator(roll, 0.0, 0.0))
    GS_PRIM.append_cylinder(mesh, unreal.GeometryScriptPrimitiveOptions(), xf, radius, height, steps, 0, True,
                            unreal.GeometryScriptPrimitiveOriginMode.BASE)


def _build_water_tower():
    mesh = new_mesh()
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            _bx(mesh, sx * 95.0, sy * 95.0, 0.0, 18.0, 18.0, 260.0)
    _bx(mesh, 0.0, 0.0, 260.0, 330.0, 330.0, 12.0)
    _cyl(mesh, 0.0, 0.0, 272.0, 150.0, 320.0, 16)
    GS_PRIM.append_cone(mesh, unreal.GeometryScriptPrimitiveOptions(),
                        unreal.Transform(location=unreal.Vector(0.0, 0.0, 592.0)), 165.0, 8.0, 110.0, 16, 0, True,
                        unreal.GeometryScriptPrimitiveOriginMode.BASE)
    return mesh


def _build_hvac():
    mesh = new_mesh()
    _bx(mesh, 0.0, 0.0, 0.0, 200.0, 130.0, 110.0)
    _cyl(mesh, 40.0, 0.0, 110.0, 45.0, 18.0, 16)
    return mesh


def _build_chimney():
    mesh = new_mesh()
    _bx(mesh, 0.0, 0.0, 0.0, 70.0, 70.0, 160.0)
    _bx(mesh, 0.0, 0.0, 160.0, 86.0, 86.0, 10.0)
    return mesh


def _build_hydrant():
    mesh = new_mesh()
    _cyl(mesh, 0.0, 0.0, 0.0, 18.0, 6.0, 12)
    _cyl(mesh, 0.0, 0.0, 6.0, 13.0, 52.0, 12)
    _cyl(mesh, 0.0, 0.0, 58.0, 16.0, 8.0, 12)
    _cyl(mesh, 0.0, 0.0, 66.0, 8.0, 8.0, 8)
    _bx(mesh, 0.0, 0.0, 32.0, 44.0, 10.0, 10.0)
    return mesh


def _build_bin():
    mesh = new_mesh()
    _cyl(mesh, 0.0, 0.0, 0.0, 30.0, 85.0, 12)
    return mesh


def _build_trash_bag():
    mesh = new_mesh()
    GS_PRIM.append_sphere_lat_long(mesh, unreal.GeometryScriptPrimitiveOptions(),
                                   unreal.Transform(location=unreal.Vector(0.0, 0.0, 26.0), scale=unreal.Vector(1.0, 1.0, 0.75)),
                                   35.0, 8, 10, unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    return mesh


def _build_scaffold_bay():
    """One bay: local X along the facade (0..W), Y out of it (0..D), Z up (0..H); deck on top."""
    w, d, h = SCAFFOLD_BAY
    mesh = new_mesh()
    for px in (2.5, w - 2.5):
        for py in (2.5, d - 2.5):
            _bx(mesh, px, py, 0.0, 5.0, 5.0, h)
    for py in (2.5, d - 2.5):
        _bx(mesh, w * 0.5, py, h * 0.5, w, 4.0, 4.0)
    _bx(mesh, w * 0.5, d - 2.5, h - 100.0, w, 4.0, 4.0)
    _bx(mesh, w * 0.5, d * 0.5, h - 4.0, w, d, 4.0)
    return mesh


def _build_parked_car():
    """A dark block car, 4.5 x 1.8 x 1.4 m: body, cabin set back, four wheels. Origin at the ground,
    centred, +X forward."""
    L, W, H = CAR_SIZE
    mesh = new_mesh()
    _bx(mesh, 0.0, 0.0, 28.0, L, W, 67.0)
    _bx(mesh, -20.0, 0.0, 95.0, 250.0, W - 16.0, H - 95.0)
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            xf = unreal.Transform(location=unreal.Vector(sx * 140.0, sy * (W * 0.5 - 11.0), 33.0),
                                  rotation=unreal.Rotator(90.0, 0.0, 0.0))
            GS_PRIM.append_cylinder(mesh, unreal.GeometryScriptPrimitiveOptions(), xf, 33.0, 22.0, 12, 0, True,
                                    unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    return mesh


CLUTTER_MESHES = {
    "SM_City_WaterTower": _build_water_tower,
    "SM_City_HVAC": _build_hvac,
    "SM_City_Chimney": _build_chimney,
    "SM_City_Hydrant": _build_hydrant,
    "SM_City_Bin": _build_bin,
    "SM_City_TrashBag": _build_trash_bag,
    "SM_City_ScaffoldBay": _build_scaffold_bay,
    "SM_City_ParkedCar": _build_parked_car,
}


def ensure_clutter_meshes():
    """{mesh name: StaticMesh}; each rebuilt only when CLUTTER_VERSION changes."""
    out = {}
    c.ensure_directory(MESH_DIR)
    for name, build in sorted(CLUTTER_MESHES.items()):
        spec = geo.record_hash(GENERATOR_VERSION, CLUTTER_VERSION, "clutter", name, list(CAR_SIZE), list(SCAFFOLD_BAY))
        asset, current = stored_hash(name)
        if asset is not None and current == spec:
            out[name] = asset
            continue
        try:
            result = write_static_mesh(build(), name, spec)
        except Exception as exc:  # noqa: BLE001
            c.log_error("clutter mesh " + name, exc)
            result = None
        if result is not None:
            out[name] = result[0]
            c.log(result[1], c.asset_path(MESH_DIR, name))
    c.log("exists", "clutter meshes", "{0} of {1} present".format(len(out), len(CLUTTER_MESHES)))
    return out


def ensure_clutter_materials():
    return {key: m.ensure_prop_instance(key, rgb, rough) for key, (rgb, rough) in sorted(CLUTTER_MATERIALS.items())}


def _near_segment(pt, a, b):
    return closest_point_on_polyline(pt, [a, b])[0]


def clutter_keepouts(district):
    """What street clutter keeps clear of: [(kind, (x, y), radius)] points and patrol segments."""
    points = []
    for x, y, _yaw, _shadows in lamp_spots(district):
        points.append(("lamp", (x, y), LAMP_CLEAR))
    thugs, patrol_points, _roof = thug_placements(district)
    for t in thugs:
        points.append(("thug", (t[1], t[2]), PATROL_CLEAR))
    for p in patrol_points:
        points.append(("patrol", (p[1], p[2]), PATROL_CLEAR))
    loc, _rot = player_start_transform(district)
    points.append(("start", (loc.x, loc.y), TEST_BLOCK_CLEAR))
    door = safehouse_keepout(district)
    if door is not None:
        points.append(("safehouse", door, SAFEHOUSE_KEEPOUT))
    for label, _tag, origin, yaw, scale in test_block_spots(district):
        ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        cx = origin[0] + ux * scale[0] * 50.0 - uy * scale[1] * 50.0
        cy = origin[1] + uy * scale[0] * 50.0 + ux * scale[1] * 50.0
        points.append(("test_block", (cx, cy), TEST_BLOCK_CLEAR + max(scale[0], scale[1]) * 50.0))
    segments = []
    for a, b in zip(patrol_points[0::2], patrol_points[1::2]):
        segments.append(((a[1], a[2]), (b[1], b[2])))
    return points, segments


def clutter_plan(district):
    """{kind: [(x, y, z, yaw, scale)]} in a stable order, plus notes for the log. The rules:

    * roofs of buildings over CLUTTER_ROOF_MIN_HEIGHT_M, never the three objective roofs (their
      beacons, volumes and the RoofPair stay clear): a water tower on 1 in WATER_TOWER_ONE_IN roofs
      over WATER_TOWER_MIN_HEIGHT_M, an HVAC box on 1 in HVAC_ONE_IN, one or two chimneys near an
      edge on 1 in CHIMNEY_ONE_IN; each footprint ROOF_EDGE_CLEAR inside the roof, ANCHOR_CLEAR from
      every grapple anchor and landing point, PROP_GAP from the others
    * a hydrant every HYDRANT_SPACING along each road at the kerb, sides alternating
    * a bin and two or three bags at a street corner of 1 in BIN_ONE_IN buildings, against the facade
    * scaffolding up to SCAFFOLD_MAX_HEIGHT over the street facade of SCAFFOLD_BUILDINGS buildings with
      no fire escape
    * parked cars every CAR_SPACING along the side of each road away from the park, none near a
      crossing or within HYDRANT_NO_PARKING of a hydrant, CAR_GAP_FRACTION left empty
    Street clutter keeps clear of lamps, the thug patrol, the PlayerStart and the parkour test blocks.
    """
    plan = {kind: [] for kind in CLUTTER_KINDS}
    # The archers' roofs and the roofs their lines cross stay bare, so nothing stands in the duel.
    objective_ids = {rec["id"] for rec in objective_roofs(district).values()} | archer_quiet_roofs(district)
    anchors = anchor_spots(district)
    anchor_pts = []   # (x, y, z): every anchor and its landing point
    for x, y, z, yaw, forward, _drop, _osm in anchors:
        lx, ly = x + math.cos(math.radians(yaw)) * forward, y + math.sin(math.radians(yaw)) * forward
        anchor_pts.extend([(x, y, z), (lx, ly, z)])
    escapes = fire_escape_spots(district)
    escape_ids = {f[0] for f in escapes}
    landing_quads = [_landing_corners(f[2][0], f[2][1], f[3], FIRE_ESCAPE_GAP_TO_OTHER) for f in escapes]
    landings = [(quad, f[2][2] - FIRE_ESCAPE_SLAB[2], f[2][2] + FIRE_ESCAPE_RAIL, geo.bounds(quad))
                for quad, f in zip(landing_quads, escapes)]

    def landings_near(box, pad=400.0):
        x0, y0, x1, y1 = box
        return [l for l in landings if l[3][0] < x1 + pad and l[3][2] > x0 - pad and l[3][1] < y1 + pad and l[3][3] > y0 - pad]

    def hits_landing(px, py, radius, z0, z1, near):
        return any(lo < z1 and hi > z0 and _point_quad_distance((px, py), quad) < radius + LANDING_CLEAR
                   for quad, lo, hi, _box in near)

    buildings = []
    for rec in sorted(district.buildings, key=lambda r: r["id"]):
        ring = geo.clean_ring(district.ring_cm(rec["outer"]), min_edge=5.0, collinear_tol=2.0)
        if len(ring) >= 3:
            buildings.append((rec, ring, geo.bounds(ring)))

    # --- roofs ---
    for rec, ring, (x0, y0, x1, y1) in buildings:
        h = rec["height_m"]
        if h < CLUTTER_ROOF_MIN_HEIGHT_M or rec["id"] in objective_ids:
            continue
        roof_z = h * 100.0
        # This roof's anchors and any neighbour's at about the same height (theirs sit on a shared
        # parapet line, and Kate lands on this roof from them).
        avoid = [(ax, ay) for ax, ay, az in anchor_pts if abs(az - roof_z) < ANCHOR_SAME_ROOF
                 and x0 - 400.0 <= ax <= x1 + 400.0 and y0 - 400.0 <= ay <= y1 + 400.0]
        near = landings_near((x0, y0, x1, y1))
        placed = []   # (x, y, radius)
        grid = []
        gx = x0 + ROOF_GRID * 0.5
        while gx < x1:
            gy = y0 + ROOF_GRID * 0.5
            while gy < y1:
                if geo.point_in_polygon((gx, gy), ring):
                    grid.append((gx, gy, _edge_distance((gx, gy), ring)))
                gy += ROOF_GRID
            gx += ROOF_GRID

        def free(px, py, radius, edge):
            if edge < radius + ROOF_EDGE_CLEAR:
                return False
            if any(math.hypot(px - ax, py - ay) < radius + ANCHOR_CLEAR for ax, ay in avoid):
                return False
            return all(math.hypot(px - qx, py - qy) >= radius + qr + PROP_GAP for qx, qy, qr in placed)

        def pick(kind, salt, want_edge=None):
            radius = CLUTTER_KINDS[kind][4]
            options = []
            for px, py, edge in grid:
                if hits_landing(px, py, radius, roof_z, roof_z + CLUTTER_HEIGHTS[kind], near):
                    continue
                if want_edge is not None:
                    if not (want_edge[0] <= edge <= want_edge[1]):
                        continue
                    if any(math.hypot(px - ax, py - ay) < radius + ANCHOR_CLEAR for ax, ay in avoid):
                        continue
                    if not all(math.hypot(px - qx, py - qy) >= radius + qr + PROP_GAP for qx, qy, qr in placed):
                        continue
                elif not free(px, py, radius, edge):
                    continue
                options.append((px, py))
            if not options:
                return None
            px, py = options[int(stable_hash(rec["id"], kind, salt) * len(options)) % len(options)]
            placed.append((px, py, radius))
            return px, py

        yaw0 = oriented_rect(ring)[4]
        if h >= WATER_TOWER_MIN_HEIGHT_M and stable_hash(rec["id"], "tower") < 1.0 / WATER_TOWER_ONE_IN:
            spot = pick("WaterTower", 0)
            if spot:
                s = 0.8 + 0.25 * stable_hash(rec["id"], "tower-scale")
                plan["WaterTower"].append((spot[0], spot[1], roof_z, yaw0 + 45.0 * stable_hash(rec["id"], "ty"), s))
        if stable_hash(rec["id"], "hvac") < 1.0 / HVAC_ONE_IN:
            spot = pick("HVAC", 0)
            if spot:
                plan["HVAC"].append((spot[0], spot[1], roof_z, yaw0, 1.0))
        if stable_hash(rec["id"], "chimney") < 1.0 / CHIMNEY_ONE_IN:
            for n in range(1 + int(stable_hash(rec["id"], "chimneys") * 2.0)):
                spot = pick("Chimney", n, CHIMNEY_EDGE)
                if spot:
                    plan["Chimney"].append((spot[0], spot[1], roof_z, yaw0, 1.0))

    # --- street ---
    roads = sorted(road_paths(district), key=lambda rp: rp[0]["id"])
    def road_key(rec):
        return rec.get("name") or rec["id"]

    carriageway = [(p, q, rec["width_m"] * 50.0, road_key(rec)) for rec, paths in roads
                   for path in paths for p, q in zip(path, path[1:])]
    ground = ground_ring_cm(district)
    parks = [district.ring_cm(p["outer"]) for p in district.parks]
    points, segments = clutter_keepouts(district)

    def in_carriageway(pt, pad=30.0, skip_road=None):
        return any(_near_segment(pt, p, q) < half + pad for p, q, half, rid in carriageway if rid != skip_road)

    def near_crossing(pt, own_road):
        return in_carriageway(pt, CROSSING_CLEAR, own_road)

    def in_building(pt, clearance):
        for rec, ring, (x0, y0, x1, y1) in buildings:
            if x0 - clearance <= pt[0] <= x1 + clearance and y0 - clearance <= pt[1] <= y1 + clearance:
                if geo.point_in_polygon(pt, ring) or ring_distance(pt, ring) < clearance:
                    return True
        return False

    def kept_out(pt, radius):
        if any(math.hypot(pt[0] - q[0], pt[1] - q[1]) < r + radius for _k, q, r in points):
            return True
        return any(_near_segment(pt, a, b) < PATROL_CLEAR + radius for a, b in segments)

    def street_ok(pt, radius, road_id=None, crossing=True):
        if not geo.point_in_polygon(pt, ground) or in_building(pt, radius + 5.0) or kept_out(pt, radius):
            return False
        if any(geo.point_in_polygon(pt, park) for park in parks):
            return False
        return not (crossing and road_id is not None and near_crossing(pt, road_id))

    # Hydrants at the kerb.
    hydrants = []
    for rec, paths in roads:
        offset = rec["width_m"] * 50.0 + HYDRANT_KERB_INSET
        side = 1.0
        for path in paths:
            s_next = HYDRANT_SPACING * 0.5
            walked = 0.0
            for (ax, ay), (bx, by) in zip(path, path[1:]):
                L = math.hypot(bx - ax, by - ay)
                if L < 1.0:
                    continue
                ux, uy = (bx - ax) / L, (by - ay) / L
                while s_next <= walked + L:
                    t = s_next - walked
                    px, py = ax + ux * t, ay + uy * t
                    for try_side in (side, -side):
                        pt = (px - uy * offset * try_side, py + ux * offset * try_side)
                        if in_carriageway(pt, 10.0) or not street_ok(pt, CLUTTER_KINDS["Hydrant"][4], road_key(rec)):
                            continue
                        if any(math.hypot(pt[0] - q[0], pt[1] - q[1]) < HYDRANT_SPACING * 0.3 for q in hydrants):
                            continue
                        hydrants.append(pt)
                        plan["Hydrant"].append((pt[0], pt[1], SIDEWALK_TOP, math.degrees(math.atan2(uy, ux)), 1.0))
                        break
                    side = -side
                    s_next += HYDRANT_SPACING
                walked += L
    for x, y, *_rest in plan["Hydrant"]:
        points.append(("hydrant", (x, y), 60.0))

    # A bin and bags at one street corner of some buildings.
    segments_all = [(p, q) for p, q, _h, _r in carriageway]
    for rec, ring, _box in buildings:
        if rec["height_m"] < CLUTTER_ROOF_MIN_HEIGHT_M or stable_hash(rec["id"], "bin") >= 1.0 / BIN_ONE_IN:
            continue
        inward = 1.0 if geo.is_ccw(ring) else -1.0
        n = len(ring)
        best = None
        for i in range(n):
            ax, ay = ring[i]
            bx, by = ring[(i + 1) % n]
            L = math.hypot(bx - ax, by - ay)
            if L < 2.0 * BIN_CORNER_IN + 100.0:
                continue
            ox, oy = (by - ay) / L * inward, -(bx - ax) / L * inward
            mx, my = (ax + bx) * 0.5, (ay + by) * 0.5
            dist = min(_near_segment((mx, my), p, q) for p, q in segments_all)
            if best is None or dist < best[0]:
                best = (dist, (ax, ay), (bx, by), (ox, oy), L)
        if best is None:
            continue
        _d, a, b, (ox, oy), L = best
        ux, uy = (b[0] - a[0]) / L, (b[1] - a[1]) / L
        end = 1.0 if stable_hash(rec["id"], "bin-end") < 0.5 else -1.0
        corner = b if end > 0 else a
        along = -end
        cx = corner[0] + ux * along * BIN_CORNER_IN + ox * BIN_FACADE_OUT
        cy = corner[1] + uy * along * BIN_CORNER_IN + oy * BIN_FACADE_OUT
        if in_carriageway((cx, cy), 150.0) or not street_ok((cx, cy), CLUTTER_KINDS["Bin"][4]):
            continue
        yaw = math.degrees(math.atan2(uy, ux))
        plan["Bin"].append((cx, cy, SIDEWALK_TOP, yaw, 1.0))
        bags = 2 + int(stable_hash(rec["id"], "bags") * 2.0)
        for k in range(bags):
            t = (k + 1) * 60.0
            bx_, by_ = cx + ux * along * t + ox * (5.0 * (k % 2)), cy + uy * along * t + oy * (5.0 * (k % 2))
            if street_ok((bx_, by_), CLUTTER_KINDS["TrashBag"][4]) and not in_carriageway((bx_, by_), 150.0):
                plan["TrashBag"].append((bx_, by_, SIDEWALK_TOP, yaw + 70.0 * k, 0.85 + 0.3 * stable_hash(rec["id"], k)))

    # Scaffolding on a few tenements without fire escapes.
    candidates = []
    for rec, ring, _box in buildings:
        h = rec["height_m"]
        if h < CLUTTER_ROOF_MIN_HEIGHT_M or rec["id"] in escape_ids or rec["id"] in objective_ids:
            continue
        inward = 1.0 if geo.is_ccw(ring) else -1.0
        n = len(ring)
        for i in range(n):
            ax, ay = ring[i]
            bx, by = ring[(i + 1) % n]
            L = math.hypot(bx - ax, by - ay)
            if L < SCAFFOLD_MIN_EDGE:
                continue
            ox, oy = (by - ay) / L * inward, -(bx - ax) / L * inward
            mx, my = (ax + bx) * 0.5, (ay + by) * 0.5
            probe = (mx + ox * (SCAFFOLD_BAY[1] + 50.0), my + oy * (SCAFFOLD_BAY[1] + 50.0))
            if in_carriageway(probe, 50.0) or in_building(probe, 20.0):
                continue
            if min(_near_segment((mx, my), p, q) for p, q in segments_all) > 1500.0:
                continue
            candidates.append((stable_hash(rec["id"], "scaffold"), rec, (ax, ay), (bx, by), (ox, oy), L))
            break
    candidates.sort(key=lambda cand: cand[0])
    chosen = 0
    for _h, rec, a, b, (ox, oy), L in candidates:
        if chosen >= SCAFFOLD_BUILDINGS:
            break
        ux, uy = (b[0] - a[0]) / L, (b[1] - a[1]) / L
        bays = int((L - 60.0) // SCAFFOLD_BAY[0])
        levels = int(min(rec["height_m"] * 100.0 - 60.0, SCAFFOLD_MAX_HEIGHT) // SCAFFOLD_BAY[2])
        start = (L - bays * SCAFFOLD_BAY[0]) * 0.5
        yaw = math.degrees(math.atan2(uy, ux))
        # The bay's local +Y must point out of the facade: flip the run if yaw turns it inward.
        if (-math.sin(math.radians(yaw))) * ox + math.cos(math.radians(yaw)) * oy < 0.0:
            a, b, ux, uy = b, a, -ux, -uy
            yaw = math.degrees(math.atan2(uy, ux))
        gap = 8.0
        quad = [(a[0] + ox * gap, a[1] + oy * gap), (b[0] + ox * gap, b[1] + oy * gap),
                (b[0] + ox * (gap + SCAFFOLD_BAY[1]), b[1] + oy * (gap + SCAFFOLD_BAY[1])),
                (a[0] + ox * (gap + SCAFFOLD_BAY[1]), a[1] + oy * (gap + SCAFFOLD_BAY[1]))]
        if any(_rects_overlap(quad, other) for other in landing_quads):
            continue
        steps = int(L // 100.0) + 1
        edge_pts = [(quad[0][0] + (quad[1][0] - quad[0][0]) * k / steps + (quad[3][0] - quad[0][0]) * f,
                     quad[0][1] + (quad[1][1] - quad[0][1]) * k / steps + (quad[3][1] - quad[0][1]) * f)
                    for k in range(steps + 1) for f in (0.0, 1.0)]
        if any(kept_out(pt, 60.0) for pt in edge_pts):
            continue
        chosen += 1
        for i in range(bays):
            for j in range(levels):
                bx_ = a[0] + ux * (start + i * SCAFFOLD_BAY[0]) + ox * gap
                by_ = a[1] + uy * (start + i * SCAFFOLD_BAY[0]) + oy * gap
                plan["Scaffold"].append((bx_, by_, SIDEWALK_TOP + j * SCAFFOLD_BAY[2], yaw, 1.0))

    # Parked cars along the side of each road away from the park.
    for rec, paths in roads:
        half = rec["width_m"] * 50.0
        offset = half - CAR_SIZE[1] * 0.5 - CAR_KERB_GAP
        for pi, path in enumerate(paths):
            if len(path) < 2:
                continue
            (ax, ay), (bx, by) = path[0], path[-1]
            L0 = math.hypot(bx - ax, by - ay) or 1.0
            nx0, ny0 = -(by - ay) / L0, (bx - ax) / L0
            mid = ((ax + bx) * 0.5, (ay + by) * 0.5)
            side = 1.0 if stable_hash(rec["id"], "car-side") < 0.5 else -1.0
            if parks:
                d_plus = min(ring_distance((mid[0] + nx0 * offset, mid[1] + ny0 * offset), p) for p in parks)
                d_minus = min(ring_distance((mid[0] - nx0 * offset, mid[1] - ny0 * offset), p) for p in parks)
                if min(d_plus, d_minus) < 3000.0:
                    side = 1.0 if d_plus > d_minus else -1.0
            walked = 0.0
            s_next = CAR_SPACING * 0.5
            for (sx, sy), (ex, ey) in zip(path, path[1:]):
                L = math.hypot(ex - sx, ey - sy)
                if L < 1.0:
                    continue
                ux, uy = (ex - sx) / L, (ey - sy) / L
                nx, ny = -uy, ux
                while s_next <= walked + L:
                    t = s_next - walked
                    px, py = sx + ux * t + nx * offset * side, sy + uy * t + ny * offset * side
                    s_next += CAR_SPACING
                    slot = "{0}:{1}:{2:.0f}".format(rec["id"], pi, s_next)
                    if stable_hash(slot, "gap") < CAR_GAP_FRACTION:
                        continue
                    nose = (px + ux * CAR_SIZE[0] * 0.5, py + uy * CAR_SIZE[0] * 0.5)
                    tail = (px - ux * CAR_SIZE[0] * 0.5, py - uy * CAR_SIZE[0] * 0.5)
                    if not all(geo.point_in_polygon(q, ground) for q in (nose, tail)):
                        continue
                    if any(in_carriageway(q, CROSSING_CLEAR * 0.6, road_key(rec)) for q in (nose, tail, (px, py))):
                        continue
                    if any(math.hypot(px - hx, py - hy) < HYDRANT_NO_PARKING for hx, hy in hydrants):
                        continue
                    if in_building((px, py), CAR_SIZE[1]) or kept_out((px, py), CLUTTER_KINDS[CAR_KINDS[0]][4]):
                        continue
                    kind = CAR_KINDS[int(stable_hash(slot, "colour") * len(CAR_KINDS)) % len(CAR_KINDS)]
                    yaw = math.degrees(math.atan2(uy, ux)) + (180.0 if stable_hash(slot, "dir") < 0.15 else 0.0)
                    plan[kind].append((px, py, ROAD_TOP, yaw, 1.0))
                walked += L
    return plan


def clutter_groups(district, meshes, materials):
    """[(kind, mesh, material, collision, shadow, [Transform])] for the props asset, and the plan."""
    plan = clutter_plan(district)
    groups = []
    for kind in sorted(plan):
        mesh_name, mat_key, collision, shadow, _radius = CLUTTER_KINDS[kind]
        xforms = [unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(s, s, s))
                  for x, y, z, yaw, s in plan[kind]]
        if xforms:
            groups.append((kind, meshes.get(mesh_name), materials.get(mat_key), collision, shadow, xforms))
    return groups, plan


def _clutter_record(group):
    kind, mesh, material, collision, shadow, xforms = group
    rec = unreal.CityClutterGroup()
    rec.set_editor_property("kind", kind)
    rec.set_editor_property("mesh", mesh)
    rec.set_editor_property("material", material)
    rec.set_editor_property("collision", collision)
    rec.set_editor_property("cast_shadow", shadow)
    rec.set_editor_property("instances", xforms)
    return rec


# --------------------------------------------------------------------------------------
# level
# --------------------------------------------------------------------------------------


# --------------------------------------------------------------------------------------
# the safehouse: a lit purple door in a storefront across from the park
# --------------------------------------------------------------------------------------

# ASafehouse (Source/Hawkeye/World/Safehouse.h) on the ground floor of a building that fronts
# Tompkins Square Park: SAFEHOUSE_PARK_REACH of the park, at least SAFEHOUSE_MIN_HEIGHT_M tall, not
# an objective roof, and more than SAFEHOUSE_START_CLEAR from the PlayerStart so the opening
# street shots keep their facades. Of those, the one nearest the start (a short walk). The door
# goes on its facade at the point nearest the park, facing out, standing on the sidewalk.
SAFEHOUSE_LABEL = "City_Safehouse"
SAFEHOUSE_ID = "ch01_east_7th"
SAFEHOUSE_PARK_REACH = 3000.0       # cm between the building and the park's edge
SAFEHOUSE_MIN_HEIGHT_M = 6.0
SAFEHOUSE_START_CLEAR = 2500.0      # cm
SAFEHOUSE_EDGE_MIN = 400.0          # cm; the facade the door is in must be at least this long
SAFEHOUSE_EDGE_INSET = 150.0        # cm from either end of that facade
SAFEHOUSE_KEEPOUT = 250.0           # cm of pavement in front of the door that clutter leaves clear
SAFEHOUSE_CLASS = "/Script/Hawkeye.Safehouse"


def _safehouse_door(ring, target):
    """(x, y, yaw, edge length) of the door on ring's edge nearest target, or None."""
    best = None
    n = len(ring)
    for i in range(n):
        a, b = ring[i], ring[(i + 1) % n]
        hit = closest_point_on_polyline(target, [a, b])
        if hit is not None and (best is None or hit[0] < best[0]):
            best = (hit[0], i)
    if best is None:
        return None
    a, b = ring[best[1]], ring[(best[1] + 1) % n]
    length = math.hypot(b[0] - a[0], b[1] - a[1])
    if length < SAFEHOUSE_EDGE_MIN:
        return None
    ux, uy = (b[0] - a[0]) / length, (b[1] - a[1]) / length
    t = (target[0] - a[0]) * ux + (target[1] - a[1]) * uy
    t = max(SAFEHOUSE_EDGE_INSET, min(length - SAFEHOUSE_EDGE_INSET, t))
    x, y = a[0] + ux * t, a[1] + uy * t
    nx, ny = uy, -ux
    if geo.point_in_polygon((x + nx * 20.0, y + ny * 20.0), ring):
        nx, ny = -nx, -ny
    return x, y, math.degrees(math.atan2(ny, nx)), length


def safehouse_spot(district):
    """{'rec', 'x', 'y', 'yaw', 'address'} for the safehouse door, or None."""
    if not district.parks:
        return None
    park = district.ring_cm(district.parks[0]["outer"])
    park_centre = geo.centroid(park)
    start, _rot = player_start_transform(district)
    start = (start.x, start.y)
    taken = {rec["id"] for rec in objective_roofs(district).values()}
    candidates = []
    for rec in district.buildings:
        if rec["id"] in taken or rec["height_m"] < SAFEHOUSE_MIN_HEIGHT_M:
            continue
        ring = geo.clean_ring(district.ring_cm(rec["outer"]), min_edge=5.0, collinear_tol=2.0)
        if len(ring) < 3 or ring_distance(start, ring) <= SAFEHOUSE_START_CLEAR:
            continue
        reach = min(ring_distance(p, ring) for p in park)
        if reach > SAFEHOUSE_PARK_REACH:
            continue
        door = _safehouse_door(ring, park_centre)
        if door is None:
            continue
        candidates.append((ring_distance(start, ring), rec["id"], rec, door))
    if not candidates:
        return None
    _d, _id, rec, door = min(candidates, key=lambda c_: (c_[0], c_[1]))
    tags = rec.get("tags", {})
    address = "{0} {1}".format(tags.get("addr:housenumber") or "", tags.get("addr:street") or "").strip()
    return {"rec": rec, "x": door[0], "y": door[1], "yaw": door[2], "address": address}


def safehouse_keepout(district):
    """(x, y) of the pavement in front of the safehouse door, or None."""
    spot = safehouse_spot(district)
    if spot is None:
        return None
    yaw = math.radians(spot["yaw"])
    return spot["x"] + math.cos(yaw) * 150.0, spot["y"] + math.sin(yaw) * 150.0


def _ensure_part_material(actor, prop, material, label):
    comp = actor.get_editor_property(prop)
    if comp is None or material is None:
        return 0
    overrides = comp.get_editor_property("override_materials")
    if len(overrides) >= 1 and overrides[0] == material:
        return 0
    comp.set_material(0, material)
    c.log("updated", label, "{0} material = {1}".format(prop, material.get_name()))
    return 1


def ensure_safehouse(district, existing):
    """City_Safehouse (ASafehouse) on its storefront. Idempotent by label."""
    cls = c.find_class("Safehouse", SAFEHOUSE_CLASS)
    if cls is None:
        c.log("skipped", SAFEHOUSE_LABEL, "ASafehouse not exposed; build the module")
        return 0
    spot = safehouse_spot(district)
    if spot is None:
        c.log("FAILED", SAFEHOUSE_LABEL, "no building fronting the park qualifies")
        return 0
    x, y, yaw = spot["x"], spot["y"], spot["yaw"]
    ground = ground_z(x + math.cos(math.radians(yaw)) * 60.0, y + math.sin(math.radians(yaw)) * 60.0,
                      SIDEWALK_TOP, existing)
    actor, changes = _ensure_located(existing, SAFEHOUSE_LABEL, cls, unreal.Vector(x, y, ground), yaw)
    if actor is None:
        return changes
    changes += _ensure_tags(actor, ["City", "CitySafehouse", "osm:" + spot["rec"]["id"]])
    if str(actor.get_editor_property("safehouse_id")) != SAFEHOUSE_ID:
        actor.set_editor_property("safehouse_id", unreal.Name(SAFEHOUSE_ID))
        changes += 1
    name = spot["address"] or "Tompkins Square"
    if str(actor.get_editor_property("display_name")) != name:
        actor.set_editor_property("display_name", unreal.Text(name))
        changes += 1
    door = c.load_or_none(MI_BEACON)
    trim = m.ensure_steel_painted()
    changes += _ensure_part_material(actor, "door", door, SAFEHOUSE_LABEL)
    changes += _ensure_part_material(actor, "door_frame", trim, SAFEHOUSE_LABEL)
    changes += _ensure_part_material(actor, "sign_board", trim, SAFEHOUSE_LABEL)
    c.log("updated" if changes else "exists", SAFEHOUSE_LABEL, "osm {0} ({1}), door at ({2:.0f}, {3:.0f}, {4:.0f}) yaw {5:.0f}".format(
        spot["rec"]["id"], name, x, y, ground, yaw))
    return changes


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
    """The night look (_materials.ensure_city_look): facade instances per style, snowy sidewalks,
    park and ground, wet asphalt."""
    look = m.ensure_city_look()
    materials = {
        "facades": look["facades"],
        "ground": m.ensure_prop_instance("Ground", *CLUTTER_MATERIALS["Ground"]),
        "asphalt": look["asphalt"],
        "grass": look["park"],
        "sidewalk": look["sidewalk"],
        "stars": look["stars"],
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

    pieces = building_pieces(district, materials["facades"]) + street_pieces(district, materials)
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
    changes += ensure_objective_beacons(district, existing)
    changes += ensure_safehouse(district, existing)
    changes += ensure_thugs(district, existing)
    changes += ensure_archers(district, existing)
    changes += ensure_clint(district, existing)
    changes += ensure_street_lamps(district, existing)
    changes += ensure_ambience(district, existing)
    changes += ensure_lamp_buzz(district, existing)
    changes += ensure_ledge_spawner(district, existing)
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
