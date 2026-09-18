"""Generate and import the six placeholder flashback stills.

Six 1280x720 solid-colour PNGs in warm tones are written to Saved/Placeholders/ with pure
Python (zlib + struct, no PIL) and imported as /Game/Flashbacks/Images/T_FB01_01..06.

Both the generation and the import are skipped when the asset already exists. The PNG on
disk is named exactly like the target asset so Interchange names the asset correctly even
though it ignores AssetImportTask.destination_name.
"""

import os
import struct
import sys
import zlib

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

IMAGE_PATH = "/Game/Flashbacks/Images"
WIDTH = 1280
HEIGHT = 720

# Warm tones, bright Sunday morning down to a black final slide.
COLOURS = [
    ("T_FB01_01", (0xE8, 0xC3, 0x9E)),
    ("T_FB01_02", (0xD9, 0xA0, 0x66)),
    ("T_FB01_03", (0xC2, 0x7C, 0x4E)),
    ("T_FB01_04", (0x8C, 0x4A, 0x2F)),
    ("T_FB01_05", (0x4A, 0x2A, 0x1E)),
    ("T_FB01_06", (0x1A, 0x1A, 0x1A)),
]


def _png_chunk(tag, data):
    out = struct.pack(">I", len(data))
    payload = tag + data
    out += payload
    out += struct.pack(">I", zlib.crc32(payload) & 0xFFFFFFFF)
    return out


def write_solid_png(file_path, width, height, rgb):
    """Minimal 8-bit RGB PNG writer."""
    row = b"\x00" + (bytes(bytearray(rgb)) * width)
    raw = row * height

    data = b"\x89PNG\r\n\x1a\n"
    data += _png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    data += _png_chunk(b"IDAT", zlib.compress(raw, 9))
    data += _png_chunk(b"IEND", b"")

    with open(file_path, "wb") as handle:
        handle.write(data)
    return file_path


def run():
    c.ensure_directory(IMAGE_PATH)
    scratch = c.ensure_disk_directory(os.path.join(c.project_dir(), "Saved", "Placeholders"))

    tasks = []
    pending = []
    for name, rgb in COLOURS:
        full = c.asset_path(IMAGE_PATH, name)
        if c.exists(full):
            c.log("exists", full)
            continue
        try:
            png = write_solid_png(os.path.join(scratch, name + ".png"), WIDTH, HEIGHT, rgb)
        except Exception as exc:  # noqa: BLE001
            c.log_error("write png " + name, exc)
            continue

        task = unreal.AssetImportTask()
        c.set_props(
            task,
            [
                ("filename", png),
                ("destination_path", IMAGE_PATH),
                ("destination_name", name),
                ("replace_existing", True),
                ("replace_existing_settings", True),
                ("automated", True),
                ("save", True),
            ],
            "AssetImportTask " + name,
        )
        tasks.append(task)
        pending.append((name, full))

    if not tasks:
        return []

    try:
        c.asset_tools().import_asset_tasks(tasks)
    except Exception as exc:  # noqa: BLE001
        c.log_error("import_asset_tasks (flashback stills)", exc)
        return []

    imported = []
    for name, full in pending:
        if c.exists(full):
            c.log("created", full, "{0}x{1}".format(WIDTH, HEIGHT))
            imported.append(full)
        else:
            c.log("FAILED", full, "import produced no asset")
    return imported


if __name__ == "__main__":
    run()
    c.print_summary("placeholder textures")
