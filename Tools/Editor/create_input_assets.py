"""Create the Enhanced Input starter assets in /Game/Input.

    IA_Move, IA_Look                (Axis2D)
    IA_Jump  IA_Sprint  IA_Crouch  IA_Fire  IA_Aim  IA_Reload
    IA_Takedown  IA_Interact  IA_Pause  IA_Skip   (Digital / bool)
    IMC_Default                     with the UE first-person template's WASD + mouse setup

Idempotent: existing assets are left alone (the IMC's key mappings are only rebuilt when
its mapping count doesn't match the table below).
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

INPUT_PATH = "/Game/Input"

BOOL = "BOOLEAN"
AXIS2D = "AXIS2D"

# name -> EInputActionValueType member name
ACTIONS = [
    ("IA_Move", AXIS2D),
    ("IA_Look", AXIS2D),
    ("IA_Jump", BOOL),
    ("IA_Sprint", BOOL),
    ("IA_Crouch", BOOL),
    ("IA_Fire", BOOL),
    ("IA_Aim", BOOL),
    ("IA_Reload", BOOL),
    ("IA_Takedown", BOOL),
    ("IA_Interact", BOOL),
    ("IA_Pause", BOOL),
    ("IA_Skip", BOOL),
]

# (action name, FKey name, [modifier specs])
# A modifier spec is (unreal class name, {property: value}).
#
# WASD matches the UE5 first-person template: the Axis2D action carries X = strafe,
# Y = forward, so W/S swizzle the key's 1.0 onto Y and S/A negate it.
SWIZZLE = ("InputModifierSwizzleAxis", {})           # default order is YXZ
NEGATE = ("InputModifierNegate", {})                 # negates X, Y and Z
NEGATE_Y = ("InputModifierNegate", {"x": False, "y": True, "z": False})

MAPPINGS = [
    ("IA_Move", "W", [SWIZZLE]),
    ("IA_Move", "S", [SWIZZLE, NEGATE]),
    ("IA_Move", "A", [NEGATE]),
    ("IA_Move", "D", []),
    ("IA_Look", "Mouse2D", [NEGATE_Y]),
    ("IA_Jump", "SpaceBar", []),
    ("IA_Sprint", "LeftShift", []),
    ("IA_Crouch", "LeftControl", []),
    ("IA_Crouch", "C", []),
    ("IA_Fire", "LeftMouseButton", []),
    ("IA_Aim", "RightMouseButton", []),
    ("IA_Reload", "R", []),
    ("IA_Takedown", "F", []),
    ("IA_Interact", "E", []),
    ("IA_Pause", "Escape", []),
    ("IA_Skip", "AnyKey", []),
]


def make_key(key_name):
    """FKey from its EKeys name ("W", "Mouse2D", "LeftMouseButton", ...)."""
    try:
        return unreal.Key(key_name=key_name)
    except Exception:  # noqa: BLE001 - older/newer bindings may not take kwargs
        key = unreal.Key()
        key.set_editor_property("key_name", key_name)
        return key


def value_type(member_name):
    enum = getattr(unreal, "InputActionValueType", None)
    if enum is None:
        return None
    return getattr(enum, member_name, None)


def create_actions():
    """Returns {name: UInputAction}."""
    factory = c.new_factory("InputAction_Factory", "InputActionFactory")
    if factory is None:
        unreal.log_warning(
            "[Castle] no UInputAction factory class exposed; falling back to factory=None"
        )

    created = {}
    for name, vt_name in ACTIONS:
        full = c.asset_path(INPUT_PATH, name)
        try:
            asset, was_created = c.create_asset(
                name, INPUT_PATH, unreal.InputAction, factory, quiet=True
            )
            if asset is None:
                continue
            created[name] = asset

            wanted = value_type(vt_name)
            if wanted is None:
                c.log("created" if was_created else "exists", full, "value_type enum missing")
                continue

            current = None
            try:
                current = asset.get_editor_property("value_type")
            except Exception:  # noqa: BLE001
                pass

            if current == wanted and not was_created:
                c.log("exists", full)
                continue

            applied = c.set_props(asset, [("value_type", wanted)], name)
            if was_created:
                c.log("created", full, vt_name)
                c.save(asset)
            elif applied:
                c.log("updated", full, vt_name)
                c.save(asset)
            else:
                c.log("exists", full, "value_type not settable")
        except Exception as exc:  # noqa: BLE001
            c.log_error(full, exc)
    return created


def build_modifier(imc, spec):
    cls_name, props = spec
    cls = getattr(unreal, cls_name, None)
    if cls is None:
        unreal.log_warning("[Castle] modifier class {0} not exposed to Python".format(cls_name))
        return None
    modifier = unreal.new_object(cls, outer=imc)
    if props:
        c.set_props(modifier, sorted(props.items()), cls_name)
    return modifier


def read_mappings(imc):
    """(container_struct_or_None, list_of_mappings). Handles 5.7+ DefaultKeyMappings."""
    try:
        container = imc.get_editor_property("default_key_mappings")
        return container, list(container.get_editor_property("mappings"))
    except Exception:  # noqa: BLE001 - pre-5.7 layout
        pass
    try:
        return None, list(imc.get_editor_property("mappings"))
    except Exception as exc:  # noqa: BLE001
        c.log_error("read IMC mappings", exc)
        return None, []


def write_mappings(imc, container, mappings):
    if container is not None:
        container.set_editor_property("mappings", mappings)
        imc.set_editor_property("default_key_mappings", container)
    else:
        imc.set_editor_property("mappings", mappings)


def create_mapping_context(actions):
    name = "IMC_Default"
    full = c.asset_path(INPUT_PATH, name)
    factory = c.new_factory("InputMappingContext_Factory", "InputMappingContextFactory")

    imc, was_created = c.create_asset(
        name, INPUT_PATH, unreal.InputMappingContext, factory, quiet=True
    )
    if imc is None:
        return None

    try:
        _container, existing = read_mappings(imc)
        if not was_created and len(existing) == len(MAPPINGS):
            c.log("exists", full, "{0} mappings".format(len(existing)))
            return imc

        # Rebuild from scratch so a re-run converges instead of duplicating.
        try:
            imc.unmap_all()
        except Exception as exc:  # noqa: BLE001
            c.log_error("IMC_Default.unmap_all", exc)

        ordered_specs = []
        for action_name, key_name, mod_specs in MAPPINGS:
            action = actions.get(action_name)
            if action is None:
                unreal.log_warning(
                    "[Castle] skipped mapping {0} -> {1}: action asset missing".format(
                        key_name, action_name
                    )
                )
                continue
            try:
                imc.map_key(action, make_key(key_name))
                ordered_specs.append((action_name, key_name, mod_specs))
            except Exception as exc:  # noqa: BLE001
                c.log_error("map_key {0} -> {1}".format(key_name, action_name), exc)

        # map_key returns a copy in Python, so modifiers are applied by rewriting the array.
        container, mappings = read_mappings(imc)
        if len(mappings) != len(ordered_specs):
            unreal.log_warning(
                "[Castle] IMC mapping count {0} != expected {1}; modifiers not applied".format(
                    len(mappings), len(ordered_specs)
                )
            )
        else:
            touched = False
            for index, (_action_name, _key_name, mod_specs) in enumerate(ordered_specs):
                if not mod_specs:
                    continue
                built = [build_modifier(imc, spec) for spec in mod_specs]
                built = [m for m in built if m is not None]
                if not built:
                    continue
                try:
                    mappings[index].set_editor_property("modifiers", built)
                    touched = True
                except Exception as exc:  # noqa: BLE001
                    c.log_error("set modifiers on mapping {0}".format(index), exc)
            if touched:
                try:
                    write_mappings(imc, container, mappings)
                except Exception as exc:  # noqa: BLE001
                    c.log_error("write IMC mappings", exc)

        c.log(
            "created" if was_created else "updated",
            full,
            "{0} mappings".format(len(ordered_specs)),
        )
        c.save(imc)
        return imc
    except Exception as exc:  # noqa: BLE001
        c.log_error(full, exc)
        return imc


def run():
    c.ensure_directory(INPUT_PATH)
    actions = create_actions()
    create_mapping_context(actions)
    return actions


if __name__ == "__main__":
    run()
    c.print_summary("input assets")
