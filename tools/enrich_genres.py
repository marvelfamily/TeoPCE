#!/usr/bin/env python3
"""Enrich a copied catalog with verifiable genre data; never edit the source."""
import argparse
import json
from pathlib import Path, PurePosixPath
import re
import xml.etree.ElementTree as ET

from artwork import names_by_crc


def normalize(value):
    value = value.casefold()
    rules = [
        ("role playing", "Role-playing (RPG)"), ("role-playing", "Role-playing (RPG)"),
        ("shoot'em", "Shoot'em Up"), ("shoot em", "Shoot'em Up"),
        ("beat'em", "Beat'em Up"), ("pinball", "Pinball"),
        ("fight", "Fighting"), ("platform", "Platform"), ("puzzle", "Puzzle"),
        ("race", "Racing"), ("racing", "Racing"), ("driving", "Racing"),
        ("sports with animals", "Sports with Animals"), ("sport", "Sports"),
        ("strategy", "Strategy"), ("simulation", "Simulation"),
        ("adventure", "Adventure"), ("board", "Board"), ("gambling", "Gambling"),
        ("quiz", "Quiz"), ("lightgun", "Lightgun Shooter"), ("shooter", "Shooter"),
        ("action", "Action"),
    ]
    for term, normalized in rules:
        if term in value:
            return normalized
    return ""


def read_genres(text):
    root = ET.fromstring(text.lstrip("\ufeff"))
    result, ambiguous = {}, set()
    for game in root.findall("game"):
        path, genre = game.findtext("path", ""), normalize(game.findtext("genre", ""))
        if not path or not genre:
            continue
        key = re.sub(r" \((?:pcenginecd|pcengine|tg16cd|tg16)\)$", "",
                     PurePosixPath(path).stem, flags=re.IGNORECASE)
        if key in result and result[key] != genre:
            ambiguous.add(key)
        else:
            result[key] = genre
    for key in ambiguous:
        result.pop(key, None)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog", type=Path)
    parser.add_argument("genres", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--huc", type=Path, action="append", default=[])
    parser.add_argument("--cd", type=Path, action="append", default=[])
    args = parser.parse_args()
    catalog = json.loads(args.catalog.read_text(encoding="utf-8"))
    names = names_by_crc(args.genres.read_text(encoding="utf-8"))
    lookup = {"huc": {}, "cd": {}}
    for family, files in (("huc", args.huc), ("cd", args.cd)):
        for source in files:
            for name, genre in read_genres(source.read_text(encoding="utf-8-sig")).items():
                lookup[family].setdefault(name, (genre, source.name))
    count = 0
    for entry in catalog["entries"]:
        path = entry["launch_path"]
        if path.lower().endswith(".sgx") or path.lower().startswith("03 supergrafx/"):
            continue
        family = "cd" if path.lower().endswith(".cue") else "huc"
        crc = str(entry.get("original_crc32", "")).lower()
        possible = names.get(crc)
        if possible and len(possible) != 1:
            continue
        name = next(iter(possible)) if possible else entry["title"]
        found = lookup[family].get(name)
        if found:
            entry["genre"], entry["menu_genre_source"] = found
            count += 1
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(catalog, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Matched genre metadata for {count} entries in copied catalog")


if __name__ == "__main__":
    main()
