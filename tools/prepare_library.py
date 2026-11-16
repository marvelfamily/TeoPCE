#!/usr/bin/env python3
"""Prepare a fresh TeoPCE card directory from a user's own PCE/CUE collection."""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import textwrap
import unicodedata
import zlib

sys.dont_write_bytecode = True
import build_catalog
import build_index
import build_find_index
import build_launch_metadata as launch
import build_game_metadata as years
import build_cheat_index as coverage
import build_cheat_groups as cheats
import descriptions
import library_media as media
import library_cheats
from library_sources import Metadata, get_art, placeholder_art

ROOT = Path(__file__).resolve().parents[1]
ROM_SHA256 = "cb154a413102a2d56fd7695cb0735c85b90551e8ddf8d4814eeeef12683ca5ef"
DATA_NAMES = {"catalog2.bin", "index.bin", "launch.bin", "find.bin", "years.bin", "cheats.idx"}


def ascii_text(value, limit=None):
    value = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode()
    value = " ".join("".join(c if 32 <= ord(c) <= 126 else " " for c in value).split())
    return value[:limit] if limit else value


def load_json(path):
    return json.loads(media.read_file(path, 16 * 1024 * 1024))


def catalog_at(card):
    return media.read_file(card / "00TEOPCE/data/catalog2.bin", 16 + 1200 * 256)


def launch_rows(card, catalog):
    rows, generation = launch.parse_catalog(catalog)
    raw = media.read_file(card / "00TEOPCE/data/launch.bin", 16 + 1200 * 12)
    identities = coverage.groups._launch_rows(raw, rows, generation)
    return rows, identities


def check_previous(previous, games, fingerprints):
    catalog = catalog_at(previous)
    rows, identities = launch_rows(previous, catalog)
    by_path = {g["path"]: g for g in games}
    old_files = fingerprints
    state_path = previous / "00TEOPCE/data/library-state.json"
    old_state = load_json(state_path) if state_path.is_file() else None
    for transaction in ["00TEOPCE/user/cc/txn.bin", "00TEOPCE/user/cdtxn.bin"]:
        if (previous / transaction).exists():
            raise ValueError("Resolve the pending cheat transaction on the console before rebuilding")
    for row in rows:
        game = by_path.get(row["path"])
        if game is None or row["kind"] != game["kind"]:
            raise ValueError("Previous game identity was removed or renamed; explicit migration required: " + row["path"])
        old = media.inspect_game(previous, row["path"], old_files)
        expected_content = old_state["games"].get(row["path"]) if old_state else old["content_identity"]
        if game["content_identity"] != expected_content:
            raise ValueError("Previous game content identity changed; explicit migration required: " + row["path"])
        if row["kind"] != 3 and identities[row["path"]] != (game["fingerprint"]["size"], game["fingerprint"]["crc32"]):
            raise ValueError("Previous launch identity does not match the preserved game")
    return catalog, rows


