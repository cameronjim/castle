"""Fix Up Redirectors for /Game, headless: resave every referencer, then delete the redirector.

The Game Animation Sample ships a few redirectors, and import_gasp.py copies them alongside any
package that may still point through them. This pass resaves those referencers and deletes the
redirectors, so a later run finds none and saves nothing. Run by create_all.py after every step
that may copy or rename assets.

Two things this has to work around, both learned the hard way in a headless run:
  * the asset registry does not refresh a package's dependencies when this process saves it,
    so every referencer query first rescans the candidate files from disk;
  * a resave only fixes references in memory until the package is written, so every package
    that referenced a redirector is loaded and written out explicitly (maps through the level
    editor).
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402


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


def run():
    return fix_up_redirectors()


if __name__ == "__main__":
    run()
    c.print_summary("fix up redirectors")
