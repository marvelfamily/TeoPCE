#!/usr/bin/env python3
"""Build a private, source-bound release-year/company metadata stage.

The stable catalog and existing descriptions are input-only. The output is a
new teo-menu4 directory plus a private provenance report beside it.
"""
from datetime import date
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import struct
import sys
import tempfile
import textwrap
import xml.etree.ElementTree as ET

TOOLS = Path(__file__).resolve().parent
PROJECT = TOOLS.parent
BUILD = PROJECT / "build"
PRIVATE_ROOT = BUILD / "daily-quality-v1" / "game-metadata"
DEFAULT_CATALOG = Path("catalog.json")
DEFAULT_STABLE_CARD = BUILD / "fw26-compat-v1" / "package-attempt1" / "card" / "teo-menu4"
XML_NAMES = (
    "descriptions-huc.xml",
    "descriptions-extra-pce.xml",
    "descriptions-extra-tg16.xml",
    "descriptions-cd.xml",
    "descriptions-extra-pcecd.xml",
    "descriptions-extra-tg16cd.xml",
)
HEADER = struct.Struct("<4sBBHII")
RECORD = struct.Struct("<IHH")
MAX_CATALOG_RECORDS = 1200
MAX_DESCRIPTION_BYTES = 8192
MAX_SOURCE_BYTES = 16 * 1024 * 1024
MAX_XML_BYTES = 8 * 1024 * 1024
MAX_GENRES_BYTES = 8 * 1024 * 1024
FIELDS = ("release_year", "developer", "publisher")
REGIONS = frozenset(("japan", "usa", "europe", "world"))

if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
import descriptions as description_tools  # noqa: E402


