"""Create the three weapon definition data assets and wire them into the content.

    /Game/Blueprints/Weapons/DA_Weapon_Hands    melee, 15 damage, the always-present slot 0
    /Game/Blueprints/Weapons/DA_Weapon_Pistol   34 / 12 / 24, x3 headshot, SM_Pistol
    /Game/Blueprints/Weapons/DA_Weapon_Rifle    placeholder: 24 / 30 / 90, no mesh yet

Then:

    BP_CastleCharacter.InventoryComponent.HandsDefinition = DA_Weapon_Hands
    BP_Pickup_Pistol.Weapon                               = DA_Weapon_Pistol

Property names come from Source/Castle/Combat/WeaponDefinition.h. Idempotent: an existing
asset keeps its values and is only re-saved when a field is actually different.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

WEAPON_PATH = "/Game/Blueprints/Weapons"
PLAYER_PATH = "/Game/Blueprints/Player"
WORLD_PATH = "/Game/Blueprints/World"

PISTOL_MESH = "/Game/Weapons/Pistol/Meshes/SM_Pistol"


def slot(name):
    """unreal.HotbarSlot member, or None when the enum is not exposed."""
    enum = getattr(unreal, "HotbarSlot", None)
    return getattr(enum, name, None) if enum is not None else None


def weapon_values(name):
    """(property, value) pairs for one weapon, as a plain table so the stats are readable."""
    if name == "DA_Weapon_Hands":
        return [
            ("display_name", "Fists"),
            ("short_name", "Fists"),
            ("slot", slot("HANDS")),
            ("damage", 15.0),
            ("magazine_size", 0),
            ("default_reserve", 0),
            ("is_melee", True),
            ("melee_range", 120.0),
            ("melee_cooldown", 0.6),
            ("stagger_on_hit", True),
            ("arms_pose_name", "Fists"),
        ]

    if name == "DA_Weapon_Pistol":
        return [
            ("display_name", "Pistol"),
            ("short_name", "Pistol"),
            ("slot", slot("PISTOL")),
            ("damage", 34.0),
            ("magazine_size", 12),
            ("default_reserve", 24),
            ("fire_rate", 600.0),
            ("reload_seconds", 2.0),
            ("hip_spread_degrees", 2.5),
            ("aim_spread_degrees", 0.5),
            ("headshot_multiplier", 3.0),
            ("is_melee", False),
            ("view_model_mesh", c.load_or_none(PISTOL_MESH)),
            ("arms_pose_name", "Pistol"),
            ("hand_offset", unreal.Vector(4.0, 0.0, 0.0)),
            # unreal.Rotator is (roll, pitch, yaw), not the (pitch, yaw, roll) an FRotator
            # literal in C++ takes. Writing the C++ order here gave the pistol a -90 degree
            # PITCH: in game the slide pointed straight down through the palm and the gun
            # vanished out of frame, while the editor screenshots - which arm Frank without a
            # definition and so fall back to the C++ default - looked perfect.
            ("hand_rotation", unreal.Rotator(0.0, 0.0, -90.0)),
        ]

    # Rifle: placeholder stats so the third slot is real long before the mission that uses it.
    return [
        ("display_name", "Rifle"),
        ("short_name", "Rifle"),
        ("slot", slot("RIFLE")),
        ("damage", 24.0),
        ("magazine_size", 30),
        ("default_reserve", 90),
        ("fire_rate", 700.0),
        ("reload_seconds", 2.4),
        ("hip_spread_degrees", 3.0),
        ("aim_spread_degrees", 0.4),
        ("headshot_multiplier", 3.0),
        ("is_melee", False),
        ("arms_pose_name", "Rifle"),
    ]


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


def create_weapon(name):
    full = c.asset_path(WEAPON_PATH, name)
    cls = c.find_class("WeaponDefinition", "/Script/Castle.WeaponDefinition")
    if cls is None:
        c.log("FAILED", full, "UWeaponDefinition not exposed to Python")
        return None

    asset, created = c.create_asset(name, WEAPON_PATH, cls, data_asset_factory(cls), quiet=True)
    if asset is None:
        return None

    wanted = [(prop, value) for prop, value in weapon_values(name) if value is not None]
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


def wire_pickup(pistol):
    """BP_Pickup_Pistol hands the player DA_Weapon_Pistol rather than a bare magazine count."""
    if pistol is None:
        return False

    full = c.asset_path(WORLD_PATH, "BP_Pickup_Pistol")
    bp = c.load_or_none(full)
    if bp is None:
        c.log("skipped", full + ".weapon", "run create_world_blueprints.py first")
        return False

    cdo = c.blueprint_cdo(bp)
    if cdo is None:
        c.log("skipped", full + ".weapon", "no class default object")
        return False

    try:
        if same_value(cdo.get_editor_property("weapon"), pistol):
            c.log("exists", full + ".weapon")
            return False
    except Exception:  # noqa: BLE001
        pass

    if not c.set_props(cdo, [("weapon", pistol)], "BP_Pickup_Pistol"):
        return False

    c.compile_blueprint(bp)
    c.save(bp)
    c.log("updated", full + ".weapon", "DA_Weapon_Pistol")
    return True


def run():
    c.ensure_directory(WEAPON_PATH)

    hands = create_weapon("DA_Weapon_Hands")
    pistol = create_weapon("DA_Weapon_Pistol")
    create_weapon("DA_Weapon_Rifle")

    # Without this the inventory falls back to a transient stand-in for Frank's fists, which
    # works but is not the asset a designer can tune.
    set_component_property(
        PLAYER_PATH, "BP_CastleCharacter", "inventory_component", "hands_definition", hands,
        "BP_CastleCharacter.InventoryComponent.hands_definition")

    wire_pickup(pistol)

    return {"hands": hands, "pistol": pistol}


if __name__ == "__main__":
    run()
    c.print_summary("weapon data")
