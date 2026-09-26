"""Stage 2 pivot cleanup: rename the guard assets to thugs, retire the first-person ones.

Two passes, both idempotent, both run by create_all.py:

    run_renames()    first, before any script asks for the new names; leaves redirectors
        /Game/Blueprints/AI/BP_Guard               -> /Game/Blueprints/AI/BP_Thug
        /Game/Characters/Guard/M_GuardBody         -> /Game/Characters/Thug/M_ThugBody
        /Game/Characters/Guard/M_GuardVisor        -> /Game/Characters/Thug/M_ThugVisor

    run_deletions()  last, once the maps and Blueprints no longer reference them
        /Game/Blueprints/Weapons/DA_Weapon_Pistol  the hitscan pistol is not a player weapon
        /Game/Blueprints/Weapons/DA_Weapon_Rifle
        /Game/Blueprints/World/BP_Pickup_Pistol
        /Game/Materials/M_FrankArms                the first-person arms' fatigues
        /Game/Materials/M_FrankGloves
      then fixes up every redirector left under /Game.

The C++ side of the rename is covered by [CoreRedirects] in Config/DefaultEngine.ini; this is
the asset side. On a project that has already been cleaned every step logs "exists" and
nothing is saved.

Two things this has to work around, both learned the hard way in a headless run:
  * the asset registry does not refresh a package's dependencies when this process saves it,
    so every referencer query first rescans the candidate files from disk;
  * a rename only fixes its referencers in memory, so every package that referenced an old
    path is loaded and written out explicitly afterwards (maps through the level editor).
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

# (old path, new path). Renamed in one batch so references between them are fixed together.
RENAMES = [
    ("/Game/Characters/Guard/M_GuardBody", "/Game/Characters/Thug/M_ThugBody"),
    ("/Game/Characters/Guard/M_GuardVisor", "/Game/Characters/Thug/M_ThugVisor"),
    ("/Game/Blueprints/AI/BP_Guard", "/Game/Blueprints/AI/BP_Thug"),
]

# Folders the renames empty out.
EMPTIED_DIRECTORIES = ["/Game/Characters/Guard"]

# Deleted together, so a reference from one of these to another does not block the set.
RETIRED = [
    "/Game/Blueprints/World/BP_Pickup_Pistol",
    "/Game/Blueprints/Weapons/DA_Weapon_Pistol",
    "/Game/Blueprints/Weapons/DA_Weapon_Rifle",
    "/Game/Materials/M_FrankArms",
    "/Game/Materials/M_FrankGloves",
]


def _package(path):
    return path.split(".")[0]


def package_file(package_name):
    """Absolute .uasset/.umap path for a /Game package, or None when neither exists."""
    if not package_name.startswith("/Game/"):
        return None
    base = os.path.join(unreal.Paths.project_content_dir(), package_name[len("/Game/"):])
    for ext in (".umap", ".uasset"):
        full = os.path.abspath(base + ext)
        if os.path.isfile(full):
            return full
    return None


def registry():
    return unreal.AssetRegistryHelpers.get_asset_registry()


def rescan(package_names):
    """Re-read these packages from disk so the registry's dependency data is current."""
    files = [f for f in (package_file(p) for p in package_names) if f]
    if not files:
        return
    try:
        registry().scan_modified_asset_files(files)
    except Exception as exc:  # noqa: BLE001
        c.log_error("scan_modified_asset_files", exc)


def referencers(asset_path, ignore=()):
    """Packages on disk that reference asset_path, minus itself and anything in ignore."""
    def query():
        try:
            found = unreal.EditorAssetLibrary.find_package_referencers_for_asset(asset_path, False)
        except Exception as exc:  # noqa: BLE001
            c.log_error("find_package_referencers_for_asset " + asset_path, exc)
            return []
        return [str(p) for p in (found or [])]

    skip = set(_package(p) for p in ignore) | {_package(asset_path)}
    candidates = [p for p in query() if p not in skip]
    if not candidates:
        return []
    # The registry can be stale for anything this process has saved; ask again after a rescan.
    rescan(candidates)
    return [p for p in query() if p not in skip]


def is_map(package_name):
    return (package_file(package_name) or "").endswith(".umap")


def resave_package(package_name):
    """Load a package and save it, so its references are written against their current targets."""
    if is_map(package_name):
        subsystem = c.level_editor_subsystem()
        if subsystem is None or not subsystem.load_level(package_name):
            c.log("FAILED", package_name, "could not open the map to resave it")
            return False
        return bool(subsystem.save_current_level())

    asset = c.load_or_none(package_name)
    if asset is None:
        return False
    if isinstance(asset, unreal.Blueprint):
        # Recompile so the generated class is written against the redirected native parent.
        c.compile_blueprint(asset)
    return bool(unreal.EditorAssetLibrary.save_asset(package_name, only_if_is_dirty=False))