def _module(name, filename):
    spec = importlib.util.spec_from_file_location(name, TOOLS / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


catalog_tools = _module("game_metadata_catalog_tools", "build_launch_metadata.py")


def require(value, message):
    if not value:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def _year_value(year):
    return type(year) is int and (year == 0 or 1970 <= year <= 2099)


def build_year_pack(catalog: bytes, years: list[int]) -> bytes:
    """Encode TPY1 rows bound to the exact catalog generation and path keys."""
    require(type(catalog) is bytes, "catalog must be exact bytes")
    rows, generation = catalog_tools.parse_catalog(catalog)
    require(isinstance(years, list) and len(years) == len(rows),
            "release year count must match catalog")
    result = bytearray(HEADER.pack(b"TPY1", 1, RECORD.size, len(rows), generation, 0))
    for row, year in zip(rows, years):
        require(_year_value(year), "release year must be 0 or 1970..2099")
        result.extend(RECORD.pack(row["key"], year, 0))
    return bytes(result)


def parse_year_pack(data: bytes, catalog: bytes) -> list[int]:
    """Validate a TPY1 pack independently against its exact catalog."""
    require(type(data) is bytes and type(catalog) is bytes,
            "year pack and catalog must be exact bytes")
    rows, generation = catalog_tools.parse_catalog(catalog)
    require(len(data) >= HEADER.size, "year pack header is truncated")
    magic, version, record_bytes, count, signature, reserved = HEADER.unpack_from(data)
    require((magic, version, record_bytes, count, signature, reserved) ==
            (b"TPY1", 1, RECORD.size, len(rows), generation, 0),
            "year pack header/catalog mismatch")
    require(len(data) == HEADER.size + count * RECORD.size,
            "year pack length mismatch")
    years = []
    for index, row in enumerate(rows):
        key, year, row_reserved = RECORD.unpack_from(data, HEADER.size + index * RECORD.size)
        require(key == row["key"], "year pack path key mismatch")
        require(row_reserved == 0, "year pack row reserved field is nonzero")
        require(_year_value(year), "year pack release year outside allowed range")
        years.append(year)
    return years


def _check_no_symlink_components(path):
    candidate = Path(os.path.abspath(path))
    current = Path(candidate.anchor)
    for part in candidate.parts[1:]:
        current = current / part
        try:
            mode = os.lstat(current).st_mode
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(mode):
            # macOS exposes /tmp and /var as stable aliases for private system paths.
            allowed = {Path("/tmp"): Path("/private/tmp"),
                       Path("/var"): Path("/private/var")}
            require(current in allowed and Path(os.path.realpath(current)) == allowed[current],
                    "symlink path component is not allowed: " + str(current))


def read_regular_file(path, limit):
    """Read one bounded regular file without following a symlink or accepting drift."""
    path = Path(path)
    _check_no_symlink_components(path)
    try:
        before_lstat = os.lstat(path)
    except OSError:
        raise
    require(stat.S_ISREG(before_lstat.st_mode), "input is not a regular file: " + str(path))
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
    descriptor = os.open(path, flags)
    try:
        before = os.fstat(descriptor)
        require(stat.S_ISREG(before.st_mode) and
                (before.st_dev, before.st_ino) == (before_lstat.st_dev, before_lstat.st_ino),
                "input changed while opening: " + str(path))
        require(0 <= before.st_size <= limit, "input exceeds byte bound: " + str(path))
        chunks = []
        total = 0
        while True:
            chunk = os.read(descriptor, min(1024 * 1024, limit + 1 - total))
            if not chunk:
                break
            total += len(chunk)
            require(total <= limit, "input exceeds byte bound: " + str(path))
            chunks.append(chunk)
        after = os.fstat(descriptor)
        stat_key = lambda item: (item.st_dev, item.st_ino, item.st_size,
                                 item.st_mtime_ns, item.st_ctime_ns)
        require(stat_key(before) == stat_key(after) and total == before.st_size,
                "input changed during read: " + str(path))
        return b"".join(chunks)
    finally:
        os.close(descriptor)


def snapshot_file(path, limit):
    path = Path(path)
    data = read_regular_file(path, limit)
    info = os.stat(path, follow_symlinks=False)
    return {"path": str(path), "bytes": len(data), "sha256": sha(data),
            "stat": [info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns]}


def verify_snapshot(snapshot):
    current = snapshot_file(snapshot["path"], max(snapshot["bytes"], 1))
    if (current["bytes"], current["sha256"], current["stat"]) != (
            snapshot["bytes"], snapshot["sha256"], snapshot["stat"]):
        raise ValueError("source changed or drifted during metadata build: " + snapshot["path"])
    return True


def _text_key(value):
    import unicodedata
    return unicodedata.normalize("NFC", value).casefold()


def _source_regions(title):
    # Region is an independent tag; revision/mastering/translation tags may follow.
    return {part.strip().casefold() for tag in re.findall(r"\(([^()]*)\)", title)
            for part in tag.split(",") if part.strip().casefold() in REGIONS}


def _valid_xml_source_path(raw):
    require(isinstance(raw, str) and raw and "\0" not in raw and "\\" not in raw,
            "invalid XML source path")
    parts = raw.split("/")
    require(not raw.startswith("/") and all(part != ".." for part in parts),
            "XML source path traversal is not allowed")
    path = PurePosixPath(raw)
    require(path.suffix.lower() in (".zip", ".chd", ".pce", ".sgx", ".cue", ".xml"),
            "unsupported XML source path")
    title = re.sub(r" \((?:pcenginecd|pcengine|tg16cd|tg16)\)$", "",
                   path.stem, flags=re.IGNORECASE)
    require(title and title not in (".", ".."), "invalid XML source identity")
    return title


def _parse_release_year(raw):
    value = (raw or "").strip()
    if not value:
        return None, None
    if re.fullmatch(r"\d{4}", value):
        year = int(value)
    else:
        match = re.fullmatch(r"(\d{4})(\d{2})(\d{2})(?:T\d{6})?", value)
        if match:
            year, month, day = map(int, match.groups())
            try:
                date(year, month, day)
            except ValueError:
                return None, value
        else:
            match = re.fullmatch(r"(\d{4})-(\d{2})-(\d{2})", value)
            if not match:
                return None, value
            year, month, day = map(int, match.groups())
            try:
                date(year, month, day)
            except ValueError:
                return None, value
    if not 1970 <= year <= 2099:
        return None, value
    return year, None


def _clean_company(raw):
    value = description_tools.clean(raw or "")
    if not value or value.casefold() in {
            "unknown", "n/a", "na", "none", "not available", "not set", "?"}:
        return None
    if len(value) > 256:
        return None
    return value


def _parse_xml_sources(xml_sources):
    require(isinstance(xml_sources, dict) and set(xml_sources) == set(XML_NAMES),
            "exact six metadata XML sources are required")
    records = []
    for filename in XML_NAMES:
        raw = xml_sources[filename]
        require(type(raw) is bytes and len(raw) <= MAX_XML_BYTES,
                "invalid or oversized metadata XML: " + filename)
        try:
            root = ET.fromstring(raw.decode("utf-8-sig"))
        except (UnicodeDecodeError, ET.ParseError) as error:
            raise ValueError("invalid metadata XML " + filename) from error
        require(root.tag == "gameList", "unexpected metadata XML root: " + filename)
        for game in root.findall("game"):
            raw_path = game.findtext("path", "").strip()
            if not raw_path:
                continue
            title = _valid_xml_source_path(raw_path)
            year, invalid_year = _parse_release_year(game.findtext("releasedate", ""))
            developer = _clean_company(game.findtext("developer", ""))
            publisher = _clean_company(game.findtext("publisher", ""))
            records.append({
                "source_file": filename,
                "source_title": title,
                "source_region": sorted(_source_regions(title)),
                "release_year": year,
                "invalid_release_year": invalid_year,
                "developer": developer,
                "publisher": publisher,
                "raw_developer_invalid": (game.findtext("developer", "").strip()
                                          if game.findtext("developer", "").strip() and developer is None
                                          else None),
                "raw_publisher_invalid": (game.findtext("publisher", "").strip()
                                          if game.findtext("publisher", "").strip() and publisher is None
                                          else None),
            })
    return records


def _source_catalog_entries(data):
    require(type(data) is bytes and len(data) <= MAX_SOURCE_BYTES,
            "source Catalog.json exceeds byte bound")
    try:
        source = json.loads(data.decode("utf-8-sig"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError("invalid source Catalog.json") from error
    entries = source.get("entries") if isinstance(source, dict) else None
    require(isinstance(entries, list) and len(entries) <= MAX_CATALOG_RECORDS,
            "invalid source catalog entry list")
    by_identity = {}
    for entry in entries:
        require(isinstance(entry, dict), "source catalog entry must be an object")
        title, path, region = entry.get("title"), entry.get("launch_path"), entry.get("region")
        require(all(isinstance(value, str) and value and "\0" not in value
                    for value in (title, path, region)), "source catalog identity is incomplete")
        require("\\" not in path and not path.startswith("/") and
                all(part not in (".", "..") for part in path.split("/")),
                "unsafe source catalog launch path")
        identity = (title, path, region)
        require(identity not in by_identity, "duplicate source catalog identity")
        require(isinstance(entry.get("source_records", []), list),
                "source record links must be a list")
        by_identity[identity] = entry
    return entries, by_identity


def _translation_parent(entry, entries):
    sources = description_tools.original_sources(entry)
    if not sources:
        return None, "translation_has_no_source_link"
    parents = [
        candidate for candidate in entries
        if candidate.get("category", "").startswith("Original ")
        and not description_tools.is_supergrafx(candidate)
        and description_tools.family(candidate) == description_tools.family(entry)
        and sources.intersection(description_tools.original_sources(candidate))
    ]
    if len(parents) != 1:
        return None, "translation_parent_unresolved" if not parents else "translation_parent_ambiguous"
    return parents[0], None


def _identity_source(entry, xml_records, crc_names):
    key, basis = description_tools.identity(entry, crc_names)
    if key is None:
        return [], {"status": "source_crc_ambiguous"}
    matches = [record for record in xml_records if _text_key(record["source_title"]) == _text_key(key)]
    if not matches:
        return [], {"status": "exact_source_record_not_found", "identity": key, "basis": basis}
    region = str(entry.get("region", "")).casefold()
    matches = [record for record in matches if region in record["source_region"]]
    if not matches:
        return [], {"status": "source_region_unproven", "identity": key, "basis": basis}
    return matches, {"status": "exact_source_region_match", "identity": key, "basis": basis}


def _field_resolution(matches, field):
    values = []
    invalid = []
    for record in matches:
        if field == "release_year":
            value = record["release_year"]
            if record["invalid_release_year"] is not None:
                invalid.append({"source": record["source_file"],
                                "value": record["invalid_release_year"]})
        else:
            value = record[field]
            raw_invalid = record["raw_" + field + "_invalid"]
            if raw_invalid is not None:
                invalid.append({"source": record["source_file"], "value": raw_invalid})
        if value is not None:
            values.append((value, record))
    normalized = {}
    for value, record in values:
        key = value if field == "release_year" else value.casefold()
        normalized.setdefault(key, (value, []))[1].append(record)
    if invalid:
        status = "invalid_source_value"
        chosen = None
    elif len(normalized) > 1:
        status = "conflict"
        chosen = None
    elif normalized:
        status = "sourced"
        chosen = next(iter(normalized.values()))[0]
    else:
        status = "unknown"
        chosen = None
    evidence = []
    for value, record in values:
        evidence.append({"source": record["source_file"], "source_title": record["source_title"],
                         "value": value})
    return chosen, status, evidence, invalid, sorted(normalized.keys(), key=str)


def _entry_metadata(entry, entries, xml_records, crc_names):
    parent = None
    if entry.get("category") == "Translation":
        parent, reason = _translation_parent(entry, entries)
        if parent is None:
            return {field: 0 if field == "release_year" else None for field in FIELDS}, {
                "parent_title": None, "identity": {"status": reason}, "fields": {},
                "field_conflicts": [], "developer_status": reason,
            }
        source_entry = parent
    else:
        source_entry = entry
    matches, identity = _identity_source(source_entry, xml_records, crc_names)
    resolved = {}
    field_evidence = {}
    conflicts = []
    for field in FIELDS:
        value, status, sources, invalid, variants = _field_resolution(matches, field)
        resolved[field] = value
        field_evidence[field] = {
            "status": status, "value": value, "sources": sources,
            "invalid_sources": invalid, "conflicting_values": variants if status == "conflict" else [],
        }
        if status in ("conflict", "invalid_source_value"):
            conflicts.append(field)
    developer_status = field_evidence["developer"]["status"]
    if (source_entry.get("title") == "1943 Kai (Japan)" and
            source_entry.get("region") == "Japan" and
            str(source_entry.get("original_crc32", "")).lower() == "fde08d6d"):
        resolved["developer"] = None
        developer_status = "unverified_port_developer_omitted"
        field_evidence["developer"]["status"] = developer_status
        field_evidence["developer"]["value"] = None
    return resolved, {
        "parent_title": parent.get("title") if parent else None,
        "identity": identity,
        "fields": field_evidence,
        "field_conflicts": conflicts,
        "developer_status": developer_status,
    }



def _year_title_identity(title):
    """Same regional game's first release, allowing only known non-edition tags."""
    match = re.fullmatch(r"(.*?)\s+\((Japan|USA|Europe|World)(?:, (?:Japan|USA|Europe|World))*\)(.*)",
                         title, re.IGNORECASE)
    if not match:
        return None
    tail = match[3].strip()
    while tail:
        tag = re.match(r"\(([^()]*)\)|\[([^\[\]]*)\]", tail)
        if not tag:
            return None
        value = tag[1] if tag[1] is not None else tag[2]
        valid = (re.fullmatch(r"(?:[A-Z][a-z])(?:,\s*[A-Z][a-z])*", value)
                 or re.fullmatch(r"Rev \d+", value)
                 or re.fullmatch(r"[A-Z]{4}(?:,\s*[A-Z]{4})*", value)
                 or value == "Unl" or value == "Translated En"
                 or value.startswith("T-En "))
        if not valid:
            return None
        tail = tail[tag.end():].strip()
    return _text_key(match[1])


def _parse_year_dat(filename, raw):
    require(filename in {"releaseyears-huc.dat", "releaseyears-sgx.dat"}, "unknown year DAT platform")
    require(type(raw) is bytes and len(raw) <= MAX_SOURCE_BYTES, "invalid year DAT bytes")
    text = raw.decode("utf-8-sig")
    blocks = re.split(r"(?m)^game\s*\(\s*$", text)
    require(re.fullmatch(r"\s*clrmamepro\s*\([\s\S]*\)\s*", blocks[0]) is not None,
            "year DAT header missing")
    result = []
    for block in blocks[1:]:
        match = re.fullmatch(r'\s*comment "([^"\n]+)"\s+releaseyear "(\d{4})"\s+'
                             r'rom\s*\(\s*crc ([0-9A-Fa-f]{8})\s*\)\s*\)\s*', block)
        require(match is not None, "malformed year DAT record")
        title, year, crc = match.groups()
        year = int(year)
        require(_year_value(year) and year != 0, "invalid year DAT date")
        result.append({"source": filename, "source_title": title, "year": year,
                       "crc": crc.lower(), "regions": _source_regions(title),
                       "family": "sgx" if filename == "releaseyears-sgx.dat" else "huc"})
    require(bool(result), "empty year DAT")
    return result


def build_year_topup(source_catalog, genres, xml_sources, catalog, baseline_years, year_sources, *, reviewed_years=None):
    """Fill only unknown TPY1 years; keep catalog IDs and known dates unchanged.

    XML evidence is constrained to the same media family, regional game and
    permitted revision/language/mastering tags. DAT evidence requires exact ROM
    CRC, region and platform. Disagreement remains unknown with its evidence.
    Translation facts require the existing explicit original-source linkage.
    """
    entries, by_identity = _source_catalog_entries(source_catalog)
    rows, generation = catalog_tools.parse_catalog(catalog)
    require(len(entries) == len(rows), "source/stable catalog count mismatch")
    stable_entries = []
    for row in rows:
        entry = by_identity.get((row["title"], row["path"], row["region"]))
        require(entry is not None, "stable identity absent from source catalog")
        stable_entries.append(entry)
    before = parse_year_pack(baseline_years, catalog)
    xml_records = _parse_xml_sources(xml_sources)
    require(type(genres) is bytes and len(genres) <= MAX_GENRES_BYTES, "invalid genres bytes")
    crc_names = description_tools.names_by_crc(genres.decode("utf-8-sig"))
    require(isinstance(year_sources, dict) and bool(year_sources), "year DAT sources required")
    dat_records = [r for filename, raw in year_sources.items() for r in _parse_year_dat(filename, raw)]
    reviewed = {}
    for item in reviewed_years or []:
        require(isinstance(item, dict), "reviewed year must be an object")
        index = item.get("index")
        require(type(index) is int and 0 <= index < len(rows) and index not in reviewed,
                "reviewed year index is invalid or duplicated")
        row = rows[index]
        expected_media = ("sgx" if description_tools.is_supergrafx(stable_entries[index])
                          else description_tools.family(stable_entries[index]))
        require((item.get("key"), item.get("title"), item.get("path"), item.get("region"), item.get("media")) ==
                (f"{row['key']:08x}", row["title"], row["path"], row["region"], expected_media),
                "reviewed year catalog identity mismatch")
        year = item.get("year")
        require(_year_value(year) and year != 0, "reviewed year outside range")
        release_date, quote = item.get("date"), item.get("quote")
        require(isinstance(release_date, str) and release_date and isinstance(quote, str)
                and len(quote) <= 8192 and release_date in quote, "reviewed date evidence missing")
        require(set(re.findall(r"\b(?:19|20)\d{2}\b", release_date)) == {str(year)},
                "reviewed date/year mismatch")
        require(item.get("source_url") == "https://en.wikipedia.org/wiki/List_of_TurboGrafx-16_games",
                "unexpected reviewed date source")
        reviewed[index] = item
    after = list(before)
    ledger = []
    for index, (row, original) in enumerate(zip(rows, stable_entries)):
        evidence = []
        parent_title = None
        reason = None
        entry = original
        if entry.get("category") == "Translation":
            entry, reason = _translation_parent(entry, stable_entries)
            parent_title = entry["title"] if entry else None
        if entry is not None:
            family = "sgx" if description_tools.is_supergrafx(entry) else description_tools.family(entry)
            region = entry["region"].casefold()
            crc = str(entry.get("original_crc32", "")).lower()
            identity, _ = description_tools.identity(entry, crc_names)
            titles = {entry["title"]}
            if identity:
                titles.add(identity)
            canonical = {v for t in titles if (v := _year_title_identity(t)) is not None}
            for record in xml_records:
                record_family = "cd" if "cd.xml" in record["source_file"] else "huc"
                same_title = (record["source_title"] in titles or
                              _year_title_identity(record["source_title"]) in canonical)
                if (record_family == family and region in record["source_region"] and same_title):
                    if record["invalid_release_year"] is not None:
                        evidence.append({"source":record["source_file"],"source_title":record["source_title"],
                                         "year":None,"invalid":record["invalid_release_year"]})
                    elif record["release_year"]:
                        evidence.append({"source":record["source_file"],"source_title":record["source_title"],
                                         "year":record["release_year"],"match":"regional media identity"})
            for record in dat_records:
                if record["family"] == family and crc == record["crc"] and region in record["regions"]:
                    evidence.append({"source":record["source"],"source_title":record["source_title"],
                                     "year":record["year"],"match":"exact CRC and region", "crc":crc})
        reviewed_item = reviewed.get(index)
        if reviewed_item:
            evidence.append({"source":reviewed_item["source_url"], "year":reviewed_item["year"],
                             "date":reviewed_item["date"], "quote":reviewed_item["quote"],
                             "match":"reviewed catalog key, regional release and media"})
        years = {e["year"] for e in evidence if e["year"] is not None}
        invalid = any(e["year"] is None for e in evidence)
        if before[index]:
            status = "preserved"
        elif reviewed_item:
            after[index] = reviewed_item["year"]
            status = "added"
        elif len(years) == 1 and not invalid:
            after[index] = next(iter(years))
            status = "added"
        elif len(years) > 1 or invalid:
            status = "conflict"
        else:
            status = reason or "no_proven_year"
        ledger.append({"index":index,"key":f"{row['key']:08x}","title":row["title"],
                       "path":row["path"],"region":row["region"],"before":before[index],
                       "year":after[index],"status":status,"parent_title":parent_title,"evidence":evidence,
                       "resolution":"preserved" if before[index] else "reviewed regional source" if reviewed_item
                       else "source agreement" if after[index] else "unresolved"})
    payload = build_year_pack(catalog, after)
    return {"years.bin":payload, "report":{
        "status":"PASS_ADDITIVE_YEAR_TOPUP", "catalog_generation":f"{generation:08x}",
        "catalog_sha256":sha(catalog), "catalog_records":len(rows),
        "baseline_sha256":sha(baseline_years), "years_sha256":sha(payload),
        "known_before":sum(bool(y) for y in before), "known_after":sum(bool(y) for y in after),
        "added":sum(r["status"] == "added" for r in ledger),
        "unknown":sum(not y for y in after), "records":ledger,
        "source_catalog_sha256":sha(source_catalog), "genres_sha256":sha(genres),
        "xml_sha256":{n:sha(raw) for n,raw in xml_sources.items()},
        "dat_sha256":{n:sha(raw) for n,raw in year_sources.items()},
        "reviewed_years":len(reviewed),
        "reviewed_years_sha256":sha(json.dumps(reviewed_years or [], sort_keys=True).encode()),
        "known_years_modified":False,"runtime_format_changed":False,"hardware_tested":False,
    }}


def _validate_description(data, index):
    require(type(data) is bytes and len(data) <= MAX_DESCRIPTION_BYTES,
            "existing description exceeds 8192 bytes at index " + str(index))
    require(all(byte == 10 or 32 <= byte <= 126 for byte in data),
            "existing description is not printable bounded ASCII at index " + str(index))
    require(all(len(line) <= 38 for line in data.split(b"\n")),
            "existing description line exceeds 38 columns at index " + str(index))


def _format_company(label, value):
    if value is None:
        return []
    text = label + ": " + value
    return textwrap.wrap(text, width=38, break_long_words=True, break_on_hyphens=False)


def build_metadata_payloads(source_catalog: bytes, genres: bytes, xml_sources: dict[str, bytes],
                            catalog: bytes, descriptions: dict[int, bytes]) -> dict:
    """Resolve exact source/region fields and return in-memory, unchanged-ID payloads."""
    require(type(genres) is bytes and len(genres) <= MAX_GENRES_BYTES,
            "genres.dat exceeds byte bound")
    require(type(catalog) is bytes, "stable catalog must be exact bytes")
    rows, generation = catalog_tools.parse_catalog(catalog)
    require(len(rows) <= MAX_CATALOG_RECORDS, "stable catalog record count exceeds bound")
    source_entries, source_by_identity = _source_catalog_entries(source_catalog)
    require(len(source_entries) == len(rows), "source/stable catalog count mismatch")
    xml_records = _parse_xml_sources(xml_sources)

    stable_entries = []
    for row in rows:
        identity = (row["title"], row["path"], row["region"])
        entry = source_by_identity.get(identity)
        require(entry is not None, "stable catalog identity is absent from source catalog: " + row["title"])
        stable_entries.append(entry)
    require(len({id(entry) for entry in stable_entries}) == len(rows),
            "source catalog identity is reused")
    crc_names = description_tools.names_by_crc(genres.decode("utf-8-sig"))
    years = []
    changed_descriptions = {}
    provenance_rows = []
    field_conflicts = {}
    coverage = {field: 0 for field in FIELDS}
    unknowns = {field: 0 for field in FIELDS}

    for index, (row, entry) in enumerate(zip(rows, stable_entries)):
        resolved, provenance = _entry_metadata(entry, stable_entries, xml_records, crc_names)
        year = resolved["release_year"] or 0
        require(_year_value(year), "resolved release year outside allowed range")
        years.append(year)
        fields = provenance["fields"]
        if fields:
            for field in FIELDS:
                if resolved[field] is not None and resolved[field] != 0:
                    coverage[field] += 1
                else:
                    unknowns[field] += 1
        else:
            unknowns["release_year"] += 1
            unknowns["developer"] += 1
            unknowns["publisher"] += 1
        if provenance["field_conflicts"]:
            field_conflicts[row["title"]] = sorted(set(
                field_conflicts.get(row["title"], []) + provenance["field_conflicts"]))

        description = descriptions.get(index)
        if description is not None:
            _validate_description(description, index)
        lines = _format_company("Developer", resolved["developer"])
        lines += _format_company("Publisher", resolved["publisher"])
        desc_written = None
        if description is not None and lines:
            new_description = ("\n".join(lines).encode("ascii") + b"\n\n" + description)
            _validate_description(new_description, index)
            changed_descriptions[index] = new_description
            desc_written = {"original_bytes": len(description), "original_sha256": sha(description),
                            "new_bytes": len(new_description), "new_sha256": sha(new_description)}
        record_report = {
            "index": index, "title": row["title"], "launch_path": row["path"],
            "region": row["region"], "stable_key": f"{row['key']:08x}",
            "release_year": year, "developer": resolved["developer"],
            "publisher": resolved["publisher"], "parent_title": provenance["parent_title"],
            "identity": provenance["identity"], "fields": fields,
            "field_conflicts": provenance["field_conflicts"],
            "developer_status": provenance["developer_status"],
            "description": desc_written,
        }
        provenance_rows.append(record_report)

    year_pack = build_year_pack(catalog, years)
    report = {
        "status": "PASS_PRIVATE_METADATA_STAGE",
        "catalog_generation": f"{generation:08x}",
        "catalog_records": len(rows),
        "year_pack": {"bytes": len(year_pack), "sha256": sha(year_pack)},
        "fields": {
            field: {"known": coverage[field], "unknown": unknowns[field]}
            for field in FIELDS
        },
        "field_conflicts": field_conflicts,
        "conflict_count_basis": "catalog_records",
        "conflict_counts": {
            field: sum(field in record["field_conflicts"] for record in provenance_rows)
            for field in FIELDS
        },
        "records": provenance_rows,
        "source_xml_records": len(xml_records),
        "source_catalog_sha256": sha(source_catalog),
        "genres_sha256": sha(genres),
        "xml_sha256": {name: sha(xml_sources[name]) for name in XML_NAMES},
        "catalog_sha256": sha(catalog),
        "source_library_modified": False,
        "stable_catalog_modified": False,
        "existing_descriptions_overwritten": False,
        "hardware_tested": False,
    }
    return {"catalog2.bin": catalog, "years.bin": year_pack,
            "descriptions": changed_descriptions, "report": report}


def _write_file(path, data):
    with Path(path).open("xb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    require(Path(path).read_bytes() == data, "metadata stage readback mismatch: " + str(path))


def write_stage(destination, payloads):
    """Atomically publish a new local stage; never overwrite an existing path."""
    destination = Path(destination)
    require(not destination.exists() and not destination.is_symlink(),
            "new metadata stage required; no overwrite")
    parent = destination.parent.resolve(strict=True)
    require(destination.name not in ("", ".", ".."), "invalid metadata stage name")
    require(set(payloads) == {"catalog2.bin", "years.bin", "descriptions", "report"},
            "metadata stage payload shape mismatch")
    require(type(payloads["catalog2.bin"]) is bytes and type(payloads["years.bin"]) is bytes,
            "metadata stage catalogs must be exact bytes")
    require(isinstance(payloads["descriptions"], dict), "description delta must be a mapping")
    with tempfile.TemporaryDirectory(dir=parent, prefix=".game-metadata-") as temporary:
        temp = Path(temporary)
        menu = temp / "teo-menu4"
        (menu / "desc").mkdir(parents=True)
        _write_file(menu / "catalog2.bin", payloads["catalog2.bin"])
        _write_file(menu / "years.bin", payloads["years.bin"])
        for index, data in sorted(payloads["descriptions"].items()):
            require(type(index) is int and 0 <= index < MAX_CATALOG_RECORDS,
                    "invalid description index")
            _validate_description(data, index)
            _write_file(menu / "desc" / f"{index:06d}.txt", data)
        report_data = (json.dumps(payloads["report"], ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode()
        _write_file(temp / "private-provenance.json", report_data)
        require(not destination.exists() and not destination.is_symlink(),
                "metadata stage appeared during preparation")
        os.replace(temp, destination)
    return destination


def _inventory_descriptions(directory, count):
    directory = Path(directory)
    _check_no_symlink_components(directory)
    require(directory.is_dir() and not directory.is_symlink(),
            "stable description directory is missing or unsafe")
    files = {}
    for path in sorted(directory.iterdir()):
        require(not path.is_symlink(), "description source symlink is not allowed: " + str(path))
        if not path.name.endswith(".txt"):
            continue
        match = re.fullmatch(r"(\d{6})\.txt", path.name)
        require(match is not None, "unexpected description source filename: " + path.name)
        index = int(match.group(1))
        require(index < count and index not in files, "description index outside stable catalog")
        data = read_regular_file(path, MAX_DESCRIPTION_BYTES)
        _validate_description(data, index)
        files[index] = data
    return files


def _input_snapshots(paths):
    snapshots = []
    for path, limit in paths:
        snapshots.append(snapshot_file(path, limit))
    return snapshots


def _verify_all_snapshots(snapshots, description_dir, description_files):
    for snapshot in snapshots:
        verify_snapshot(snapshot)
    current = _inventory_descriptions(description_dir, MAX_CATALOG_RECORDS)
    require(current.keys() == description_files.keys(), "description directory changed during build")
    for index, data in description_files.items():
        require(sha(current[index]) == sha(data),
                "description source drift during metadata build at index " + str(index))
    return True


def _safe_output(destination):
    destination = Path(os.path.abspath(destination))
    _check_no_symlink_components(destination.parent)
    require(not destination.exists() and not destination.is_symlink(),
            "new metadata stage required; no overwrite")
    PRIVATE_ROOT.mkdir(parents=True, exist_ok=True)
    root = PRIVATE_ROOT.resolve(strict=True)
    parent = destination.parent.resolve(strict=True)
    require(parent == root or root in parent.parents,
            "metadata output must stay inside the private game-metadata folder")
    return destination


def build_from_paths(source_catalog, genres_path, xml_paths, stable_card, destination):
    """Read pinned inputs, check source stability, then write a new private stage."""
    source_catalog = Path(source_catalog)
    genres_path = Path(genres_path)
    stable_card = Path(stable_card)
    require(isinstance(xml_paths, dict) and set(xml_paths) == set(XML_NAMES),
            "exact six metadata XML input paths are required")
    catalog_path = stable_card / "catalog2.bin"
    description_dir = stable_card / "desc"
    paths = [(source_catalog, MAX_SOURCE_BYTES), (genres_path, MAX_GENRES_BYTES),
             (catalog_path, MAX_SOURCE_BYTES)]
    paths.extend((Path(xml_paths[name]), MAX_XML_BYTES) for name in XML_NAMES)
    snapshots = _input_snapshots(paths)
    source_data = read_regular_file(source_catalog, MAX_SOURCE_BYTES)
    genre_data = read_regular_file(genres_path, MAX_GENRES_BYTES)
    stable_data = read_regular_file(catalog_path, MAX_SOURCE_BYTES)
    xml_data = {name: read_regular_file(xml_paths[name], MAX_XML_BYTES) for name in XML_NAMES}
    count = struct.unpack_from("<H", stable_data, 6)[0] if len(stable_data) >= 8 else 0
    descriptions = _inventory_descriptions(description_dir, count)
    payloads = build_metadata_payloads(source_data, genre_data, xml_data, stable_data, descriptions)
    _verify_all_snapshots(snapshots, description_dir, descriptions)
    payloads["report"]["inputs"] = {
        "source_catalog": snapshots[0],
        "genres": snapshots[1],
        "stable_catalog": snapshots[2],
        "xml": {name: snapshots[3 + index] for index, name in enumerate(XML_NAMES)},
        "description_files": len(descriptions),
    }
    destination = _safe_output(destination)
    return write_stage(destination, payloads)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    parser.add_argument("--genres", type=Path, default=PROJECT / "assets" / "genres.dat")
    parser.add_argument("--xml-root", type=Path, default=PROJECT / "assets")
    parser.add_argument("--stable-card", type=Path, default=DEFAULT_STABLE_CARD)
    parser.add_argument("--output", type=Path, default=PRIVATE_ROOT / "stage")
    args = parser.parse_args()
    xml_paths = {name: args.xml_root / name for name in XML_NAMES}
    destination = build_from_paths(args.catalog, args.genres, xml_paths,
                                   args.stable_card, args.output)
    report = json.loads((destination / "private-provenance.json").read_text(encoding="utf-8"))
    print(json.dumps({
        "status": report["status"], "stage": str(destination),
        "catalog_records": report["catalog_records"],
        "fields": report["fields"], "conflict_counts": report["conflict_counts"],
        "years_sha256": report["year_pack"]["sha256"],
    }, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)

