#!/usr/bin/env python3
"""Build a cartridge prototype and a hardware-independent visual preview."""
from pathlib import Path
import argparse
import importlib.util
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / "vendor/krikzz"
BUILD = ROOT / "build"


def run(args):
    subprocess.run([str(x) for x in args], cwd=VENDOR, check=True)



def _tool(name):
    path = ROOT / "tools" / (name + ".py")
    spec = importlib.util.spec_from_file_location("rom_build_" + name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _validate_preview_art(data, path):
    import struct
    if len(data) == 3488:
        offset, count = 0, 16
    elif (len(data) == 3550 and
          struct.unpack_from("<4sBBBBHHI", data) == (b"TPA2", 2, 96, 72, 2, 108, 14, 0) and
          not data[93] & 0xf0):
        offset, count = 16, 32
    else:
        raise ValueError(f"Invalid preview art layout: {path}")
    if any(word > 0x1ff for word in struct.unpack_from(f"<{count}H", data, offset)):
        raise ValueError(f"Invalid preview RGB333 palette: {path}")


def _app_data_root(app_root):
    if (not isinstance(app_root,str) or len(app_root)>16 or
            not re.fullmatch(r"[A-Za-z0-9_-]+",app_root)):
        raise ValueError("app root must be a safe single cartridge directory (1..16 characters)")
    return app_root + "/data"


def _prepare_preview_cheats(catalog, indices, stage, launch_path, package_root="teo-cheats1"):
    prep = _tool("prepare_cheat_groups")
    stage = prep._checked_absolute(Path(stage), directory=True)
    prep.source._checked_source(stage, package_root, directory=True)
    records, generation = prep.launch.parse_catalog(catalog)
    identities = prep._launch_rows(prep._read_input(Path(launch_path), 16384), records, generation)
    entries = []
    bank = 7
    bank_bytes = 0
    for index in indices:
        row = records[index]
        size, crc = identities[row["path"]]
        item = {"size": size, "crc32": crc, "data": b"", "bank": 0}
        path = stage / package_root / f"{row['key']:08X}.tcg"
        if prep.launch.launchable(row) and (path.exists() or path.is_symlink()):
            data = prep._read_input(path, 8192)
            prep.codec.decode_package(data, row["key"], size, crc)
            if bank_bytes + len(data) > 8192:
                bank += 1
                bank_bytes = 0
            if bank > 10:
                raise ValueError("Preview cheat data exceed four free banks; BNK11 is reserved for pure selector code")
            item.update(data=data, bank=bank)
            bank_bytes += len(data)
        entries.append(item)
    return entries


def prepare_preview(dest, menu, preview_catalog=None, preview_cheats=None, preview_launch=None, app_root=None):
    import struct

    if app_root is not None:
        _app_data_root(app_root)
    cheats_enabled = preview_cheats is not None or preview_launch is not None
    if cheats_enabled and (preview_cheats is None or preview_launch is None or preview_catalog is None):
        raise ValueError("Preview cheats require explicit catalog2, launch metadata and package stage")
    if preview_catalog is not None:
        catalog = Path(preview_catalog).read_bytes()
        _tool("build_launch_metadata").parse_catalog(catalog)
    else:
        catalog, _ = _tool("build_index").build_pack((menu / "catalog.bin").read_bytes())
    count = struct.unpack_from("<H", catalog, 6)[0]
    records = [catalog[16+i*256:16+(i+1)*256] for i in range(count)]
    titles = [record[:88].split(b"\0", 1)[0].decode("utf-8") for record in records]
    wanted = ["1943 Kai (Japan)", "Air Zonk (USA)",
              "Akumajou Dracula X - Chi no Rondo (Japan)", "Alien Crush (USA)",
              "Bomberman '94 (Japan)", "Bonk's Adventure (USA)"]
    indices = [titles.index(title) for title in wanted]
    years = [0] * count
    year_path = menu / "years.bin"
    if year_path.exists() or year_path.is_symlink():
        year_data = _tool("prepare_cheat_groups")._read_input(year_path, 16 + count * 8)
        years = _tool("build_game_metadata").parse_year_pack(year_data, catalog)
    cheats = (_prepare_preview_cheats(catalog, indices, preview_cheats, preview_launch,
                                       app_root + "/cheats" if app_root is not None else "teo-cheats1")
              if cheats_enabled else None)
    def array(name, data):
        return ("static const unsigned char " + name + "[] = {\n" +
                ",\n".join(",".join(str(x) for x in data[i:i+32])
                            for i in range(0, len(data), 32)) + "\n};\n")
    output = [f"#define PREVIEW_COUNT {len(indices)}\n#define PREVIEW_CHEATS {int(cheats_enabled)}\n"]
    coverage_path = menu / "cheats.idx"
    if coverage_path.exists():
        coverage = _tool("build_cheat_index").parse_index(coverage_path.read_bytes(), catalog)
        output.append("#define PREVIEW_COVERAGE 1\nstatic const unsigned short preview_cheat_counts[PREVIEW_COUNT][2] = {" +
                      ",".join("{%d,%d}" % coverage[index] for index in indices) + "};\n")
    available = []
    lengths = []
    description_lengths, description_available = [], []
    for slot, index in enumerate(indices):
        output.append(f'#pragma rodata-name ("BNK{slot+1:02}")\n')
        flags = 0
        pair_lengths = []
        for which, code in (("cover", "c"), ("screen", "s")):
            path = menu / "art" / f"{index:06d}-{code}.bin"
            data = path.read_bytes() if path.exists() else bytes(3488)
            _validate_preview_art(data, path)
            pair_lengths.append(len(data))
            if path.exists():
                flags |= 1 if code == "c" else 2
            output.append(array(f"preview_{which}_{slot}", data))
        path = menu / "desc" / f"{index:06d}.txt"
        data = path.read_bytes() if path.exists() else b""
        if len(data) > 8192 or any(c != 10 and not 32 <= c <= 126 for c in data) or any(len(line) > 38 for line in data.split(b"\n")):
            raise ValueError(f"Invalid bounded ASCII preview description: {path}")
        description_lengths.append(len(data))
        description_available.append(int(path.exists()))
        output.append(array(f"preview_desc_{slot}", data + b"\0"))
        available.append(flags)
        lengths.append(pair_lengths)
        if sum(pair_lengths) + len(data) + 1 > 8192:
            raise ValueError("Preview artwork/description/record exceed their bank")
    if cheats is not None:
        for slot, item in enumerate(cheats):
            if item["data"]:
                output.append(f'#pragma rodata-name ("BNK{item["bank"]:02}")\n')
                output.append(array(f"preview_cheat_{slot}", item["data"]))
    output.append('#pragma rodata-name ("RODATA")\n')
    for slot, index in enumerate(indices):
        output.append(array(f"preview_record_{slot}", records[index]))
    for table, stem in (("preview_records", "record"), ("preview_covers", "cover"),
                        ("preview_screens", "screen"), ("preview_descriptions", "desc")):
        output.append("static const unsigned char * const " + table + "[] = {" +
                      ",".join(f"preview_{stem}_{i}" for i in range(len(indices))) + "};\n")
    output.append(array("preview_art_available", bytes(available)))
    output.append(array("preview_description_available", bytes(description_available)))
    output.append("static const unsigned short preview_release_years[PREVIEW_COUNT] = {" +
                  ",".join(str(years[index]) for index in indices) + "};\n")
    output.append("static const unsigned short preview_description_sizes[PREVIEW_COUNT] = {" +
                  ",".join(map(str, description_lengths)) + "};\n")
    output.append("static const unsigned short preview_art_sizes[PREVIEW_COUNT][2] = {" +
                  ",".join("{" + ",".join(map(str, pair)) + "}" for pair in lengths) + "};\n")
    if cheats is not None:
        output.append("static const unsigned char * const preview_cheat_data[PREVIEW_COUNT] = {" +
                      ",".join(f"preview_cheat_{i}" if item["data"] else "0" for i, item in enumerate(cheats)) + "};\n")
        output.append(array("preview_cheat_banks", bytes(item["bank"] + 4 if item["bank"] else 0 for item in cheats)))
        output.append("static const unsigned short preview_cheat_sizes[PREVIEW_COUNT] = {" +
                      ",".join(str(len(item["data"])) for item in cheats) + "};\n")
        output.append("static const NativeBridgeTarget preview_cheat_targets[PREVIEW_COUNT] = {" +
                      ",".join("{" + f"{item['size']}UL,0x{item['crc32']:08X}UL" + "}" for item in cheats) + "};\n")
    (dest / "preview_asset.h").write_text("".join(output), encoding="ascii")

def build(binary_dir, preview=False, probe=False, preview_assets=None, usb_trace=False,
          metadata_root=None, output_dir=None, preview_catalog=None, preview_cheats=None, preview_launch=None,
          resource_root=None, app_root=None):
    if usb_trace and (preview or probe):
        raise ValueError("USB trace is a separate native browser build")
    if app_root is not None:
        data_root = _app_data_root(app_root)
        if any(root is not None and root != data_root for root in (metadata_root,resource_root)):
            raise ValueError("app root requires matching metadata and resource directories")
        metadata_root = resource_root = data_root
    for root, label in ((metadata_root, "metadata"), (resource_root, "resource")):
        if root is not None:
            if (not isinstance(root, str) or len(root) > 96 or
                    not re.fullmatch(r"[A-Za-z0-9_-]+(?:/[A-Za-z0-9_.-]+)*", root) or
                    any(part in (".", "..") or part.endswith(".") for part in root.split("/"))):
                raise ValueError(label + " root must be a safe relative cartridge directory")
    tag = "usb-trace" if usb_trace else "probe" if probe else "preview" if preview else "prototype"
    build_root = Path(output_dir) if output_dir is not None else BUILD
    name = "TeoPCE-USBTrace.pce" if usb_trace else "TeoPCE-LaunchProbe.pce" if probe else "TeoPCE-Preview.pce" if preview else "TeoPCE.pce"
    output_rom = build_root / name
    dest = build_root / tag
    if output_dir is not None and (build_root.is_symlink() or dest.exists() or dest.is_symlink() or output_rom.exists() or output_rom.is_symlink()):
        raise ValueError("isolated build output must be new")
    build_root = build_root.resolve()
    output_rom = build_root / name
    dest = build_root / tag
    dest.mkdir(parents=True, exist_ok=True)
    cc, assembler, linker = (binary_dir / name for name in ("cc65", "ca65", "ld65"))
    if preview:
        prepare_preview(dest, preview_assets or BUILD / "card/teo-menu", preview_catalog=preview_catalog,
                        preview_cheats=preview_cheats, preview_launch=preview_launch, app_root=app_root)
    objects = []
    for name in ("crt0", "sys_asm", "everdrive_asm", "pce_callax"):
        output = dest / f"{name}.o"
        source = ROOT / "rom/pce_callax.s" if name == "pce_callax" else VENDOR / f"{name}.s"
        run([assembler, "--cpu", "HuC6280", "-t", "nes", "-I", VENDOR,
             "-o", output, source])
        objects.append(output)
    sources = [("sys", VENDOR / "sys.c"), ("everdrive", VENDOR / "everdrive.c")]
    if probe:
        sources += [("device", ROOT / "rom/device.c"),
                    ("probe_logic", ROOT / "rom/probe_logic.c"),
                    ("main", ROOT / "rom/probe_main.c")]
    else:
        names = ["card_io", "ui_hint", "find", "recent", "library_ui", "device", "device_return", "input", "navigation", "preferences", "pack", "catalog", "launch",
                 "cheat_groups", "cheat_notes", "cheat_selection", "cheat_cache", "cheat_recovery", "cheat_legacy", "game_info", "game_meta", "description", "cheat_index",
                 "cheat_store", "cheat_store_codec", "cheat_store_io", "cheat_cd"]
        if not preview:
            names += ["native_bridge", "cd_guard", "cd_guard_packet"]
        if usb_trace:
            names += ["trace"]
        names += ["picture"]
        names += ["preview"] if preview else ["storage"]
        sources += [(name, ROOT / "rom" / f"{name}.c") for name in names]
        sources += [("main", ROOT / "rom/main.c")]
    for name, path in sources:
        asm = dest / f"{name}.s"
        output = dest / f"{name}.o"
        command = [cc, "--cpu", "HuC6280", "-O", "-I", VENDOR, "-I", dest, "-o", asm]
        if app_root is not None:
            command += ["-D", 'TEO_APP_ROOT="' + app_root + '"']
        if metadata_root is not None:
            command += ["-D", 'TEO_METADATA_ROOT="' + metadata_root + '"']
        if resource_root is not None:
            command += ["-D", 'TEO_RESOURCE_ROOT="' + resource_root + '"']
        if usb_trace:
            command += ["-D", "USB_TRACE"]
        if preview:
            command += ["-D", "PREVIEW"]
        run(command + [path])
        run([assembler, "--cpu", "HuC6280", "-t", "nes", "-I", VENDOR, "-o", output, asm])
        objects.append(output)
    linker_config = VENDOR / "pce.cfg"
    if preview:
        # The paged reader executes in its own bank; assets map only during memcpy.
        # Keep native 128KiB untouched and pad this developer preview to 256KiB.
        config = linker_config.read_text()
        memory = "".join(f"    BNK{bank:02}: start = $4000, size = $2000, file = %O, fill = yes;\n"
                         for bank in range(12, 28))
        config = config.replace("    RAM:     start", memory + "    RAM:     start", 1)
        config = config.replace("    BSS:      load", "    BNK12: load = BNK12, type = ro, optional = yes;\n    BNK13: load = BNK13, type = ro, optional = yes;\n    BNK14: load = BNK14, type = ro, optional = yes;\n    BNK15: load = BNK15, type = ro, optional = yes;\n    BNK16: load = BNK16, type = ro, optional = yes;\n    BNK17: load = BNK17, type = ro, optional = yes;\n    BSS:      load", 1)
        linker_config = dest / "pce-preview.cfg"
        linker_config.write_text(config)
    run([linker, "-C", linker_config, "-o", output_rom, *objects,
         binary_dir.parent / "lib/nes.lib", "-m", dest / "memory.map"])
    map_path = dest / "memory.map"
    if map_path.exists():
        memory_map = map_path.read_text()
        found = re.search(r"^BSS +([0-9A-F]+) +([0-9A-F]+) +([0-9A-F]+)", memory_map, re.M)
        if found:
            bss_end = int(found[2], 16)
            headroom = 0x4000 - bss_end - 1
            if headroom < 0x500:
                raise ValueError("native RAM leaves less than the configured 1280-byte software stack")
            print(f"RAM: BSS ends at ${bss_end:04X}; {headroom} bytes remain for stack")
        if not preview and not probe:
            module = memory_map.split("native_bridge.o:", 1)[1].split("\n\n", 1)[0]
            bss = re.search(r"BSS +Offs=[0-9A-F]+ +Size=([0-9A-F]+)", module)
            if not bss or int(bss[1], 16) != 64:
                raise ValueError("reviewed native bridge BSS must remain exactly64 bytes")
    print(f"Built {output_rom} ({output_rom.stat().st_size} bytes)")
    return output_rom


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cc65-bin", type=Path, required=True)
    parser.add_argument("--preview-assets", type=Path, help="Menu assets directory for the six-game visual preview")
    parser.add_argument("--preview-catalog", type=Path, help="Validated catalog2.bin for preview metadata; assets keep original raw IDs")
    parser.add_argument("--preview-cheats", type=Path, help="Private stage containing exact-ROM teo-cheats1/*.tcg")
    parser.add_argument("--preview-launch", type=Path, help="Independent TLA1 launch.bin matching the explicit preview catalog")
    parser.add_argument("--app-root", help="Single application folder: ROM data, cheats and user state use its subdirectories")
    parser.add_argument("--metadata-root", help="Isolated cartridge metadata directory; default is teo-menu3")
    parser.add_argument("--resource-root", help="Cartridge art, description and legacy code directory; default is teo-menu2")
    parser.add_argument("--output-dir", type=Path, help="New isolated build root; never replaces existing artifacts")
    parser.add_argument("--native-only", action="store_true", help="Build the console ROM without preview or diagnostic assets")
    parser.add_argument("--usb-trace", action="store_true",
                        help="Build only the separate developer USB boot-trace ROM; requires a connected reading host")
    args = parser.parse_args()
    binary_dir = args.cc65_bin.resolve()
    if args.usb_trace:
        build(binary_dir, usb_trace=True, metadata_root=args.metadata_root, output_dir=args.output_dir,
              resource_root=args.resource_root, app_root=args.app_root)
        return
    build(binary_dir, preview=False, metadata_root=args.metadata_root, output_dir=args.output_dir,
          resource_root=args.resource_root, app_root=args.app_root)
    if args.native_only:
        return
    build(binary_dir, preview=True, preview_assets=args.preview_assets, metadata_root=args.metadata_root, output_dir=args.output_dir, preview_catalog=args.preview_catalog,
          preview_cheats=args.preview_cheats, preview_launch=args.preview_launch, resource_root=args.resource_root, app_root=args.app_root)
    build(binary_dir, probe=True, metadata_root=args.metadata_root, output_dir=args.output_dir,
          resource_root=args.resource_root, app_root=args.app_root)


if __name__ == "__main__":
    main()