class Reference:
    def __init__(self, card):
        self.card = card
        self.rows = []
        self.identities = {}
        self.fingerprints = {}
        self.art = {}
        self.details = {}
        if card is not None:
            state_path = card / "00TEOPCE/data/library-state.json"
            if state_path.is_file():
                self.details = load_json(state_path).get("details", {})
                if not isinstance(self.details, dict) or any(
                        not isinstance(fields, dict) or any(k not in ("description", "developer", "publisher")
                        or not isinstance(v, str) for k, v in fields.items()) for fields in self.details.values()):
                    raise ValueError("Invalid saved game details")
            ledger_path = card / "00TEOPCE/data/ART-PROVENANCE.json"
            if ledger_path.is_file():
                ledger = load_json(ledger_path)
                if not isinstance(ledger, list):
                    raise ValueError("Invalid reference artwork ledger")
                self.art = {(item["index"], item["image"]): item for item in ledger}
            catalog = catalog_at(card)
            self.rows, self.identities = launch_rows(card, catalog)
            self.categories = {}
            find_path = card / "00TEOPCE/data/find.bin"
            if find_path.is_file():
                find = media.read_file(find_path, 16 + 1200 * 20)
                if len(find) != 16 + len(self.rows) * 20:
                    raise ValueError("Invalid reference search index")
                if find[4] == 2:
                    self.categories = {r["path"]: find[16 + r["index"] * 20] >> 2 for r in self.rows}
                    if build_find_index.build_find(catalog, category_by_path=self.categories) != find:
                        raise ValueError("Invalid reference search index")
                elif build_find_index.build_find(catalog) != find:
                    raise ValueError("Invalid reference search index")
            self.years = [0] * len(self.rows)
            path = card / "00TEOPCE/data/years.bin"
            if path.is_file():
                self.years = years.parse_year_pack(media.read_file(path, 16 + 1200 * 8), catalog)

    def match(self, game):
        fp = game["fingerprint"]
        candidates = [r for r in self.rows if r["kind"] == game["kind"]
                      and (r["kind"] == 3 or self.identities[r["path"]] == (fp["size"], fp["crc32"]))
                      and (r["kind"] != 2 or r["path"] == game["path"])]
        matches = []
        for row in candidates:
            try:
                old = media.inspect_game(self.card, row["path"], self.fingerprints)
            except (OSError, ValueError):
                continue
            if old["content_identity"] == game["content_identity"]:
                matches.append(row)
        same_path = [r for r in matches if r["path"] == game["path"]]
        if len(same_path) == 1:
            return same_path[0]
        return matches[0] if len(matches) == 1 else None

    def package(self, row, key, game):
        if row is None:
            return None, "No exact reference pack"
        if key != row["key"]:
            return None, "Reference path differs; exact-path cheat policies retained"
        path = self.card / "00TEOPCE/cheats" / ("%08X.tcg" % key)
        if not path.is_file():
            return None, "No exact reference pack"
        raw = media.read_file(path, cheats.MAX_FILE)
        cheats.decode_package(raw, key, game["fingerprint"]["size"], game["fingerprint"]["crc32"])
        return raw, "Exact path and full game content verified"


def ordered_catalog(games, metadata, previous_catalog, edited_paths=()):
    entries = []
    for game in games:
        meta = metadata[game["path"]]
        title = ascii_text(meta["title"], 87)
        if not title:
            title = "Game %08X" % zlib.crc32(game["path"].encode("ascii"))
        genre = meta["genre"] if meta["genre"] in build_index.GENRES else "Unknown"
        region = ascii_text(meta["region"], 11) or "Unknown"
        entries.append({"title": title, "launch_path": game["path"], "genre": genre, "region": region})
    v1 = build_catalog.make_records(entries, {})
    if previous_catalog is None:
        return build_index.build_pack(v1)
    old_rows, _ = launch.parse_catalog(previous_catalog)
    old_paths = {r["path"] for r in old_rows}
    fresh, _ = build_index.build_pack(v1)
    fresh_rows, _ = launch.parse_catalog(fresh)
    fresh_by_path = {r["path"]: r for r in fresh_rows}
    payload = b"".join(fresh_by_path[r["path"]]["raw"] if r["path"] in edited_paths else r["raw"] for r in old_rows)
    payload += b"".join(r["raw"] for r in fresh_rows if r["path"] not in old_paths)
    return build_index.build_pack(struct.pack("<4sHH", b"TPCE", 1, len(games)) + payload)


def description(meta, row):
    parts = [meta.get("description", "")]
    for label, key in [("Developer", "developer"), ("Publisher", "publisher")]:
        if meta.get(key):
            parts.append(label + " - " + meta[key])
    if not any(parts):
        parts = [row["title"], "HuCard" if row["kind"] == 1 else "CD-ROM" if row["kind"] == 2 else "SuperGrafx"]
        if row["genre"] != "Unknown":
            parts.append(row["genre"])
        if row["region"]:
            parts.append(row["region"])
        parts.append("Description not available.")
    value = descriptions.format_text("\n\n".join(p for p in parts if p))
    raw = value.encode("ascii")
    if len(raw) > 8192:
        raise ValueError("Formatted description exceeds 8192 bytes")
    return raw


def copy_tree_checked(source, destination):
    if not source.exists():
        return
    for path in sorted(source.rglob("*")):
        if path.is_symlink():
            raise ValueError("Symbolic links are unsupported in previous application data")
        if path.is_file() and not path.name.startswith("."):
            media.copy_verified(path, destination / path.relative_to(source), media.fingerprint(path))


