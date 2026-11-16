#!/usr/bin/env python3
"""Export attributed cheat data from a prepared reference without copying games."""
import argparse
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path
import re
import zlib

import build_cheat_groups as codec
import build_launch_metadata as launch
import library_cheats
import library_media as media
import prepare_library


def export(reference, library, destination, verified_manifest=None, extra_provenance=()):
    catalog = prepare_library.catalog_at(reference)
    rows, identities = prepare_library.launch_rows(reference, catalog)
    # These are the exact keys used by the shipped runtime's guards and notes.
    policy_keys = set()
    for name in ["cheat_notes.c", "cd_guard_profiles.h", "cd_guard.c"]:
        text = (prepare_library.ROOT / "rom" / name).read_text()
        pattern = r'\{0x([A-Fa-f0-9]{8})UL' if name == "cheat_notes.c" else r'0x([A-Fa-f0-9]{8})UL'
        policy_keys.update(int(key, 16) for key in re.findall(pattern, text))
    manifest = {}
    if verified_manifest:
        document = json.loads(media.read_file(verified_manifest, 16 * 1024 * 1024))
        manifest = {f["path"]: f for f in document["files"]}
    sources = {}
    bindings = library / "90 Documentation/Cheat Bindings.csv"
    if bindings.is_file():
        with bindings.open(encoding="utf-8-sig", newline="") as stream:
            for row in csv.DictReader(stream):
                sources.setdefault(row["rom"], set()).add(row["source"])
    provenance = reference / "00TEOPCE/data/CD-CHEAT-PROVENANCE.json"
    if provenance.is_file():
        for row in json.loads(provenance.read_text())["prepared"]:
            sources.setdefault(row["path"], set()).add(row["source_url"])
    md5_source = reference / "00TEOPCE/data/MD5-CHEAT-ADDITION.json"
    if md5_source.is_file():
        document = json.loads(md5_source.read_text())
        for row in document["files"]:
            sources.setdefault(row["rom"], set()).add(document["source"]["url"])
    supplements = reference / "00TEOPCE/data/CHEAT-EXPANSION-0.9.1.json"
    if supplements.is_file():
        for row in json.loads(supplements.read_text())["groups"]:
            for source in row["sources"]:
                url = source.get("source_url")
                if not url and source.get("source_commit"):
                    url = "https://github.com/FrodeSolheim/cheats/tree/" + source["source_commit"]
                if url:
                    sources.setdefault(row["catalog_path"], set()).add(url)
    for path in extra_provenance:
        document = json.loads(media.read_file(path, 16 * 1024 * 1024))
        url = document["source"]["url"]
        for item in document["files"]:
            sources.setdefault(item["rom"], set()).add(url)
    result, excluded = {}, []
    fingerprints = {}
    reference_fingerprints = {}
    for row in rows:
        pack = reference / "00TEOPCE/cheats" / ("%08X.tcg" % row["key"])
        if not pack.is_file():
            continue
        try:
            size, crc = identities[row["path"]]
            groups = codec.decode_package(media.read_file(pack, codec.MAX_FILE), row["key"], size, crc)
            if row["kind"] == 2 and manifest:
                raw, tracks = media.cue_files(library, row["path"])
                if (len(raw), zlib.crc32(raw)) != (size, crc):
                    raise ValueError("CUE changed since this package was prepared")
                cue_pin = manifest[row["path"]]
                if len(raw) != cue_pin["size"] or hashlib.sha256(raw).hexdigest() != cue_pin["sha256"]:
                    raise ValueError("CUE differs from the verified reference snapshot")
                track_hashes = []
                for name in tracks:
                    item = manifest[name]
                    if not re.fullmatch(r"[a-f0-9]{64}", item["sha256"]) or (library / name).stat().st_size != item["size"]:
                        raise ValueError("Track manifest binding is incomplete")
                    track_hashes.append(bytes.fromhex(item["sha256"]))
                identity = hashlib.sha256(raw + b"\0" + b"".join(track_hashes)).hexdigest()
            else:
                game = media.inspect_game(library, row["path"], fingerprints)
                if (game["fingerprint"]["size"], game["fingerprint"]["crc32"]) != (size, crc):
                    raise ValueError("Game changed since this package was prepared")
                if manifest:
                    pin = manifest[row["path"]]
                    if (game["fingerprint"]["size"], game["fingerprint"]["sha256"]) != (pin["size"], pin["sha256"]):
                        raise ValueError("Game differs from the verified reference snapshot")
                else:
                    original = media.inspect_game(reference, row["path"], reference_fingerprints)
                    if original["content_identity"] != game["content_identity"]:
                        raise ValueError("Game content differs from the complete reference")
                identity = game["content_identity"]
            urls = sorted(sources.get(row["path"], []))
            if not urls:
                raise ValueError("Original source attribution unavailable")
            entry = {"title": row["title"], "kind": row["kind"], "path": row["path"],
                     "content_identity": identity, "size": size, "crc32": crc,
                     "requires_path": row["kind"] == 2 and row["key"] in policy_keys,
                     "groups": groups, "sources": urls}
            ident = row["kind"], identity
            old = result.get(ident)
            if old:
                if old["groups"] != groups or old["requires_path"] or entry["requires_path"]:
                    raise ValueError("Conflicting package for identical content")
                old["sources"] = sorted(set(old["sources"]) | set(urls))
            else:
                result[ident] = entry
        except (OSError, ValueError, KeyError) as error:
            excluded.append({"title": row["title"], "reason": str(error)})
    entries = sorted(result.values(), key=lambda e: (e["kind"], e["title"]))
    document = {"version": 1, "entries": entries,
                "hash_provenance": "Full game fingerprints from complete reference reads or the recorded verified reference snapshot"}
    raw = (json.dumps(document, indent=2) + "\n").encode()
    # JSON roundtrip converts tuples back to the public list representation.
    decoded = json.loads(raw)
    for entry in decoded["entries"]:
        library_cheats.Database([]).add(entry)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("xb") as stream:
        stream.write(raw)
    report = {"entries": len(entries), "media": dict(Counter(e["kind"] for e in entries)),
              "groups": sum(len(e["groups"]) for e in entries),
              "path_bound_cd": sum(e["requires_path"] for e in entries),
              "sha256": hashlib.sha256(raw).hexdigest(), "excluded": excluded}
    destination.with_suffix(".report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--verified-manifest", type=Path,
                        help="Previously verified file manifest supplying known CD track hashes")
    parser.add_argument("--provenance", type=Path, action="append", default=[])
    args = parser.parse_args()
    print(json.dumps(export(args.reference, args.library, args.output, args.verified_manifest, args.provenance)))
