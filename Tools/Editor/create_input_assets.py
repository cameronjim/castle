"""Create the Enhanced Input starter assets in /Game/Input.

    IA_Move, IA_Look                (Axis2D)
    IA_LookStick                    (Axis2D, gamepad right stick; kept apart from IA_Look so
                                     ACastleCharacter's handler can scale it by delta time)
    IA_Jump  IA_Sprint  IA_Crouch  IA_Fire  IA_Aim  IA_Reload
    IA_Takedown  IA_Interact  IA_Pause  IA_Skip   (Digital / bool)
    IA_Slot1 .. IA_Slot6  IA_Inventory             (Digital / bool, keys 1..6: quiver slots)
    IA_Grapple                      (Digital / bool, Q: the grapple arrow)
    IA_Melee                        (Digital / bool, V: bow strike, tap light / hold heavy)
    IA_SlotScroll                   (Axis1D, the mouse wheel and the D-pad left/right)
    IMC_Default                     with the UE first-person template's WASD + mouse setup, plus
                                     a full Xbox-layout gamepad mapping (PlayStation pads read the
                                     same physical buttons through Unreal's Gamepad_* keys)

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
AXIS1D = "AXIS1D"
AXIS2D = "AXIS2D"

# name -> EInputActionValueType member name
ACTIONS = [
    ("IA_Move", AXIS2D),
    ("IA_Look", AXIS2D),
    ("IA_LookStick", AXIS2D),
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
    ("IA_Slot1", BOOL),
    ("IA_Slot2", BOOL),
    ("IA_Slot3", BOOL),
    ("IA_Slot4", BOOL),
    ("IA_Slot5", BOOL),
    ("IA_Slot6", BOOL),
    ("IA_SlotScroll", AXIS1D),
    ("IA_Inventory", BOOL),
    ("IA_Grapple", BOOL),
    ("IA_Melee", BOOL),
]

# (action name, FKey name, [modifier specs])
# A modifier spec is (unreal class name, {property: value}).
#
# WASD matches the UE5 first-person template: the Axis2D action carries X = strafe,
# Y = forward, so W/S swizzle the key's 1.0 onto Y and S/A negate it.
SWIZZLE = ("InputModifierSwizzleAxis", {})           # default order is YXZ
NEGATE = ("InputModifierNegate", {})                 # negates X, Y and Z
NEGATE_Y = ("InputModifierNegate", {"x": False, "y": True, "z": False})
NEGATE_X = ("InputModifierNegate", {"x": True, "y": False, "z": False})

# Gamepad_Left2D: X = strafe, Y = forward, same axis order WASD swizzles onto, so the stick needs
# no swizzle - only a deadzone so a thumb resting on the stick doesn't creep the character.
_RADIAL = ("ENUM", "DeadZoneType", "RADIAL")
STICK_MOVE_DEADZONE = ("InputModifierDeadZone", {"lower_threshold": 0.2, "upper_threshold": 1.0, "type": _RADIAL})

# The look stick: a slightly bigger deadzone than movement (a stick that isn't perfectly centred
# must never cause a slow camera drift) plus a Scalar so 1.0 stick deflection is easy to tune
# later without touching StickYawDegreesPerSecond/StickPitchDegreesPerSecond in C++.
STICK_LOOK_DEADZONE = ("InputModifierDeadZone", {"lower_threshold": 0.25, "upper_threshold": 1.0, "type": _RADIAL})
STICK_LOOK_SCALAR = ("InputModifierScalar", {"scalar": unreal.Vector(1.0, 1.0, 1.0)})

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
    ("IA_Slot1", "One", []),
    ("IA_Slot2", "Two", []),
    ("IA_Slot3", "Three", []),
    ("IA_Slot4", "Four", []),
    ("IA_Slot5", "Five", []),
    ("IA_Slot6", "Six", []),
    # The wheel is a single axis: up is +1 (next slot), down is -1 (previous).
    ("IA_SlotScroll", "MouseWheelAxis", []),
    ("IA_Inventory", "Tab", []),
    ("IA_Grapple", "Q", []),
    # V: a bow strike. Tap for the light, hold 0.4 s for the heavy (ACastleCharacter decides).
    ("IA_Melee", "V", []),

    # --- Gamepad (Xbox layout; a PlayStation pad reports the same Gamepad_* keys) --------------
    ("IA_Move", "Gamepad_Left2D", [STICK_MOVE_DEADZONE]),
    # IA_Look stays mouse-only; the stick drives IA_LookStick instead so ACastleCharacter's
    # handler (which must scale by delta time and StickSensitivity) has an unambiguous source.
    ("IA_LookStick", "Gamepad_Right2D", [STICK_LOOK_DEADZONE, STICK_LOOK_SCALAR, NEGATE_Y]),
    ("IA_Jump", "Gamepad_FaceButton_Bottom", []),               # A
    ("IA_Sprint", "Gamepad_LeftThumbstick", []),                # L3, held
    ("IA_Crouch", "Gamepad_FaceButton_Right", []),              # B: also drives dodge tap / slide
    # Bool actions read an analog trigger as pressed past the default 0.5 threshold, so no
    # modifier is needed to make the pull digital here.
    ("IA_Fire", "Gamepad_RightTrigger", []),                    # RT: draw the bow
    ("IA_Aim", "Gamepad_LeftTrigger", []),                       # LT
    ("IA_Grapple", "Gamepad_RightShoulder", []),                # RB
    ("IA_Melee", "Gamepad_FaceButton_Left", []),                # X: strike
    # Takedown and Interact share Y: ACastleCharacter binds Takedown first and has Interact back
    # off for that press when a takedown just landed (see Input_Takedown/Input_Interact).
    ("IA_Takedown", "Gamepad_FaceButton_Top", []),              # Y
    ("IA_Interact", "Gamepad_FaceButton_Top", []),              # Y
    ("IA_Inventory", "Gamepad_Special_Left", []),               # View
    ("IA_Pause", "Gamepad_Special_Right", []),                  # Menu
    # D-pad left/right cycles arrow slots; up/down jump straight to slot 1 (standard) and 2
    # (grapple), the two slots CH01 always grants.
    ("IA_SlotScroll", "Gamepad_DPad_Left", [NEGATE_X]),
    ("IA_SlotScroll", "Gamepad_DPad_Right", []),
    ("IA_Slot1", "Gamepad_DPad_Up", []),
    ("IA_Slot2", "Gamepad_DPad_Down", []),
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


def resolve_enum_value(enum_class_name, member_name):
    """unreal.<EnumClassName>.<MEMBER>, or None if the enum isn't exposed to Python."""
    enum = getattr(unreal, enum_class_name, None)
    if enum is None:
        return None
    return getattr(enum, member_name, None)


def _resolve_prop_value(value):
    """('ENUM', ClassName, MEMBER) becomes the real enum value; anything else passes through."""
    if isinstance(value, tuple) and len(value) == 3 and value[0] == "ENUM":
        resolved = resolve_enum_value(value[1], value[2])
        if resolved is None:
            unreal.log_warning(
                "[Castle] enum {0}.{1} not exposed to Python".format(value[1], value[2])
            )
        return resolved
    return value


def build_modifier(imc, spec):
    cls_name, props = spec
    cls = getattr(unreal, cls_name, None)
    if cls is None:
        unreal.log_warning("[Castle] modifier class {0} not exposed to Python".format(cls_name))
        return None
    modifier = unreal.new_object(cls, outer=imc)
    if props:
        resolved = [(name, _resolve_prop_value(value)) for name, value in props.items()]
        c.set_props(modifier, sorted(resolved), cls_name)
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
