#!/usr/bin/env python3
"""Compile the verified PC Engine library inventory into a fixed-record index."""
import json
import os
from pathlib import Path, PurePosixPath
import re
import struct
import sys
import tempfile

HEADER = struct.Struct("<4sHH")
RECORD = struct.Struct("<88s128s20s12sB7x")


def parse_genres(text):
    matches = {}
    for block in re.findall(r"(?ms)^game\s*\(\s*(.*?)^\)", text):
        genre = re.search(r'(?m)^\s*genre\s+"([^"]+)"', block)
        crc = re.search(r"\bcrc\s+([0-9a-fA-F]{8})\b", block)
        if not genre or not crc:
            continue
        key = crc.group(1).lower()
        value = genre.group(1)
        if value.strip().casefold() in ("n/a", "unknown"):
            continue
        if key in matches and matches[key] != value:
            matches[key] = "Unknown"
        else:
            matches[key] = value
    return matches


def field(value, width, name):
    if not isinstance(value, str) or "\0" in value:
        raise ValueError(f"invalid {name}")
    encoded = value.encode("utf-8")
    if len(encoded) >= width:
        raise ValueError(f"{name} too long ({len(encoded)} bytes): {value}")
    return encoded


def make_records(entries, genres):
    if len(entries) > 1200:
        raise ValueError("too many entries")
    encoded = []
    seen_paths = set()
    for entry in entries:
        path = entry["launch_path"]
        if not isinstance(path, str) or not path or "\\" in path or "\0" in path:
            raise ValueError(f"unsafe path: {path!r}")
        parts = PurePosixPath(path).parts
        if path.startswith("/") or any(p in (".", "..") for p in path.split("/")):
            raise ValueError(f"unsafe path: {path!r}")
        suffix = PurePosixPath(path).suffix.lower()
        if suffix not in (".pce", ".sgx", ".cue"):
            raise ValueError(f"unsupported path: {path}")
        key = path.casefold()
        if key in seen_paths:
            raise ValueError(f"duplicate path: {path}")
        seen_paths.add(key)
        kind = 3 if suffix == ".sgx" or "supergrafx" in parts[0].casefold() else 2 if suffix == ".cue" else 1
        crc = str(entry.get("original_crc32", "")).lower()
        genre = genres.get(crc, entry.get("genre") or "Unknown")
        record = RECORD.pack(
            field(entry["title"], 88, "title"),
            field(path, 128, "path"),
            field(genre, 20, "genre"),
            field(entry.get("region", ""), 12, "region"),
            kind,
        )
        encoded.append((entry["title"].casefold(), path.casefold(), record))
    encoded.sort(key=lambda row: (row[0], row[1]))
    return HEADER.pack(b"TPCE", 1, len(encoded)) + b"".join(row[2] for row in encoded)


def main():
    if len(sys.argv) != 4:
        raise ValueError("usage: build_catalog.py catalog.json genres.dat output.bin")
    source, genre_file, destination = map(Path, sys.argv[1:])
    entries = json.loads(source.read_text(encoding="utf-8"))["entries"]
    data = make_records(entries, parse_genres(genre_file.read_text(encoding="utf-8")))
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as tmp:
        temporary = Path(tmp.name)
        try:
            tmp.write(data)
            tmp.flush()
            os.fsync(tmp.fileno())
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
    try:
        os.replace(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"Built {len(entries)} catalog entries ({len(data)} bytes): {destination}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