def verify_result(card):
    data = card / "00TEOPCE/data"
    catalog = media.read_file(data / "catalog2.bin", 16 + 1200 * 256)
    rows, generation = launch.parse_catalog(catalog)
    packed, index = build_index.build_pack(struct.pack("<4sHH", b"TPCE", 1, len(rows)) + catalog[16:])
    if packed != catalog or media.read_file(data / "index.bin", 16 + 1200 * 8) != index:
        raise ValueError("Catalog/index verification failed")
    launch_raw = media.read_file(data / "launch.bin", 16 + 1200 * 12)
    identities = coverage.groups._launch_rows(launch_raw, rows, generation)
    year_values = years.parse_year_pack(media.read_file(data / "years.bin", 16 + 1200 * 8), catalog)
    cheat_raw = media.read_file(data / "cheats.idx", 16 + 1200 * 8)
    coverage.parse_index(cheat_raw, catalog)
    packages = {p.name: media.read_file(p, cheats.MAX_FILE) for p in (card / "00TEOPCE/cheats").glob("*.tcg")}
    if coverage.build_index(catalog, launch_raw, packages) != cheat_raw:
        raise ValueError("Cheat coverage verification failed")
    find = media.read_file(data / "find.bin", 16 + 1200 * 20)
    categories = {r["path"]: find[16 + r["index"] * 20] >> 2 for r in rows}
    if build_find_index.build_find(catalog, category_by_path=categories) != find:
        raise ValueError("Search index verification failed")
    return rows, identities, year_values


