#!/usr/bin/env python3
"""Build read-only cheat reference pages for exact-CRC cartridge images.

These pages do not enable, install or apply any cheat. Rebuild into a fresh
output pack: old viewer files must never survive a changed-ROM CRC rejection.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import tempfile
import textwrap
import unicodedata
import zlib

CATALOG_HEADER = struct.Struct("<4sHHII")
CATALOG_RECORD = struct.Struct("<88s128s20s12sB7x")
INLINE_METADATA = struct.Struct("<IBBB")
VIEW_HEADER = struct.Struct("<4sHH")
MAX_ENTRIES = 1200
# Catalog genre IDs are defined by build_index.GENRES; zero is the UI All filter.
MAX_GENRE_ID = 25
MAX_LINES = 1024
LINE_BYTES = 40
LINE_CONTENT = 38
MAX_GROUP_BYTES = 65536
CSV_FIELDS = {"game", "rom", "source_crc32", "pack_folder", "packs", "source"}
NATIVE_LINE = re.compile(
    r'\{\s*([+-])\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*'
    r'(0[xX][0-9a-fA-F]+)\s*,\s*(0[xX][0-9a-fA-F]+|\d+)\s*,\s*'
    r'("(?:[^"\\]|\\.)*")\s*\}\s*'
)


class UnsafeSource(ValueError):
    pass


def _relative(path):
    if not isinstance(path, str) or not path or path.startswith("/") or "\\" in path:
        raise ValueError("unsafe relative path")
    if any(part in ("", ".", "..") or part.endswith((" ", ".")) for part in path.split("/")):
        raise ValueError("unsafe relative path")
    if any(ord(char) < 32 or ord(char) == 127 or char in ':*?"<>|' for char in path):
        raise ValueError("unsafe relative path")
    return PurePosixPath(path)


def _field(raw, label):
    end = raw.find(b"\0")
    if end < 0:
        raise ValueError(f"catalog {label} is not null-terminated")
    try:
        return raw[:end].decode("utf-8")
    except UnicodeDecodeError as error:
        raise ValueError(f"catalog {label} is not valid UTF-8") from error


def _catalog(data):
    if len(data) < CATALOG_HEADER.size:
        raise ValueError("catalog header is truncated")
    magic, version, count, signature, reserved = CATALOG_HEADER.unpack_from(data)
    if magic != b"TPCE" or version != 2 or reserved or count > MAX_ENTRIES:
        raise ValueError("expected a bounded TPCE version 2 catalog")
    if len(data) != CATALOG_HEADER.size + count * CATALOG_RECORD.size:
        raise ValueError("catalog size does not match its record count")
    payload = data[CATALOG_HEADER.size:]
    if zlib.crc32(payload) & 0xFFFFFFFF != signature:
        raise ValueError("catalog generation checksum mismatch")
    records = {}
    keys = set()
    canonical_paths = set()
    for number in range(count):
        offset = number * CATALOG_RECORD.size
        fields = CATALOG_RECORD.unpack_from(payload, offset)
        title, path, genre, region = [_field(raw, label) for raw, label in
                                      zip(fields[:4], ("title", "path", "genre", "region"))]
        kind = fields[4]
        _relative(path)
        if kind not in (1, 2, 3) or PurePosixPath(path).suffix.lower() not in (".pce", ".sgx", ".cue"):
            raise ValueError("unsupported catalog media")
        key, genre_id, region_id, letter = INLINE_METADATA.unpack_from(payload, offset + 249)
        canonical = unicodedata.normalize("NFC", path).casefold()
        if (key != zlib.crc32(path.encode("utf-8")) & 0xFFFFFFFF or key in keys or
                canonical in canonical_paths):
            raise ValueError("invalid or duplicate catalog identity")
        if not (1 <= genre_id <= MAX_GENRE_ID and 1 <= region_id <= 4 and
                (letter == ord("#") or ord("A") <= letter <= ord("Z"))):
            raise ValueError("invalid catalog index metadata")
        keys.add(key)
        canonical_paths.add(canonical)
        records[path] = {"path": path, "title": title, "kind": kind, "key": key}
    return records, signature


def _checked_source(root, relative, directory=False):
    parts = _relative(relative).parts
    path = root
    for position, part in enumerate(parts):
        path = path / part
        info = path.lstat()
        if stat.S_ISLNK(info.st_mode):
            raise UnsafeSource("symlink input is not allowed")
        must_be_directory = position < len(parts) - 1 or directory
        if must_be_directory and not stat.S_ISDIR(info.st_mode):
            raise ValueError("source component is not a directory")
        if not must_be_directory and not stat.S_ISREG(info.st_mode):
            raise ValueError("source is not a regular file")
    return path


def _open_regular(path):
    # A final-component symlink introduced after validation must not be followed.
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            raise UnsafeSource("source is not a regular file")
        return os.fdopen(descriptor, "rb")
    except BaseException:
        os.close(descriptor)
        raise


def _rom_crc(path):
    checksum = 0
    with _open_regular(path) as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum = zlib.crc32(chunk, checksum)
    return checksum & 0xFFFFFFFF


def _group(path):
    with _open_regular(path) as stream:
        data = stream.read(MAX_GROUP_BYTES + 1)
    if len(data) > MAX_GROUP_BYTES:
        raise ValueError("native cheat group is too large")
    text = data.decode("utf-8-sig")
    rows = [line.strip() for line in text.splitlines() if line.strip()]
    if not 1 <= len(rows) <= 16:
        raise ValueError("a native group must contain 1 to 16 complete code lines")
    codes = []
    for line in rows:
        match = NATIVE_LINE.fullmatch(line)
        if not match:
            raise ValueError("malformed or unsupported native code line")
        marker, first, second, third, address, value, description = match.groups()
        if marker != "-":
            raise ValueError("active codes are not accepted")
        if (int(first), int(second), int(third)) != (1, 0, 1):
            raise ValueError("unsupported native code flags")
        address_value = int(address, 16)
        data_value = int(value, 16 if value.lower().startswith("0x") else 10)
        if not 0 <= address_value <= 0xFFFFFF or not 0 <= data_value <= 0xFF:
            raise ValueError("native code address or value is out of range")
        description_value = json.loads(description)
        codes.append((address_value, data_value, description_value))
    return {"name": path.stem, "codes": codes}


def _ascii(text):
    result = []
    for char in text:
        value = ord(char)
        if 32 <= value < 127:
            result.append(char)
        else:
            result.append(f"\\u{value:04X}" if value <= 0xFFFF else f"\\U{value:08X}")
    return "".join(result)


def _wrapped(text):
    return textwrap.wrap(_ascii(text), width=LINE_CONTENT, break_long_words=True,
                         break_on_hyphens=False, replace_whitespace=False,
                         drop_whitespace=False) or [""]


def _viewer(checksum, groups):
    lines = ["VIEW ONLY - NOT APPLIED", f"ROM CRC32: {checksum:08X}",
             f"GROUPS: {len(groups)} / ALL CODES OFF"]
    codes_total = 0
    for number, group in enumerate(groups, 1):
        codes = group["codes"]
        codes_total += len(codes)
        label = f"GROUP {number} - {len(codes)} CODE" + ("S (COMPOUND)" if len(codes) > 1 else "")
        lines.extend(["", label])
        lines.extend(_wrapped("NAME: " + group["name"]))
        for address, value, description in codes:
            lines.append(f"OFF {address:06X}:{value:02X}")
            lines.extend(_wrapped(description))
    if len(lines) > MAX_LINES:
        raise ValueError(f"viewer exceeds {MAX_LINES} lines; no groups were truncated")
    encoded = bytearray(VIEW_HEADER.pack(b"TPCH", 1, len(lines)))
    for line in lines:
        raw = line.encode("ascii")
        if len(raw) > LINE_CONTENT:
            raise ValueError("internal viewer line exceeds fixed display width")
        encoded.extend(raw + b"\0" * (LINE_BYTES - len(raw)))
    return bytes(encoded), len(lines), codes_total


def _read_bindings(path):
    if path.is_symlink():
        raise UnsafeSource("bindings CSV must not be a symlink")
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream, strict=True)
        if (reader.fieldnames is None or len(reader.fieldnames) != len(CSV_FIELDS) or
                set(reader.fieldnames) != CSV_FIELDS):
            raise ValueError("bindings CSV columns do not match the expected schema")
        rows = []
        for number, row in enumerate(reader, 2):
            if None in row or any(value is None for value in row.values()):
                raise ValueError(f"bindings CSV row {number} is truncated or has extra fields")
            rows.append((number, row))
    return rows


def _prepare(records, signature, rows, library_root, bindings_path):
    skipped = []
    candidates = {}
    prepared = []
    files = []
    groups_total = 0

    def skip(number, row, reason, detail=None):
        item = {"row": number, "game": row["game"], "rom": row["rom"], "reason": reason}
        if detail:
            item["detail"] = str(detail)
        skipped.append(item)

    for number, row in rows:
        try:
            _relative(row["rom"])
            _relative(row["pack_folder"])
        except ValueError as error:
            skip(number, row, "invalid_binding", error)
            continue
        path = row["rom"]
        if path.startswith("REFERENCE ONLY") and not row["source_crc32"].strip():
            skip(number, row, "unbound_reference")
        elif PurePosixPath(path).suffix.lower() == ".cue" or records.get(path, {}).get("kind") == 2:
            skip(number, row, "cd_not_bound")
        elif path not in records:
            skip(number, row, "not_in_catalog")
        else:
            candidates.setdefault(path, []).append((number, row))

    for path, matching_rows in candidates.items():
        if len(matching_rows) != 1:
            for number, row in matching_rows:
                skip(number, row, "duplicate_binding")
            continue
        number, row = matching_rows[0]
        crc_text = row["source_crc32"].strip()
        try:
            if not re.fullmatch(r"[0-9a-fA-F]{8}", crc_text):
                raise ValueError("source_crc32 must contain exactly eight hexadecimal digits")
            expected_groups = int(row["packs"], 10)
            if expected_groups <= 0:
                raise ValueError("packs must be a positive group count")
        except ValueError as error:
            skip(number, row, "invalid_binding", error)
            continue
        try:
            rom = _checked_source(library_root, path)
            checksum = _rom_crc(rom)
            if checksum != int(crc_text, 16):
                skip(number, row, "crc_mismatch",
                     f"actual {checksum:08X}; binding {int(crc_text, 16):08X}")
                continue
            folder = _checked_source(library_root, row["pack_folder"], directory=True)
            sources = []
            for source in folder.iterdir():
                info = source.lstat()
                if stat.S_ISLNK(info.st_mode):
                    raise UnsafeSource("symlink inside native cheat folder")
                if source.suffix.lower() == ".txt":
                    if not stat.S_ISREG(info.st_mode):
                        raise UnsafeSource("native cheat group is not a regular file")
                    sources.append(source)
            sources.sort(key=lambda item: (item.name.casefold(), item.name))
            if len(sources) != expected_groups:
                skip(number, row, "pack_count_mismatch",
                     f"binding {expected_groups}; actual {len(sources)}")
                continue
        except UnsafeSource as error:
            skip(number, row, "unsafe_source", error)
            continue
        except (OSError, ValueError) as error:
            skip(number, row, "source_unavailable", error)
            continue
        try:
            groups = [_group(source) for source in sources]
            data, line_count, code_count = _viewer(checksum, groups)
        except (OSError, ValueError, UnicodeError) as error:
            skip(number, row, "invalid_group", error)
            continue
        key = f'{records[path]["key"]:08X}'
        relative_output = f"cheats/{key}.bin"
        prepared.append((relative_output, data))
        groups_total += len(groups)
        files.append({"key": key, "path": relative_output, "rom": path,
                      "crc32": f"{checksum:08X}", "groups": len(groups),
                      "codes": code_count, "lines": line_count,
                      "sha256": hashlib.sha256(data).hexdigest()})

    eligible = [path for path, item in records.items()
                if item["kind"] != 2 and PurePosixPath(path).suffix.lower() in (".pce", ".sgx")]
    report = {"schema": 1, "status": "PASS", "mode": "view_only", "codes_applied": False,
              "catalog_entries": len(records), "eligible_games": len(eligible),
              "bindings_rows": len(rows), "generated_games": len(files),
              "generated_groups": groups_total, "catalog_signature": f"{signature:08X}",
              "games_without_binding": sum(path not in candidates for path in eligible),
              "source_bindings": str(bindings_path), "library_root": str(library_root),
              "rebuild_requires_fresh_directory": True, "files": files, "skipped": skipped}
    return prepared, report


def _check_output(directory):
    if directory.is_symlink() or directory.exists() and not directory.is_dir():
        raise ValueError("output must be a real directory")
    cheats = directory / "cheats"
    if cheats.is_symlink():
        raise ValueError("output cheats directory must not be a symlink")
    if cheats.exists() and (not cheats.is_dir() or any(cheats.iterdir())):
        raise ValueError("cheat viewer pack already exists; rebuild in a fresh directory")
    manifest = directory / "cheat-view-manifest.json"
    if manifest.exists() or manifest.is_symlink():
        raise ValueError("cheat viewer manifest already exists; rebuild in a fresh directory")


def _publish(directory, prepared, report):
    _check_output(directory)
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "cheats").mkdir(exist_ok=True)
    staged = []
    try:
        outputs = prepared + [("cheat-view-manifest.json",
                               (json.dumps(report, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))]
        for relative, contents in outputs:
            destination = directory / relative
            with tempfile.NamedTemporaryFile(dir=destination.parent, prefix=".cheat-view-",
                                             suffix=".tmp", delete=False) as stream:
                temporary = Path(stream.name)
                staged.append((temporary, destination))
                stream.write(contents)
                stream.flush()
                os.fsync(stream.fileno())
            if relative.startswith("cheats/"):
                temporary.chmod(0o444)
        for temporary, destination in staged:
            os.replace(temporary, destination)
    finally:
        for temporary, _ in staged:
            temporary.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog2", type=Path)
    parser.add_argument("bindings", type=Path)
    parser.add_argument("library_root", type=Path)
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args(argv)
    if args.catalog2.is_symlink() or args.library_root.is_symlink():
        raise UnsafeSource("catalog and library root must not be symlinks")
    with args.catalog2.open("rb") as source:
        data = source.read(CATALOG_HEADER.size + MAX_ENTRIES * CATALOG_RECORD.size + 1)
    records, signature = _catalog(data)
    rows = _read_bindings(args.bindings)
    library_root = args.library_root.resolve(strict=True)
    if not library_root.is_dir():
        raise ValueError("library root is not a directory")
    prepared, report = _prepare(records, signature, rows, library_root, args.bindings)
    _publish(args.output_directory, prepared, report)
    print(f'Built {report["generated_games"]} VIEW ONLY pages; '
          f'{len(report["skipped"])} bindings skipped and reported')
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, csv.Error) as error:
        import sys
        print(error, file=sys.stderr)
        raise SystemExit(1)
