"""Create the weapon, bow and arrow data and wire it into the player.

    /Game/Blueprints/Weapons/DA_Weapon_Hands      melee, 15 damage: left click while no bow is owned
    /Game/Blueprints/Weapons/M_Bow                dark purple, the placeholder bow's material
    /Game/Blueprints/Weapons/SM_Bow_Placeholder   a 120 cm bow from three cylinders (riser and two
                                                  limbs raked back to tips at x -12, z +-60); +X is
                                                  where the arrow goes, grip at the origin. The
                                                  string is drawn at runtime by UBowComponent.
    /Game/Blueprints/Weapons/BP_Arrow_Standard    parent AArrowProjectile
    /Game/Blueprints/Weapons/BP_Arrow_Grapple     parent AGrappleArrowProjectile
    /Game/Blueprints/Weapons/DA_Bow_Kate          UBowDefinition, 0.8 s draw
    /Game/Blueprints/Weapons/DA_Bow_Clint         UBowDefinition, 1.0 s draw (unused until Clint)
    /Game/Blueprints/Weapons/DA_Arrow_Standard    UArrowDefinition, slot 1, 40 damage, cap 30
    /Game/Blueprints/Weapons/DA_Arrow_Grapple     UArrowDefinition, slot 2, cap 6, OnHitEffect Grapple

Then:

    BP_CastleCharacter.InventoryComponent.HandsDefinition         = DA_Weapon_Hands
    BP_CastleCharacter.InventoryComponent.StandardArrowDefinition = DA_Arrow_Standard

Property names come from Source/Castle/Combat/{Weapon,Bow,Arrow}Definition.h. Idempotent: an
existing asset keeps its values and is only re-saved when a field is actually different; the
mesh is built once and only its material is checked afterwards.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

WEAPON_PATH = "/Game/Blueprints/Weapons"
PLAYER_PATH = "/Game/Blueprints/Player"

BOW_MATERIAL = "M_Bow"
BOW_MATERIAL_RGB = (0.10, 0.03, 0.16)
BOW_MESH = "SM_Bow_Placeholder"

# Palm of the left hand on the UEFN mannequin (a socket on hand_l, found by introspecting
# SKM_UEFN_Mannequin): the grip sits in it while drawing.
KATE_HAND_SOCKET = "palm_l_Socket"


def enum_value(enum_name, member):
    enum = getattr(unreal, enum_name, None)
    return getattr(enum, member, None) if enum is not None else None


def bow_values(name, mesh):
    """(property, value) pairs for one bow, as a plain table so the numbers are readable."""
    common = [
        ("min_draw_fraction", 0.25),
        ("max_speed", 6000.0),
        ("min_speed_fraction", 0.4),
        ("min_damage_fraction", 0.4),
        ("min_spread", 0.5),
        ("max_spread", 4.0),
        ("perfect_window_seconds", 0.1),
        ("perfect_bonus", 0.25),
        ("headshot_multiplier", 3.0),
        ("bow_mesh", mesh),
        ("hand_socket", KATE_HAND_SOCKET),
    ]
    if name == "DA_Bow_Kate":
        return [("display_name", "Kate's recurve"), ("full_draw_seconds", 0.8)] + common
    if name == "DA_Bow_Clint":
        return [("display_name", "Clint's recurve"), ("full_draw_seconds", 1.0)] + common
    return []


def arrow_values(name, projectile_classes):
    if name == "DA_Arrow_Standard":
        return [
            ("display_name", "Standard arrow"),
            ("short_name", "Arrow"),
            ("slot", 1),
            ("projectile_class", projectile_classes.get("BP_Arrow_Standard")),
            ("damage", 40.0),
            ("cap", 30),
            ("recoverable", True),
            ("on_hit_effect", enum_value("ArrowHitEffect", "NONE")),
        ]
    if name == "DA_Arrow_Grapple":
        return [
            ("display_name", "Grapple arrow"),
            ("short_name", "Grapple"),
            ("slot", 2),
            ("projectile_class", projectile_classes.get("BP_Arrow_Grapple")),
            ("damage", 0.0),
            ("cap", 6),
            ("recoverable", True),
            ("on_hit_effect", enum_value("ArrowHitEffect", "GRAPPLE")),
        ]
    return []


def weapon_values(name):
    if name == "DA_Weapon_Hands":
        return [
            ("display_name", "Fists"),
            ("short_name", "Fists"),
            ("damage", 15.0),
            ("magazine_size", 0),
            ("default_reserve", 0),
            ("is_melee", True),
            ("melee_range", 120.0),
            ("melee_cooldown", 0.6),
            ("stagger_on_hit", True),
        ]
    return []


def data_asset_factory(data_asset_class):
    factory = c.new_factory("DataAssetFactory")
    if factory is None:
        return None
    c.set_props(factory, [("data_asset_class", data_asset_class)], "DataAssetFactory")
    return factory


def same_value(current, wanted):
    """Property comparison that copes with FText, soft pointers and enums reading back oddly."""
    if current is None:
        return wanted is None
    try:
        if current == wanted:
            return True
    except Exception:  # noqa: BLE001 - some struct types refuse ==
        pass

    # A float property round-trips through single precision, so 0.6 comes back as
    # 0.6000000238 and a string comparison would rewrite the asset on every run.
    if isinstance(wanted, float) and isinstance(current, (int, float)):
        return abs(float(current) - wanted) < 1e-4

    return str(current) == str(wanted)


def create_data_asset(name, class_name, values):
    """Create or correct one data asset from (property, value) pairs. None values are skipped."""
    full = c.asset_path(WEAPON_PATH, name)
    cls = c.find_class(class_name, "/Script/Castle." + class_name)
    if cls is None:
        c.log("FAILED", full, "U{0} not exposed to Python".format(class_name))
        return None

    asset, created = c.create_asset(name, WEAPON_PATH, cls, data_asset_factory(cls), quiet=True)
    if asset is None:
        return None

    wanted = [(prop, value) for prop, value in values if value is not None]
    changed = []
    for prop, value in wanted:
        try:
            if same_value(asset.get_editor_property(prop), value):
                continue
        except Exception:  # noqa: BLE001 - set_props reports a missing property
            pass
        if c.set_props(asset, [(prop, value)], name):
            changed.append(prop)

    if created:
        c.save(asset)
        c.log("created", full, "{0} fields".format(len(wanted)))
    elif changed:
        c.save(asset)
        c.log("updated", full, ", ".join(changed))
    else:
        c.log("exists", full)
    return asset


# --- the placeholder bow mesh ------------------------------------------------------------------


def first(result):
    return result[0] if isinstance(result, tuple) else result


def build_bow_mesh():
    """Riser, two limbs raked back to the string tips. cm, +X forward, +Z up the bow."""
    prim = unreal.GeometryScript_Primitives
    options = unreal.GeometryScriptPrimitiveOptions()
    base = unreal.GeometryScriptPrimitiveOriginMode.BASE
    mesh = unreal.DynamicMesh()

    def cylinder(location, pitch, radius, height):
        xf = unreal.Transform(location=unreal.Vector(*location),
                              rotation=unreal.Rotator(roll=0.0, pitch=pitch, yaw=0.0))
        prim.append_cylinder(mesh, options, xf, radius, height, 8, 0, True, base)

    # Riser from z -18 to 18, the grip in the middle.
    cylinder((0.0, 0.0, -18.0), 0.0, 1.8, 36.0)
    # Limbs from the riser ends (z +-16) to the tips (x -12, z +-60): 45.6 cm, raked 15.3 degrees
    # back towards the archer. Pitch turns the cylinder's +Z towards -X.
    cylinder((0.0, 0.0, 16.0), 15.26, 1.1, 45.6)
    cylinder((0.0, 0.0, -16.0), 164.74, 1.1, 45.6)
    return mesh


def ensure_bow_mesh(material):
    full = c.asset_path(WEAPON_PATH, BOW_MESH)
    static_mesh = c.load_or_none(full)
    action = "exists"
    if static_mesh is None:
        options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.set_editor_property("enable_recompute_normals", False)
        options.set_editor_property("enable_recompute_tangents", True)
        options.set_editor_property("enable_nanite", False)
        options.set_editor_property("enable_collision", False)
        static_mesh = first(unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
            build_bow_mesh(), full, options))
        if static_mesh is None:
            c.log("FAILED", full, "create_new_static_mesh_asset_from_mesh returned None")
            return None
        action = "created"

    if material is not None:
        try:
            if static_mesh.get_material(0) != material:
                static_mesh.set_material(0, material)
                if action == "exists":
                    action = "updated"
        except Exception as exc:  # noqa: BLE001
            c.log_error("material on " + full, exc)

    if action != "exists":
        c.save(static_mesh)
    c.log(action, full)
    return static_mesh


# --- arrow projectile Blueprints ---------------------------------------------------------------


def ensure_projectile_blueprints():
    """BP_Arrow_Standard and BP_Arrow_Grapple. Returns {name: generated class}."""
    import create_blueprints as bps  # noqa: WPS433 - same folder; make_blueprint is shared

    parents = {
        "BP_Arrow_Standard": c.find_class("ArrowProjectile", "/Script/Castle.ArrowProjectile"),
        "BP_Arrow_Grapple": c.find_class("GrappleArrowProjectile", "/Script/Castle.GrappleArrowProjectile"),
    }
    classes = {}
    for name, parent in parents.items():
        bp, created = bps.make_blueprint(name, WEAPON_PATH, parent, ("BlueprintFactory",))
        if bp is not None and created:
            c.compile_blueprint(bp)
            c.save(bp)
        if bp is not None:
            bps.ensure_parent(bp, c.asset_path(WEAPON_PATH, name), parent)
        classes[name] = c.load_generated_class(WEAPON_PATH, name)
    return classes


def set_component_property(bp_path, bp_name, component_name, prop, value, context):
    """Write one property on an inherited component template through the Blueprint CDO."""
    if value is None:
        c.log("skipped", context, "asset not found")
        return False

    bp = c.load_or_none(c.asset_path(bp_path, bp_name))
    if bp is None:
        c.log("skipped", context, "Blueprint not found")
        return False

    cdo = c.blueprint_cdo(bp)
    component = None
    if cdo is not None:
        try:
            component = cdo.get_editor_property(component_name)
        except Exception:  # noqa: BLE001
            component = None
    if component is None:
        c.log("skipped", context, "no component called " + component_name)
        return False

    try:
        if same_value(component.get_editor_property(prop), value):
            c.log("exists", context)
            return False
    except Exception:  # noqa: BLE001 - set_props reports a missing property
        pass

    if not c.set_props(component, [(prop, value)], context):
        return False

    c.compile_blueprint(bp)
    c.save(bp)
    c.log("updated", context, c.safe_name(value))
    return True


def run():
    c.ensure_directory(WEAPON_PATH)

    hands = create_data_asset("DA_Weapon_Hands", "WeaponDefinition", weapon_values("DA_Weapon_Hands"))

    material = c.ensure_constant_color_material(BOW_MATERIAL, WEAPON_PATH, BOW_MATERIAL_RGB, 0.45)
    mesh = ensure_bow_mesh(material)
    projectiles = ensure_projectile_blueprints()

    bows = {name: create_data_asset(name, "BowDefinition", bow_values(name, mesh))
            for name in ("DA_Bow_Kate", "DA_Bow_Clint")}
    arrows = {name: create_data_asset(name, "ArrowDefinition", arrow_values(name, projectiles))
              for name in ("DA_Arrow_Standard", "DA_Arrow_Grapple")}

    # Without these the inventory falls back to transient stand-ins, which work but are not the
    # assets a designer can tune.
    set_component_property(
        PLAYER_PATH, "BP_CastleCharacter", "inventory_component", "hands_definition", hands,
        "BP_CastleCharacter.InventoryComponent.hands_definition")
    set_component_property(
        PLAYER_PATH, "BP_CastleCharacter", "inventory_component", "standard_arrow_definition",
        arrows.get("DA_Arrow_Standard"), "BP_CastleCharacter.InventoryComponent.standard_arrow_definition")

    return {"hands": hands, "bows": bows, "arrows": arrows, "mesh": mesh}


if __name__ == "__main__":
    run()
    c.print_summary("weapon data")