def prepare(args, progress=None):
    progress = progress or (lambda message: print(message, flush=True))
    library = args.library.resolve(strict=True)
    output = args.output.parent.resolve() / args.output.name
    if output.exists() or output.is_symlink():
        raise ValueError("Choose a new output directory; existing output is retained")
    if library == output or library in output.parents or output in library.parents:
        raise ValueError("Output and source library directories must be separate")
    for parent in [output.parent, *output.parent.parents]:
        if parent.is_symlink():
            raise ValueError("Output parent must not use symbolic links")
    previous = args.previous.resolve(strict=True) if args.previous else None
    if previous is None and (library / "00TEOPCE/data/catalog2.bin").is_file():
        previous = library
    cache = args.cache.resolve()
    protected = [library, output] + ([previous] if previous else [])
    if args.reference:
        protected.append(args.reference.resolve(strict=True))
    if any(cache == p or p in cache.parents for p in protected):
        raise ValueError("Cache must be outside source, previous, reference and output directories")
    source_rom = args.rom or ROOT / "resources/TeoPCE.pce"
    if args.rom is None and not source_rom.is_file():
        source_rom = ROOT / "build/cd-launch-missing-profile-20261112/rom/TeoPCE.pce"
    rom = media.read_file(source_rom, 131072)
    if hashlib.sha256(rom).hexdigest() != ROM_SHA256:
        raise ValueError("Use the verified TeoPCE 0.9.13 ROM")
    overrides = {}
    metadata_root = library
    if args.metadata:
        metadata_root = args.metadata.absolute().parent
        document = load_json(args.metadata)
        if document.get("version") != 1 or not isinstance(document.get("games"), dict):
            raise ValueError("Expected metadata version 1 with a games mapping")
        overrides = document["games"]
        if any(not isinstance(p, str) or not isinstance(m, dict) for p, m in overrides.items()):
            raise ValueError("Each metadata game must map a path to an object")
    output.parent.mkdir(parents=True, exist_ok=True)
    lock = output.parent / ("." + output.name + ".teopce-lock")
    # Exclusive creation prevents two builders from publishing the same output.
    with lock.open("x"):
        pass
    try:
        progress("Reading games · checking files on your card")
        games, fingerprints, skipped = media.scan(library, progress=progress)
        paths = {g["path"] for g in games}
        if set(overrides) - paths:
            raise ValueError("Metadata names games that did not pass scanning")
        previous_catalog = None
        previous_app_files = None
        if previous == library:
            previous_app_files = {p.relative_to(previous).as_posix(): media.fingerprint(p)["sha256"]
                                  for p in (previous / "00TEOPCE").rglob("*") if p.is_file() and not p.name.startswith(".")}
            previous_app_files["TeoPCE.pce"] = media.fingerprint(previous / "TeoPCE.pce")["sha256"]
        if previous:
            previous_catalog, _ = check_previous(previous, games, fingerprints if previous == library else {})
        reference = Reference(args.reference.resolve(strict=True) if args.reference else previous)
        if reference.card == library:
            reference.fingerprints = fingerprints
        database_paths = []
        bundled_cheats = ROOT / "resources/cheats/library.json"
        if bundled_cheats.is_file():
            database_paths.append(bundled_cheats)
        cheat_library = library_cheats.Database(database_paths, preferred=getattr(args, "cheat_database", None))
        prior_cheat_sources = {}
        if reference.card:
            source_ledger = reference.card / "00TEOPCE/data/CHEAT-SOURCES.json"
            if source_ledger.is_file():
                ledger = json.loads(media.read_file(source_ledger, 16 * 1024 * 1024))
                cheats.require(type(ledger) is dict and ledger.get("version") == 1
                               and type(ledger.get("packages")) is dict, "Invalid cheat attribution ledger")
                prior_cheat_sources = ledger["packages"]
        progress("Matching game details · bundled and cached Libretro database")
        public = Metadata({g["kind"] for g in games}, cache, args.offline, overrides, progress=progress)
        metadata = {}
        matches = {}
        for game in games:
            meta = public.resolve(game, fingerprints)
            old = reference.match(game)
            matches[game["path"]] = old
            if old:
                inherited = {"title": old["title"], "genre": old["genre"], "region": old["region"],
                             "year": reference.years[old["index"]]}
                inherited.update(reference.details.get(old["path"], {}))
                if old["path"] in reference.categories:
                    inherited["category"] = reference.categories[old["path"]]
                meta.update({k: v for k, v in inherited.items() if k not in overrides.get(game["path"], {})})
                meta["match"] = "full content reference"
            metadata[game["path"]] = meta
        full, index = ordered_catalog(games, metadata, previous_catalog, overrides)
        rows, generation = launch.parse_catalog(full)
        by_path = {g["path"]: g for g in games}
        identities = {r["index"]: (by_path[r["path"]]["fingerprint"]["size"],
                                   by_path[r["path"]]["fingerprint"]["crc32"])
                      for r in rows if launch.launchable(r)}
        launch_data = launch.encode_launch(full, identities)
        categories = {p: m["category"] for p, m in metadata.items()}
        find = build_find_index.build_find(full, category_by_path=categories)
        with tempfile.TemporaryDirectory(dir=output.parent, prefix=".teopce-build-") as td:
            stage = Path(td) / "result"
            card = stage / "card"
            data = card / "00TEOPCE/data"
            data.mkdir(parents=True)
            packs = card / "00TEOPCE/cheats"
            packs.mkdir()
            (card / "00TEOPCE/user/cc").mkdir(parents=True)
            (card / "00TEOPCE/user/cd").mkdir()
            (card / "TeoPCE.pce").write_bytes(rom)
            source_names = sorted({name for game in games for name in game["files"]})
            copy_names = [] if getattr(args, "data_only", False) else source_names
            for number, name in enumerate(copy_names, 1):
                media.copy_verified(library / name, card / name, fingerprints[name])
                progress("Copied source file %d/%d" % (number, len(copy_names)))
            if previous:
                # Namespaces and indices stay stable. Transaction journals were rejected above.
                copy_tree_checked(previous / "00TEOPCE/user", card / "00TEOPCE/user")
                copy_tree_checked(previous / "00TEOPCE/cheats", packs)
                copy_tree_checked(previous / "edturbo", card / "edturbo")
            package_bytes = {}
            cheat_ledger = {}
            art_ledger = []
            entries_report = []
            year_values = []
            details_state = {}
            for number, row in enumerate(rows, 1):
                progress("Preparing game %d/%d · %s" % (number, len(rows), row["title"]))
                game = by_path[row["path"]]
                meta = metadata[row["path"]]
                old = matches[row["path"]]
                year_values.append(meta["year"])
                desc = None
                edits = overrides.get(row["path"], {})
                if old and "description" not in edits:
                    path = reference.card / "00TEOPCE/data/desc" / ("%06d.txt" % old["index"])
                    if path.is_file():
                        desc = media.read_file(path, 8192)
                        years._validate_description(desc, row["index"])
                        if {"developer", "publisher"} & edits.keys():
                            if "description" not in reference.details.get(old["path"], {}):
                                raise ValueError("Include description when changing developer or publisher on a legacy card: " + row["path"])
                            desc = None  # Reformat the preserved prose with the updated facts.
                if desc is not None and "description" not in reference.details.get(old["path"], {}):
                    # Legacy formatted prose cannot be safely split from appended facts.
                    saved_details = None
                else:
                    saved_details = {k: meta[k] for k in ("description", "developer", "publisher") if k in meta}
                if saved_details is not None:
                    details_state[row["path"]] = saved_details
                (data / "desc").mkdir(exist_ok=True)
                (data / "desc" / ("%06d.txt" % row["index"])).write_bytes(desc if desc is not None else description(meta, row))
                for code in ["c", "s"]:
                    compiled = None
                    if old and not meta.get("cover" if code == "c" else "screen"):
                        path = reference.card / "00TEOPCE/data/art" / ("%06d-%s.bin" % (old["index"], code))
                        if path.is_file():
                            compiled = media.read_file(path, 3550)
                            digest = hashlib.sha256(compiled).hexdigest()
                            provenance = {"source": "owned exact-content reference", "sha256": digest}
                            prior_art = reference.art.get((old["index"], code))
                            if prior_art and prior_art.get("converted_sha256", prior_art.get("sha256")) == digest:
                                provenance = {k: v for k, v in prior_art.items() if k not in ("index", "image")}
                            if provenance["source"] == "owned exact-content reference" and compiled == placeholder_art(len(compiled) == 3488):
                                provenance = {"source": "placeholder", "converted_sha256": digest}
                            progress("Using artwork from your card · " + row["title"])
                    if compiled is None:
                        compiled, provenance = get_art(meta, game, code, metadata_root, cache, args.offline, progress=progress)
                    validate_art(compiled)
                    (data / "art").mkdir(exist_ok=True)
                    (data / "art" / ("%06d-%s.bin" % (row["index"], code))).write_bytes(compiled)
                    art_ledger.append({"index": row["index"], "image": code, **provenance})
                package, reason = reference.package(old, row["key"], game)
                cheat_sources = []
                if package is None:
                    package, reason, cheat_sources = cheat_library.package(row, game)
                name = "%08X.tcg" % row["key"]
                preserved = packs / name
                if preserved.is_file():
                    prior = media.read_file(preserved, cheats.MAX_FILE)
                    if package is None or package != prior:
                        raise ValueError("Existing cheat package identity changed; explicit migration required")
                if package is not None:
                    preserved.write_bytes(package)
                    package_bytes[name] = package
                decoded = cheats.decode_package(package, row["key"], game["fingerprint"]["size"], game["fingerprint"]["crc32"]) if package else []
                if package is not None:
                    digest = hashlib.sha256(package).hexdigest()
                    if cheat_sources:
                        cheat_ledger[name] = {"sha256": digest, "sources": cheat_sources,
                                              "libraries": cheat_library.sources}
                    elif old:
                        old_name = "%08X.tcg" % old["key"]
                        attribution = prior_cheat_sources.get(old_name)
                        if attribution is not None:
                            original = media.read_file(reference.card / "00TEOPCE/cheats" / old_name, cheats.MAX_FILE)
                            cheats.require(type(attribution) is dict and attribution.get("sha256") == hashlib.sha256(original).hexdigest()
                                           and type(attribution.get("sources")) is list, "Cheat attribution checksum mismatch")
                            cheat_ledger[name] = {**attribution, "sha256": digest}
                            cheat_sources = attribution["sources"]
                entries_report.append({"index": row["index"], "path": row["path"], "title": row["title"],
                                       "key": "%08X" % row["key"], "match": meta["match"],
                                       "category": meta["category"], "year": meta["year"],
                                       "editable_description": saved_details.get("description", "") if saved_details is not None else desc.decode("ascii"),
                                       "art": {p["image"]: {k: v for k, v in p.items() if k not in ("index", "image")} for p in art_ledger[-2:]},
                                       "description": ("facts only" if b"Description not available." in desc else "reference") if desc is not None else "user" if meta.get("description") else "facts only",
                                       "cheats": reason, "cheat_groups": sum(not g["reason"] for g in decoded),
                                       "cheat_held": sum(bool(g["reason"]) for g in decoded),
                                       "cheat_sources": cheat_sources, "content_identity": game["content_identity"]})
            payloads = {"catalog2.bin": full, "index.bin": index, "launch.bin": launch_data,
                        "find.bin": find, "years.bin": years.build_year_pack(full, year_values),
                        "cheats.idx": coverage.build_index(full, launch_data, package_bytes)}
            for name, raw in payloads.items():
                (data / name).write_bytes(raw)
            state = {"version": 1, "details": details_state, "previous_app_files": previous_app_files, "files": {name: fingerprints[name] for name in source_names},
                     "games": {g["path"]: g["content_identity"] for g in games}}
            (data / "library-state.json").write_text(json.dumps(state, indent=2) + "\n")
            (data / "ART-PROVENANCE.json").write_text(json.dumps(art_ledger, indent=2) + "\n")
            (data / "CHEAT-SOURCES.json").write_text(json.dumps({"version": 1, "packages": cheat_ledger}, indent=2) + "\n")
            (data / "METADATA-SOURCES.json").write_text(json.dumps(public.sources, indent=2) + "\n")
            license_root = ROOT / "resources/cheats"
            license_text = media.read_file(license_root / "LICENSE-CC-BY-SA-4.0.txt", 64 * 1024)
            source_manifest = json.loads(media.read_file(license_root / "SOURCES.json", 2 * 1024 * 1024))
            cheats.require(hashlib.sha256(license_text).hexdigest() == source_manifest.get("license_sha256"),
                           "Bundled data license checksum mismatch")
            (data / "CC-BY-SA-4.0.txt").write_bytes(license_text)
            progress("Verifying catalog · artwork, game links and cheat packages")
            verify_result(card)
            report = {"version": 1, "rom_version": "0.9.13", "games": len(rows),
                      "visible_games": sum(r["kind"] != 3 for r in rows), "catalog_generation": "%08X" % generation,
                      "skipped": skipped, "warnings": public.warnings, "entries": entries_report,
                      "art": dict(Counter(p["source"] for p in art_ledger)), "sources": public.sources,
                      "cheat_sources": cheat_library.sources, "cheat_packages": len(package_bytes), "hardware_tested": False,
                      "previous_user_state_preserved": previous is not None}
            (stage / "REPORT.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
            (stage / "README.txt").write_text(INSTALL_TEXT, encoding="utf-8")
            checksums = []
            for path in sorted(card.rglob("*")):
                if path.is_file():
                    checksums.append(media.fingerprint(path)["sha256"] + "  " + path.relative_to(stage).as_posix())
            (stage / "SHA256SUMS.txt").write_text("\n".join(checksums) + "\n")
            if output.exists():
                raise ValueError("Output appeared during preparation")
            os.rename(stage, output)
        return report
    finally:
        lock.unlink(missing_ok=True)


def validate_art(raw):
    if len(raw) == 3488:
        start, colors = 0, 16
    elif len(raw) == 3550 and struct.unpack_from("<4sBBBBHHI", raw) == (b"TPA2", 2, 96, 72, 2, 108, 14, 0):
        start, colors = 16, 32
        if raw[93] & 0xf0:
            raise ValueError("Invalid artwork tile mask")
    else:
        raise ValueError("Invalid converted artwork format")
    if any(c > 511 for c in struct.unpack_from("<%dH" % colors, raw, start)):
        raise ValueError("Artwork palette exceeds RGB333")


INSTALL_TEXT = """TeoPCE 0.9.13 prepared library

Read REPORT.json for skipped games and missing metadata.
Copy the contents of card/ to a separate prepared SD, preserving paths.
The card needs the compatible Krikzz menu and your own working CD BIOS.
Keep a backup of the destination card, especially edturbo and 00TEOPCE/user.
For an existing TeoPCE card, use --previous with that card to preserve state.
Apply a prepared generation as a complete set. Do not mix index files from older builds.
Safely eject the SD. Start TeoPCE.pce from the Krikzz menu.
SuperGrafx entries are hidden on CoreGrafx.
Source games and current card have not been edited by preparation.
"""


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--previous", type=Path, help="Previous complete prepared card, with games and user state")
    parser.add_argument("--reference", type=Path, help="Owned prepared card for exact-content art/descriptions/cheats")
    parser.add_argument("--cheat-database", type=Path, help="Additional version-1 content-bound cheat library JSON")
    parser.add_argument("--metadata", type=Path, help="Optional version-1 JSON with a games mapping keyed by relative path")
    parser.add_argument("--rom", type=Path, help="Verified TeoPCE 0.9.13 ROM")
    parser.add_argument("--cache", type=Path, default=Path.home() / ".cache/teopce-library")
    parser.add_argument("--data-only", action="store_true", help="Prepare application data for games already on the card")
    parser.add_argument("--offline", action="store_true", help="Use cached data and local images only")
    args = parser.parse_args(argv)
    if args.library is None:
        args.library = Path(input("Folder containing your games: ").strip().strip('"'))
    if args.output is None:
        args.output = Path(input("New output folder: ").strip().strip('"'))
    try:
        report = prepare(args)
    except (OSError, ValueError, UnicodeError, KeyError) as error:
        print("Preparation stopped - " + str(error), file=sys.stderr)
        return 1
    print("Prepared %d games, skipped %d. Output: %s" %
          (report["games"], len(report["skipped"]), args.output), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
