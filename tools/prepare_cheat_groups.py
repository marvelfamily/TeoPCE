#!/usr/bin/env python3
"""Prepare exact-ROM whole cheat groups in a new local build stage.

Creates teo-cheats1/KEY8.tcg and a private provenance manifest only. It never
installs native files, enables cheats, opens USB or writes to the source library.
"""
import argparse
from collections import Counter
import csv
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import tempfile
import unicodedata
import zlib

sys.dont_write_bytecode = True
TOOLS = Path(__file__).resolve().parent
BUILD = TOOLS.parent / "build"


def _module(name, filename):
    spec = importlib.util.spec_from_file_location(name, TOOLS / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


codec = _module("prepare_group_codec", "build_cheat_groups.py")
launch = _module("prepare_group_launch", "build_launch_metadata.py")
source = codec.source
REASON_NAMES = [(codec.MALFORMED, "malformed_source"), (codec.FLAGS, "unsupported_flags"),
                (codec.ACTIVE, "active_source"), (codec.ADDRESS, "unsupported_address"),
                (codec.COUNT, "code_count")]


def _require(value, message):
    if not value:
        raise ValueError(message)


def _statkey(info):
    return info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns


def _checked_absolute(path, directory=False):
    path = path.absolute()
    return source._checked_source(Path(path.anchor), path.relative_to(path.anchor).as_posix(), directory)


def _read_input(path, limit):
    path = _checked_absolute(path)
    with source._open_regular(path) as stream:
        before = os.fstat(stream.fileno())
        data = stream.read(limit + 1)
        after = os.fstat(stream.fileno())
    _require(len(data) <= limit, "metadata input exceeds bound")
    _require(_statkey(before) == _statkey(after) and len(data) == before.st_size,
             "metadata input changed during read")
    return data


def _launch_rows(data, records, generation):
    _require(len(data) >= launch.HEADER.size, "launch header truncated")
    magic, version, width, count, actual_generation, reserved = launch.HEADER.unpack_from(data)
    _require((magic, version, width, count, actual_generation, reserved) ==
             (b"TLA1", 1, launch.RECORD.size, len(records), generation, 0),
             "launch header, count or catalog generation mismatch")
    _require(len(data) == launch.HEADER.size + count * launch.RECORD.size,
             "launch length does not match record count")
    result = {}
    for row in records:
        key, size, crc = launch.RECORD.unpack_from(data, launch.HEADER.size + row["index"] * launch.RECORD.size)
        _require(key == row["key"], "launch path key mismatch")
        _require(1 <= size <= launch.MAX_ROM if launch.eligible(row) else ((size, crc) == (0, 0) or (row["kind"] == 2 and launch.launchable(row) and 1 <= size <= launch.MAX_CUE)),
                 "launch size or unsupported identity fields")
        result[row["path"]] = (size, crc)
    return result


def _rom_identity(path):
    total, crc, digest = 0, 0, hashlib.sha256()
    with source._open_regular(path) as stream:
        before = os.fstat(stream.fileno())
        _require(1 <= before.st_size <= launch.MAX_ROM, "ROM size outside 1..8MiB")
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            total += len(chunk)
            _require(total <= launch.MAX_ROM, "ROM exceeds 8MiB")
            crc = zlib.crc32(chunk, crc)
            digest.update(chunk)
        after = os.fstat(stream.fileno())
    _require(_statkey(before) == _statkey(after) and total == before.st_size,
             "ROM changed during read")
    return total, crc & 0xFFFFFFFF, digest.hexdigest()


def _group(path):
    # Retain a bounded parser prefix, but hash all source bytes for provenance/ID.
    prefix = bytearray()
    digest = hashlib.sha256()
    identity = hashlib.sha256(path.stem.encode("utf-8") + b"\0")
    total = 0
    with source._open_regular(path) as stream:
        before = os.fstat(stream.fileno())
        for chunk in iter(lambda: stream.read(65536), b""):
            total += len(chunk)
            digest.update(chunk)
            identity.update(chunk)
            remaining = source.MAX_GROUP_BYTES + 1 - len(prefix)
            if remaining > 0:
                prefix.extend(chunk[:remaining])
        after = os.fstat(stream.fileno())
    _require(_statkey(before) == _statkey(after) and total == before.st_size,
             "group source changed during read")
    group = codec.parse_group(path.stem, bytes(prefix))
    group["id"] = int.from_bytes(identity.digest()[:4], "little")
    provenance = {"source_name": path.name, "source_bytes": total,
                  "source_sha256": digest.hexdigest(), "id": f"{group['id']:08X}",
                  "name": group["name"], "codes": len(group["codes"]),
                  "reason_flags": group["reason"],
                  "held_reasons": [name for mask, name in REASON_NAMES if group["reason"] & mask]}
    return group, provenance


def _prepare(records, identities, rows, library_root):
    by_path = {row["path"]: row for row in records}
    basename_counts = Counter(unicodedata.normalize("NFC", PurePosixPath(row["path"]).name).casefold()
                              for row in records if launch.eligible(row))
    candidates, skipped, prepared, files = {}, [], [], []

    def skip(number, row, reason, detail=None):
        item = {"row": number, "game": row["game"], "rom": row["rom"], "reason": reason}
        if detail is not None:
            item["detail"] = str(detail)
        skipped.append(item)

    for number, row in rows:
        try:
            source._relative(row["rom"])
            source._relative(row["pack_folder"])
        except ValueError as error:
            skip(number, row, "invalid_binding", error)
            continue
        if row["rom"] not in by_path:
            skip(number, row, "not_in_catalog")
        elif not launch.eligible(by_path[row["rom"]]):
            skip(number, row, "unsupported_media")
        else:
            candidates.setdefault(row["rom"], []).append((number, row))

    for path, bindings in candidates.items():
        if len(bindings) != 1:
            for number, row in bindings:
                skip(number, row, "duplicate_binding")
            continue
        number, row = bindings[0]
        basename = unicodedata.normalize("NFC", PurePosixPath(path).name).casefold()
        if basename_counts[basename] != 1:
            skip(number, row, "ambiguous_native_basename")
            continue
        try:
            crc_text = row["source_crc32"].strip()
            _require(re.fullmatch(r"[0-9A-Fa-f]{8}", crc_text), "source_crc32 must be eight hex digits")
            expected_groups = int(row["packs"], 10)
            _require(1 <= expected_groups <= 1024, "packs must be 1..1024")
        except ValueError as error:
            skip(number, row, "invalid_binding", error)
            continue
        try:
            rom_path = source._checked_source(library_root, path)
            size, crc, rom_sha = _rom_identity(rom_path)
            if crc != int(crc_text, 16):
                skip(number, row, "crc_mismatch", f"actual {crc:08X}; binding {int(crc_text, 16):08X}")
                continue
            if (size, crc) != identities[path]:
                skip(number, row, "launch_identity_mismatch", f"actual {size}/{crc:08X}; launch {identities[path][0]}/{identities[path][1]:08X}")
                continue
            folder = source._checked_source(library_root, row["pack_folder"], directory=True)
            group_paths = []
            for child in folder.iterdir():
                info = child.lstat()
                if stat.S_ISLNK(info.st_mode):
                    raise source.UnsafeSource("symlink inside group source folder")
                if child.suffix.lower() == ".txt":
                    if not stat.S_ISREG(info.st_mode):
                        raise source.UnsafeSource("group source is not a regular file")
                    group_paths.append(child)
            group_paths.sort(key=lambda item: (item.name.casefold(), item.name))
            if len(group_paths) != expected_groups:
                skip(number, row, "pack_count_mismatch", f"binding {expected_groups}; actual {len(group_paths)}")
                continue
            groups, provenance = [], []
            for child in group_paths:
                group, item = _group(child)
                groups.append(group)
                provenance.append(item)
            key = by_path[path]["key"]
            data = codec.encode_package(key, size, crc, groups)
            _require(codec.decode_package(data, key, size, crc) == groups, "TCG1 decode roundtrip mismatch")
        except source.UnsafeSource as error:
            skip(number, row, "unsafe_source", error)
            continue
        except (OSError, ValueError, UnicodeError) as error:
            skip(number, row, "source_or_package_invalid", error)
            continue
        relative = f"teo-cheats1/{key:08X}.tcg"
        prepared.append((relative, data))
        files.append({"key": f"{key:08X}", "path": relative, "rom": path,
                      "rom_size": size, "rom_crc32": f"{crc:08X}", "rom_sha256": rom_sha,
                      "binding_row": number, "binding_source": row["source"],
                      "pack_folder": row["pack_folder"], "groups": provenance,
                      "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                      "decode_roundtrip": True})
    return prepared, files, skipped


def _output_path(path, library_root=None):
    _require(not path.exists() and not path.is_symlink(), "fresh output stage required; no overwrite")
    parent = path.absolute().parent.resolve(strict=True)
    _require(parent.is_relative_to(BUILD.resolve()), "output must be a fresh local build stage")
    result = parent / path.name
    if library_root is not None:
        _require(not result.is_relative_to(library_root), "output cannot be inside source library")
    return result


def _publish(destination, prepared, report):
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix=".cheat-groups-") as tmp:
        stage = Path(tmp)
        (stage / "teo-cheats1").mkdir()
        payloads = prepared + [("cheat-group-manifest.json",
                               (json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode())]
        for relative, data in payloads:
            path = stage / relative
            with path.open("xb") as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            _require(path.read_bytes() == data, "stage readback mismatch")
        _output_path(destination)
        os.rename(stage, destination)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("catalog2", "launch", "bindings", "library-root", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args(argv)
    destination = _output_path(args.output)
    library_root = _checked_absolute(args.library_root, directory=True).resolve(strict=True)
    destination = _output_path(destination, library_root)
    inputs = {"catalog2": args.catalog2, "launch": args.launch, "bindings": args.bindings}
    blobs = {name: _read_input(path, 4 * 1024 * 1024) for name, path in inputs.items()}
    records, generation = launch.parse_catalog(blobs["catalog2"])
    identities = _launch_rows(blobs["launch"], records, generation)
    rows = source._read_bindings(_checked_absolute(args.bindings))
    prepared, files, skipped = _prepare(records, identities, rows, library_root)
    # Do not publish a pack built from authority files changed during source work.
    for name, path in inputs.items():
        _require(_read_input(path, 4 * 1024 * 1024) == blobs[name], "metadata authority changed during preparation")
    report = {"schema": 1, "status": "PASS_HOST_PREPARATION_ONLY", "mode": "whole_group_data",
              "codes_applied": False, "hardware_tested": False, "native_export_created": False,
              "catalog_generation": f"{generation:08X}", "catalog_entries": len(records),
              "eligible_hucards": sum(launch.eligible(row) for row in records),
              "bindings_rows": len(rows), "generated_games": len(files),
              "generated_groups": sum(len(item["groups"]) for item in files),
              "held_groups": sum(bool(group["reason_flags"]) for item in files for group in item["groups"]),
              "rebuild_requires_fresh_directory": True, "library_root": str(library_root),
              "input_snapshots": {name: {"path": str(path.absolute()), "bytes": len(blobs[name]),
                                          "sha256": hashlib.sha256(blobs[name]).hexdigest()} for name, path in inputs.items()},
              "tool_sources_sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in
                                       (Path(__file__), TOOLS / "build_cheat_groups.py", TOOLS / "build_cheat_view.py", TOOLS / "build_launch_metadata.py", TOOLS / "build_index.py")},
              "files": files, "skipped": skipped}
    _publish(destination, prepared, report)
    print(f"Prepared {len(files)} exact-ROM packages / {report['generated_groups']} whole groups; "
          f"{report['held_groups']} held; {len(skipped)} bindings skipped")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, csv.Error) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
