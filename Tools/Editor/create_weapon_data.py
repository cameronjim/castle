"""Create the Hands weapon definition and wire it into the player.

    /Game/Blueprints/Weapons/DA_Weapon_Hands    melee, 15 damage, the always-present slot 0

Then:

    BP_CastleCharacter.InventoryComponent.HandsDefinition = DA_Weapon_Hands

The pistol and rifle definitions went with the first-person build (pivot_cleanup.py deletes
them). TODO(stage2): DA_Bow_Kate and the DA_Arrow_* definitions arrive here with the bow.

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


def run():
    c.ensure_directory(WEAPON_PATH)

    hands = create_weapon("DA_Weapon_Hands")

    # Without this the inventory falls back to a transient stand-in for bare hands, which
    # works but is not the asset a designer can tune.
    set_component_property(
        PLAYER_PATH, "BP_CastleCharacter", "inventory_component", "hands_definition", hands,
        "BP_CastleCharacter.InventoryComponent.hands_definition")

    return {"hands": hands}


if __name__ == "__main__":
    run()
    c.print_summary("weapon data")
