#!/usr/bin/env python3
"""Reconstruct bound cheat groups from pinned, attributed Libretro sources."""
import argparse
import configparser
from collections import defaultdict
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import unicodedata
from urllib.parse import quote

import build_cheat_groups as codec
import library_cheats
import library_media as media

REVISION = "969e31a324119a17add184e8b716315a74adb51b"
BASE_URL = "https://raw.githubusercontent.com/libretro/libretro-database/" + REVISION + "/"
FAMILIES = {"NEC - PC Engine - TurboGrafx 16": 1,
            "NEC - PC Engine CD - TurboGrafx-CD": 2}


def parse_cheats(raw):
    config = configparser.ConfigParser(interpolation=None)
    try:
        config.read_string("[cheats]\n" + raw.decode("utf-8-sig"))
        fields = {key: json.loads(value) if value.startswith('"') else value
                  for key, value in config["cheats"].items()}
        count = int(fields.get("cheats", "0"))
        codec.require(0 <= count <= 1024, "Libretro cheat count")
    except (UnicodeError, configparser.Error, ValueError) as error:
        raise ValueError("Invalid Libretro cheat file") from error
    groups = []
    for index in range(count):
        prefix = "cheat%d_" % index
        name = fields.get(prefix + "desc", "")
        code = fields.get(prefix + "code", "")
        if not isinstance(name, str) or not name or not isinstance(code, str):
            continue
        if fields.get(prefix + "handler", "0") == "0" and re.fullmatch(
                r"[a-fA-F0-9]{1,6}:[a-fA-F0-9]{1,2}(?:\+[a-fA-F0-9]{1,6}:[a-fA-F0-9]{1,2}){0,15}", code):
            codes = [(int(address, 16), int(value, 16))
                     for address, value in (part.split(":") for part in code.split("+"))]
        elif all(fields.get(prefix + key) == value for key, value in
                 [("handler", "1"), ("cheat_type", "1"), ("memory_search_size", "3")]):
            try:
                address, value = int(fields[prefix + "address"]), int(fields[prefix + "value"])
                repeat = int(fields.get(prefix + "repeat_count", "1"))
            except (KeyError, ValueError, TypeError):
                continue
            if not (0 <= address < 8192 and 0 <= value <= 255 and repeat <= 1):
                continue
            codes = [(0x1F0000 + address, value)]
        else:
            continue
        groups.append({"name": codec.display_name(name), "codes": codes})
    return groups


def title_key(title):
    title = re.sub(r" \((?:En|Unl|Rumbles|RetroArch|Rev \d+|V\d+(?:\.\d+)*)\)", "", title, flags=re.I)
    title = unicodedata.normalize("NFKD", title).encode("ascii", "ignore").decode().lower()
    return re.sub("[^a-z0-9]", "", title)


def signature(codes):
    return tuple((address + 0xD92000 if 0x1F0000 <= address <= 0x1F1FFF else address, value)
                 for address, value, *_ in codes)


def convert(reference, upstream, manifest):
    codec.require(reference.get("version") == 1 and isinstance(reference.get("entries"), list),
                  "Expected reference library version 1")
    codec.require(manifest.get("revision") == REVISION and manifest.get("license") == "CC-BY-SA-4.0",
                  "Expected pinned CC-BY-SA-4.0 source manifest")
    license_text = media.read_file(Path(upstream) / "LICENSE", 64 * 1024)
    codec.require(hashlib.sha256(license_text).hexdigest() == manifest.get("license_sha256"),
                  "Libretro license checksum mismatch")
    source_files = manifest.get("files")
    codec.require(isinstance(source_files, list) and len(source_files) <= 1000, "Source manifest file count")
    index = defaultdict(list)
    seen = set()
    invalid_sources = []
    for item in source_files:
        relative = item["path"]
        codec.require(type(relative) is str and "\\" not in relative, "Invalid source path")
        path = PurePosixPath(relative)
        codec.require(not path.is_absolute() and str(path) == relative and ".." not in path.parts, "Invalid source path")
        parts = path.parts
        codec.require(len(parts) == 3 and parts[0] == "cht" and parts[1] in FAMILIES
                      and parts[2].endswith(".cht") and relative not in seen, "Unsupported source path")
        seen.add(relative)
        url = BASE_URL + quote(relative)
        codec.require(item["url"] == url, "Source URL differs from pinned revision")
        source_path = Path(upstream) / relative
        codec.require(source_path.resolve().is_relative_to(Path(upstream).resolve()), "Source path escapes upstream directory")
        raw = media.read_file(source_path, 256 * 1024)
        codec.require(hashlib.sha256(raw).hexdigest() == item["sha256"], "Libretro source checksum mismatch")
        blob = b"blob " + str(len(raw)).encode() + b"\0" + raw
        codec.require(hashlib.sha1(blob).hexdigest() == item.get("git_blob"), "Libretro Git blob checksum mismatch")
        try:
            groups = parse_cheats(raw)
        except ValueError as error:
            invalid_sources.append({"path": relative, "reason": str(error)})
            continue
        key = FAMILIES[parts[1]], title_key(Path(parts[2]).stem)
        index[key].extend({**group, "url": url, "path": relative} for group in groups)
    entries, excluded, used = [], [], set()
    validator = library_cheats.Database([])
    for entry in reference["entries"]:
        validator.add(entry)
        candidates = index[entry["kind"], title_key(entry["title"])]
        groups, sources = [], set()
        for group in entry["groups"]:
            match = next((source for source in candidates
                          if signature(source["codes"]) == signature(group["codes"])), None)
            if match is None:
                excluded.append({"title": entry["title"], "group_id": group["id"],
                                 "reason": "No identical complete Libretro group for this game and region"})
                continue
            # Only proven identity, policy and address representation come from
            # the reference. Name and numeric writes have a complete source match.
            groups.append({**group, "name": match["name"]})
            sources.add(match["url"])
            used.add(match["path"])
        if groups:
            entries.append({**entry, "groups": groups, "sources": sorted(sources)})
    document = {"version": 1, "entries": entries,
                "hash_provenance": reference.get("hash_provenance", "Exact-content reference bindings"),
                "data_license": "CC-BY-SA-4.0",
                "attribution": {"project": "Libretro Database contributors", "revision": REVISION,
                                "source": "https://github.com/libretro/libretro-database",
                                "license": "https://creativecommons.org/licenses/by-sa/4.0/",
                                "changes": "Selected complete groups, normalized labels and RAM aliases, added TeoPCE content bindings, stable IDs and startup policies."}}
    report = {"entries": len(entries), "groups": sum(len(e["groups"]) for e in entries),
              "source_files": sorted(used), "excluded": excluded, "invalid_sources": invalid_sources}
    return document, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-library", required=True, type=Path)
    parser.add_argument("--upstream", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    reference = json.loads(media.read_file(args.reference_library, library_cheats.MAX_DATABASE))
    manifest = json.loads(media.read_file(args.manifest, library_cheats.MAX_DATABASE))
    document, report = convert(reference, args.upstream, manifest)
    codec.require(bool(document["entries"]), "No complete groups matched the source snapshot")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(document, stream, indent=2)
        stream.write("\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
