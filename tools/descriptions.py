#!/usr/bin/env python3
"""Prepare short descriptions using exact game identity and documented sources."""
import argparse
import html
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import textwrap
import unicodedata
import xml.etree.ElementTree as ET

from artwork import names_by_crc


def clean(text):
    text = html.unescape(text)
    text = text.translate(str.maketrans({"‘": "'", "’": "'", "“": '"', "”": '"',
                                        "–": " - ", "—": " - ", "…": "..."}))
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"\s+", " ", text).strip()


def parse_gamelist(text, conflicts=None):
    root = ET.fromstring(text.lstrip("\ufeff"))
    result = {}
    ambiguous = set()
    for game in root.findall("game"):
        path = game.findtext("path", "")
        description = clean(game.findtext("desc", ""))
        if not path or not description:
            continue
        key = re.sub(r" \((?:pcenginecd|pcengine|tg16cd|tg16)\)$", "",
                     PurePosixPath(path).stem, flags=re.IGNORECASE)
        if key in result and result[key] != description:
            ambiguous.add(key)
        else:
            result[key] = description
    for key in ambiguous:
        result.pop(key, None)
    if conflicts is not None:
        conflicts.update(ambiguous)
    return result



def family(entry):
    return "cd" if entry["launch_path"].lower().endswith(".cue") else "huc"


def is_supergrafx(entry):
    path = entry["launch_path"].lower()
    return path.endswith(".sgx") or path.startswith("03 supergrafx/")


def load_lookup(files):
    """Preserve exact-source priority, but flag conflicts for new fallbacks."""
    lookup, conflicts = {}, set()
    for source in files:
        parsed = parse_gamelist(source.read_text(encoding="utf-8-sig"), conflicts)
        for name, description in parsed.items():
            if name in lookup and lookup[name][0] != description:
                conflicts.add(name)
            lookup.setdefault(name, (description, source.name))
    return lookup, conflicts


def identity(entry, crc_names):
    possible = crc_names.get(str(entry.get("original_crc32", "")).lower())
    if possible and len(possible) != 1:
        return None, None
    return (next(iter(possible)), "crc") if possible else (entry["title"], "exact title")


def sourced_match(name, found, match, tier, evidence=None):
    description, source = found
    return {"description": description, "source_title": name, "source": source,
            "match": match, "tier": tier, "description_kind": "sourced synopsis",
            "source_text_sha256": hashlib.sha256(description.encode("ascii")).hexdigest(),
            "identity_evidence": evidence or {}}


def unique_title(title, lookup, conflicts):
    normalized = unicodedata.normalize("NFC", title).casefold()
    matches = [name for name in lookup
               if unicodedata.normalize("NFC", name).casefold() == normalized]
    if len(matches) != 1 or any(
            unicodedata.normalize("NFC", name).casefold() == normalized for name in conflicts):
        return None
    return matches[0]



def compatible_crc_title(alias, title):
    """A release alias may differ in tags, never in game name or region."""
    identities = []
    for name in (alias, title):
        match = re.match(r"^(.*?) \((Japan|USA|Europe|World)(?:, (Japan|USA|Europe|World))*\)",
                         name, flags=re.IGNORECASE)
        if not match:
            return False
        first_tag = name[match.start(2):name.find(")", match.start(2))]
        regions = set(first_tag.casefold().split(", "))
        identities.append((unicodedata.normalize("NFC", match[1]).casefold(), regions))
    return (identities[0][0] == identities[1][0] and
            bool(identities[0][1].intersection(identities[1][1])))


def original_sources(entry):
    return {record["source"] for record in entry.get("source_records", [])
            if isinstance(record.get("source"), str) and record["source"]}


def match_description(entry, entries, lookup, conflicts, crc_names):
    key, basis = identity(entry, crc_names)
    if key is None:
        return None
    if key in lookup:
        return sourced_match(key, lookup[key], basis, "baseline_exact_identity")

    name = unique_title(entry["title"], lookup, conflicts)
    if name is not None and (basis != "crc" or compatible_crc_title(key, entry["title"])):
        match = "exact title fallback" if name == entry["title"] else "casefold title fallback"
        return sourced_match(name, lookup[name], match, "catalog_title_fallback",
                             {"crc_identity_key": key})

    if entry.get("category") == "Translation":
        sources = original_sources(entry)
        parents = [parent for parent in entries
                   if parent.get("category", "").startswith("Original ")
                   and not is_supergrafx(parent) and family(parent) == family(entry)
                   and sources.intersection(original_sources(parent))]
        if len(parents) != 1:
            return None
        parent = parents[0]
        parent_key, parent_basis = identity(parent, crc_names)
        # Only the parent's existing exact match is approved; do not recurse
        # through aliases, translated-source suffixes or other ledgers.
        if parent_key is None or parent_key not in lookup:
            return None
        if unique_title(parent_key, lookup, conflicts) != parent_key:
            return None
        return sourced_match(parent_key, lookup[parent_key], "exact source parent",
                             "translation_source_parent",
                             {"parent_title": parent["title"],
                              "parent_launch_path": parent["launch_path"],
                              "parent_original_crc32": parent.get("original_crc32"),
                              "parent_identity_basis": parent_basis,
                              "shared_sources": sorted(sources.intersection(original_sources(parent)))})

    if entry.get("category", "").startswith("Original "):
        suffix = " (Translated En)"
        name = key + suffix
        if name not in lookup or unique_title(name, lookup, conflicts) != name:
            return None
        description = lookup[name][0]
        if re.search(r"\b(?:translat\w*|patch\w*|english|language\w*|hack\w*)\b",
                     description, flags=re.IGNORECASE):
            return None
        return sourced_match(name, lookup[name], "explicit translated source parent",
                             "explicit_translated_source_parent",
                             {"identity_key": key, "removed_source_suffix": suffix})
    return None


def format_text(text, width=38, lines=None):
    text = clean(text)
    wrapped = textwrap.wrap(text, width=width, break_long_words=True, break_on_hyphens=False)
    if lines is not None and len(wrapped) > lines:
        wrapped = wrapped[:lines]
        last = wrapped[-1]
        wrapped[-1] = last[:width - 3].rstrip() + "..."
    return "\n".join(wrapped)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog", type=Path)
    parser.add_argument("genres", type=Path)
    parser.add_argument("huc_xml", type=Path)
    parser.add_argument("cd_xml", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--huc-extra", type=Path, action="append", default=[])
    parser.add_argument("--cd-extra", type=Path, action="append", default=[])
    args = parser.parse_args()
    entries = json.loads(args.catalog.read_text(encoding="utf-8"))["entries"]
    entries.sort(key=lambda e: (e["title"].casefold(), e["launch_path"].casefold()))
    crc_names = names_by_crc(args.genres.read_text(encoding="utf-8"))
    lookup, conflicts = {}, {}
    for medium, files in (("huc", [args.huc_xml, *args.huc_extra]),
                          ("cd", [args.cd_xml, *args.cd_extra])):
        lookup[medium], conflicts[medium] = load_lookup(files)
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for index, entry in enumerate(entries):
        if is_supergrafx(entry):
            continue
        medium = family(entry)
        found = match_description(entry, entries, lookup[medium], conflicts[medium], crc_names)
        if not found:
            continue
        description = found.pop("description")
        (args.output / f"{index:06}.txt").write_text(format_text(description), encoding="ascii")
        manifest.append({"index": index, "title": entry["title"], **found})
    (args.output.parent / "description-manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Prepared {len(manifest)} descriptions for {len(entries)} records")


if __name__ == "__main__":
    main()
