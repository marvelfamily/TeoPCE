#!/usr/bin/env python3
"""Build a generation-matched console catalog and compact browsing index.

The two output files are replaced individually after both are staged. Their
matching count and signature let the console reject an interrupted pair update.
"""
import argparse
import os
from pathlib import Path, PurePosixPath
import struct
import tempfile
import unicodedata
import zlib

SOURCE_HEADER = struct.Struct("<4sHH")
PACK_HEADER = struct.Struct("<4sHHII")
RECORD = struct.Struct("<88s128s20s12sB7x")
INDEX_RECORD = struct.Struct("<BBBBI")
MAX_ENTRIES = 1200
MAX_SOURCE_SIZE = SOURCE_HEADER.size + MAX_ENTRIES * RECORD.size

# Zero belongs to the UI's "All" filter, never to an unknown record genre.
GENRES = {
    "Action": 1, "Adventure": 2, "Beat'em Up": 3, "Board": 4,
    "Fighting": 5, "Gambling": 6, "Lightgun Shooter": 7, "Pinball": 8,
    "Platform": 9, "Puzzle": 10, "Quiz": 11, "Racing": 12,
    "Role-playing (RPG)": 13, "Shoot'em Up": 14, "Shooter": 15,
    "Simulation": 16, "Sports": 17, "Sports with Animals": 18,
    "Strategy": 19, "Unknown": 20,
    "Utility": 21, "Music": 22, "Compilation": 23,
    "Multimedia": 24, "Education": 25,
}
REGIONS = {"Japan": 1, "USA": 2, "English": 3}


def _field(raw, name, index):
    end = raw.find(b"\0")
    if end < 0:
        raise ValueError(f"record {index}: {name} is not null-terminated")
    try:
        return raw[:end].decode("utf-8")
    except UnicodeDecodeError as error:
        raise ValueError(f"record {index}: {name} is not valid UTF-8") from error


def _validate_path(path, index):
    parts = path.split("/")
    if (not path or path.startswith("/") or "\\" in path or
            any(part in ("", ".", "..") or part.endswith((" ", ".")) for part in parts) or
            any(ord(char) < 32 or ord(char) == 127 or char in ':*?"<>|' for char in path) or
            PurePosixPath(path).suffix.lower() not in (".pce", ".sgx", ".cue")):
        raise ValueError(f"record {index}: unsafe or unsupported launch path")


def build_pack(data):
    """Return a v2 catalog with inline identity metadata and its compact index.

    Each v2 record preserves bytes 0..248 of its v1 source. Bytes 249..255
    contain stable key (u32 LE), genre ID, region ID and initial, in that order.
    Both headers sign the complete enriched v2 record payload.
    """
    if len(data) < SOURCE_HEADER.size:
        raise ValueError("catalog header is truncated")
    magic, version, count = SOURCE_HEADER.unpack_from(data)
    if magic != b"TPCE" or version != 1:
        raise ValueError("expected a TPCE version 1 catalog")
    if count > MAX_ENTRIES:
        raise ValueError(f"catalog exceeds console limit of {MAX_ENTRIES} entries")
    expected_size = SOURCE_HEADER.size + count * RECORD.size
    if len(data) != expected_size:
        raise ValueError("catalog size does not match its record count")

    records = data[SOURCE_HEADER.size:]
    enriched_records = bytearray(records)
    index_records = bytearray()
    identities = set()
    canonical_paths = set()
    for index in range(count):
        title_raw, path_raw, genre_raw, region_raw, kind = RECORD.unpack_from(
            records, index * RECORD.size)
        title = _field(title_raw, "title", index)
        path = _field(path_raw, "path", index)
        genre = _field(genre_raw, "genre", index)
        region = _field(region_raw, "region", index)
        if kind not in (1, 2, 3):
            raise ValueError(f"record {index}: unsupported media kind")
        _validate_path(path, index)
        stable_key = zlib.crc32(path.encode("utf-8")) & 0xFFFFFFFF
        canonical_path = unicodedata.normalize("NFC", path).casefold()
        if stable_key in identities or canonical_path in canonical_paths:
            raise ValueError(f"record {index}: duplicate or colliding game identity")
        identities.add(stable_key)
        canonical_paths.add(canonical_path)
        first = title[:1]
        letter = ord(first.upper()) if ("A" <= first <= "Z" or "a" <= first <= "z") else ord("#")
        genre_id = GENRES.get(genre, 20)
        region_id = REGIONS.get(region, 4)
        index_records.extend(INDEX_RECORD.pack(
            kind, genre_id, letter, region_id, stable_key))
        # Version 2 owns the seven formerly reserved bytes. The first 249
        # bytes remain byte-identical, including all original strings/kind.
        metadata_start = index * RECORD.size + 249
        enriched_records[metadata_start:metadata_start + 7] = struct.pack(
            "<IBBB", stable_key, genre_id, region_id, letter)

    signature = zlib.crc32(enriched_records) & 0xFFFFFFFF
    full = PACK_HEADER.pack(b"TPCE", 2, count, signature, 0) + bytes(enriched_records)
    compact = PACK_HEADER.pack(b"TPIX", 1, count, signature, 0) + index_records
    return full, compact


def _write_pack(directory, full, compact):
    directory.mkdir(parents=True, exist_ok=True)
    staged = []
    try:
        for name, contents in (("catalog2.bin", full), ("index.bin", compact)):
            with tempfile.NamedTemporaryFile(dir=directory, prefix=f".{name}.",
                                             suffix=".tmp", delete=False) as output:
                temporary = Path(output.name)
                staged.append((temporary, directory / name))
                output.write(contents)
                output.flush()
                os.fsync(output.fileno())
        for temporary, destination in staged:
            os.replace(temporary, destination)
    finally:
        for temporary, _ in staged:
            temporary.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_catalog", type=Path)
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args(argv)
    # Do not allocate arbitrary host memory for a malformed or unrelated file.
    with args.input_catalog.open("rb") as source:
        data = source.read(MAX_SOURCE_SIZE + 1)
    if len(data) > MAX_SOURCE_SIZE:
        raise ValueError(f"catalog exceeds console limit of {MAX_ENTRIES} entries")
    full, compact = build_pack(data)
    _write_pack(args.output_directory, full, compact)
    print(f"Built {PACK_HEADER.unpack_from(full)[2]} entries: "
          f"{len(full)} catalog bytes, {len(compact)} index bytes")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        import sys
        print(error, file=sys.stderr)
        raise SystemExit(1)