def run_renames():
    pending = []
    for old, new in RENAMES:
        old_exists, new_exists = c.exists(old), c.exists(new)
        if not old_exists:
            c.log("exists", new if new_exists else old,
                  "already renamed" if new_exists else "nothing to rename")
        elif new_exists:
            c.log("FAILED", old, "both {0} and {1} exist; merge by hand".format(old, new))
        else:
            pending.append((old, new))

    if pending:
        renamed = dict(pending)
        # Who points at the old paths, asked before the rename moves anything. The registry is
        # fresh from the startup scan at this point.
        former_referencers = set()
        for old, _new in pending:
            former_referencers |= set(referencers(old))

        batch = []
        for old, new in pending:
            asset = c.load_or_none(old)
            new_dir, new_name = new.rsplit("/", 1)
            c.ensure_directory(new_dir)
            if asset is not None:
                batch.append(unreal.AssetRenameData(asset, new_dir, new_name))
        try:
            ok = c.asset_tools().rename_assets(batch)
        except Exception as exc:  # noqa: BLE001
            c.log_error("rename_assets", exc)
            ok = False
        if not ok:
            c.log("FAILED", "pivot renames", "rename_assets returned false")
        else:
            for old, new in pending:
                c.log("updated", new, "renamed from " + old)

            # Write out the renamed assets and every package that pointed at an old path, at
            # its new path when it was itself renamed.
            to_save = set(new for _old, new in pending)
            to_save |= set(renamed.get(p, p) for p in former_referencers)
            for package in sorted(to_save, key=lambda p: (is_map(p), p)):
                if resave_package(package):
                    c.log("updated", package, "resaved against the renamed assets")

    # The old paths are redirectors now. They stay until run_deletions(): a package this process
    # has just loaded is still open, and Windows will not let the file go until it is released.
    remove_empty_directories()


def remove_empty_directories():
    for directory in EMPTIED_DIRECTORIES:
        try:
            if not unreal.EditorAssetLibrary.does_directory_exist(directory):
                continue
            if unreal.EditorAssetLibrary.list_assets(directory, True, False):
                continue
            unreal.EditorAssetLibrary.delete_directory(directory)
            c.log("deleted", directory, "empty after the rename")
        except Exception as exc:  # noqa: BLE001
            c.log_error("delete_directory " + directory, exc)


def redirectors_under(root="/Game"):
    """Package names of every ObjectRedirector under root."""
    filt = unreal.ARFilter(
        class_paths=[unreal.TopLevelAssetPath("/Script/CoreUObject", "ObjectRedirector")],
        package_paths=[root],
        recursive_paths=True,
    )
    return sorted(set(str(data.package_name) for data in (registry().get_assets(filt) or [])))


def delete_package(package_name, redirector=False):
    """Delete an asset package and make sure the file is really gone.

    A redirector is removed as a file only: delete_asset follows the redirect, reports success
    and leaves the redirector package on disk.
    """
    deleted = False
    if not redirector:
        try:
            deleted = bool(unreal.EditorAssetLibrary.delete_asset(package_name))
        except Exception as exc:  # noqa: BLE001
            c.log_error("delete_asset " + package_name, exc)

    leftover = package_file(package_name)
    if leftover:
        # Release any linker still holding the file open.
        try:
            unreal.SystemLibrary.collect_garbage()
        except Exception:  # noqa: BLE001
            pass
        try:
            os.remove(leftover)
            deleted = True
        except OSError as exc:
            c.log_error("remove " + leftover, exc)
            return False
        try:
            registry().scan_modified_asset_files([leftover])
        except Exception:  # noqa: BLE001 - the file is gone either way
            pass
    return deleted


def fix_up_redirectors():
    """The Fix Up Redirectors pass: resave every referencer, then delete the redirector."""
    found = redirectors_under("/Game")
    if not found:
        c.log("exists", "/Game redirectors", "none left")
        return 0

    fixed = 0
    for redirector in found:
        for package in referencers(redirector):
            if resave_package(package):
                c.log("updated", package, "resaved past redirector " + redirector)
        remaining = referencers(redirector)
        if remaining:
            c.log("FAILED", redirector, "still referenced by " + ", ".join(remaining))
            continue
        if delete_package(redirector, redirector=True):
            fixed += 1
            c.log("deleted", redirector, "redirector fixed up")
    return fixed


def delete_retired():
    deleted = 0
    for path in RETIRED:
        if not c.exists(path):
            c.log("exists", path, "already retired")
            continue
        blocking = referencers(path, ignore=RETIRED)
        if blocking:
            c.log("FAILED", path, "still referenced by " + ", ".join(blocking))
            continue
        if delete_package(path):
            deleted += 1
            c.log("deleted", path, "retired in the stage 2 pivot")
        else:
            c.log("FAILED", path, "could not be deleted")
    return deleted


def run_deletions():
    delete_retired()
    fix_up_redirectors()


def run():
    run_renames()
    run_deletions()


if __name__ == "__main__":
    run()
    c.print_summary("pivot cleanup")
