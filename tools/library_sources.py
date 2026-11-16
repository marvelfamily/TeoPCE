"""Pinned public metadata, cached artwork and explicit user-provided facts."""
from collections import defaultdict
from functools import lru_cache
from io import BytesIO
import hashlib
import json
from pathlib import Path
import re
import sys
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen

from PIL import Image, ImageDraw
import artwork
import convert_art
import enrich_genres
import library_media as media

DATABASE_REVISION = "969e31a324119a17add184e8b716315a74adb51b"
FAMILIES = {1: "NEC - PC Engine - TurboGrafx 16", 2: "NEC - PC Engine CD - TurboGrafx-CD",
            3: "NEC - PC Engine SuperGrafx"}
DATABASE_BASE = "https://raw.githubusercontent.com/libretro/libretro-database/" + DATABASE_REVISION + "/"
CATEGORY_NAMES = {"original": 1, "translations": 2, "homebrew": 3, "museum": 4,
                  "tools": 5, "unlicensed": 6}


def download(url, cache, offline=False, limit=16 * 1024 * 1024, progress=None, label=None):
    progress = progress or (lambda message: None)
    label = label or url
    cache.mkdir(parents=True, exist_ok=True)
    name = hashlib.sha256(url.encode()).hexdigest()
    path = cache / name
    bundled = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parents[1])) / "resources/database-cache" / name
    if not path.exists() and bundled.is_file():
        path = bundled
    if path.is_file():
        raw = media.read_file(path, limit)
        digest_path = path.with_name(name + ".sha256")
        if not digest_path.is_file() or digest_path.read_text().strip() != hashlib.sha256(raw).hexdigest():
            raise ValueError("Cached download checksum mismatch: " + str(path))
        progress("%s · %s" % ("Using bundled data" if path == bundled else "Using cached data", label))
        return raw
    if offline or (cache / (name + ".missing")).exists():
        progress(("Unavailable offline · " if offline else "Not found in source · ") + label)
        return None
    progress("Downloading · " + label)
    try:
        req = Request(url, headers={"User-Agent": "TeoPCE-library-builder/1"})
        with urlopen(req, timeout=15) as response:
            raw = response.read(limit + 1)
        if len(raw) > limit:
            raise ValueError("Download exceeds size limit")
    except HTTPError as error:
        if error.code == 404:
            (cache / (name + ".missing")).touch()
        progress(("Not found online · " if error.code == 404 else "Download unavailable · ") + label)
        return None
    except (OSError, URLError, TimeoutError):
        progress("Connection unavailable · " + label)
        return None
    # Each process writes its own staging file, then publishes complete bytes.
    import os
    import tempfile
    with tempfile.NamedTemporaryFile(dir=cache, delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(raw)
    temporary.replace(path)
    digest = hashlib.sha256(raw).hexdigest()
    with tempfile.NamedTemporaryFile(dir=cache, mode="w", delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(digest + "\n")
    temporary.replace(cache / (name + ".sha256"))
    progress("Downloaded · " + label)
    return raw


def dat_games(raw):
    if raw is None:
        return []
    games = []
    for block in re.findall(r"(?ms)^game\s*\(\s*(.*?)^\)", raw.decode("utf-8-sig")):
        fields = dict(re.findall(r'^\s*(\w+)\s+"([^"\n]*)"', block, re.M))
        roms = []
        for rom in re.findall(r"(?m)^\s*rom\s*\((.*)\)\s*$", block):
            values = re.findall(r'(\w+)\s+(?:"([^"\n]*)"|([^\s]+))', rom)
            roms.append({key: quoted or plain for key, quoted, plain in values})
        games.append({**fields, "roms": roms})
    return games


class Metadata:
    def __init__(self, families, cache, offline=False, overrides=None, progress=None):
        self.cache = Path(cache)
        self.offline = offline
        self.identities = defaultdict(list)
        self.facts = {}
        self.sources = []
        self.warnings = []
        self.overrides = overrides or {}
        for kind in sorted(families):
            family = FAMILIES[kind]
            system = "redump" if kind == 2 else "no-intro"
            for field in [system, "genre", "releaseyear", "developer", "publisher"]:
                relative = "metadat/" + field + "/" + family + ".dat"
                raw = download(DATABASE_BASE + quote(relative), self.cache, offline,
                               progress=progress, label="Game facts / " + family + " / " + field)
                if raw is None:
                    self.warnings.append("Metadata unavailable: " + relative)
                    continue
                self.sources.append({"url": DATABASE_BASE + quote(relative),
                                     "sha256": hashlib.sha256(raw).hexdigest(),
                                     "license": "CC-BY-SA-4.0"})
                for game in dat_games(raw):
                    if field == system:
                        for rom in game["roms"]:
                            if "sha1" in rom and "size" in rom:
                                self.identities[kind, int(rom["size"]), rom["sha1"].lower()].append(game)
                    else:
                        for rom in game["roms"]:
                            if "crc" in rom and field in game:
                                key = kind, int(rom["crc"], 16), field
                                value = game[field]
                                old = self.facts.get(key, value)
                                self.facts[key] = value if old == value else None

    def resolve(self, game, fingerprints):
        kind = game["kind"]
        matches = []
        for name in game["data_files"]:
            fp = fingerprints[name]
            matches.extend(self.identities.get((kind, fp["size"], fp["sha1"]), []))
        titles = {m.get("name") for m in matches}
        title = next(iter(titles)) if len(titles) == 1 else None
        path = Path(game["path"])
        fallback = path.parent.name if kind == 2 and path.stem.lower() in ("disc", "disk", "game") else path.stem
        result = {"title": title or fallback, "genre": "Unknown", "year": 0,
                  "region": "", "description": "", "category": category(game["path"]),
                  "match": "sha1+size" if title else "filename", "art_title": title or fallback}
        if title:
            result["region"] = region(title)
            for field in ["genre", "releaseyear", "developer", "publisher"]:
                values = {self.facts.get((kind, fingerprints[name]["crc32"], field))
                          for name in game["data_files"]}
                values.discard(None)
                if len(values) != 1:
                    continue
                value = next(iter(values))
                if field == "genre":
                    result["genre"] = enrich_genres.normalize(value) or "Unknown"
                elif field == "releaseyear" and value.isdigit() and 1970 <= int(value) <= 2099:
                    result["year"] = int(value)
                elif field in ("developer", "publisher"):
                    result[field] = value
        if not result["region"]:
            result["region"] = "English" if result["category"] == 2 else region(fallback)
        override = self.overrides.get(game["path"], {})
        allowed = {"title", "genre", "region", "year", "description", "category",
                   "developer", "publisher", "cover", "screen", "art_title"}
        if set(override) - allowed:
            raise ValueError("Unknown metadata fields for " + game["path"])
        result.update(override)
        if isinstance(result["category"], str):
            if result["category"] not in CATEGORY_NAMES:
                raise ValueError("Unknown category for " + game["path"])
            result["category"] = CATEGORY_NAMES[result["category"]]
        if type(result["category"]) is not int or not 1 <= result["category"] <= 6:
            raise ValueError("Category must be 1..6")
        if type(result["year"]) is not int or result["year"] != 0 and not 1970 <= result["year"] <= 2099:
            raise ValueError("Year must be 0 or 1970..2099")
        for key in ["title", "genre", "region", "description", "developer", "publisher", "cover", "screen", "art_title"]:
            if key in result and not isinstance(result[key], str):
                raise ValueError("Metadata field must be text: " + key)
        if len(result["description"]) > 16000:
            raise ValueError("Description exceeds size limit")
        return result


def region(title):
    if re.search(r"\(USA(?:\)|,)", title):
        return "USA"
    if re.search(r"\(Japan(?:\)|,)", title):
        return "Japan"
    return ""


def category(path):
    root = path.split("/")[0]
    from build_find_index import CATEGORY_ROOTS
    if root in CATEGORY_ROOTS:
        return CATEGORY_ROOTS[root]
    parts = {p.casefold() for p in Path(path).parts[:-1]}
    for names, value in [({"translations", "translation", "patches"}, 2),
                         ({"homebrew"}, 3), ({"museum"}, 4),
                         ({"tools", "tests", "utilities"}, 5), ({"unlicensed"}, 6)]:
        if parts & names:
            return value
    return 1


def placeholder():
    image = Image.new("RGB", (96, 72), (22, 29, 38))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((29, 9, 67, 61), radius=3, outline=(115, 130, 145), width=2)
    draw.line((37, 19, 59, 19), fill=(115, 130, 145), width=2)
    draw.text((45, 32), "?", fill=(170, 185, 200))
    output = BytesIO()
    image.save(output, format="PNG")
    return output.getvalue()


@lru_cache(maxsize=2)
def placeholder_art(legacy=False):
    convert = convert_art.convert if legacy else convert_art.convert_two_palette
    return convert(BytesIO(placeholder()))


def get_art(metadata, game, code, metadata_root, cache, offline, progress=None):
    progress = progress or (lambda message: None)
    label = ("Cover" if code == "c" else "Screenshot") + " · " + metadata["title"]
    field = "cover" if code == "c" else "screen"
    if metadata.get(field):
        source = (metadata_root / metadata[field]).absolute()
        raw = media.read_file(source, 12 * 1024 * 1024)
        provenance = {"source": "user image", "path": str(source)}
        progress("Using your image · " + label)
    else:
        family = "cd" if game["kind"] == 2 else "huc"
        repo, revision = artwork.REPOS[family]
        title = metadata["art_title"].translate(artwork._FILENAME_REPLACEMENTS)
        directory = "Named_Boxarts" if code == "c" else "Named_Snaps"
        url = "https://raw.githubusercontent.com/libretro-thumbnails/" + repo + "/" + revision + "/" + quote(directory + "/" + title + ".png")
        raw = download(url, cache, offline, 12 * 1024 * 1024, progress=progress, label=label)
        provenance = {"url": url, "source": "libretro-thumbnails", "match": metadata["match"]}
    if raw is None:
        raw = placeholder()
        provenance = {"source": "placeholder"}
    try:
        compiled = convert_art.convert_two_palette(BytesIO(raw))
    except (OSError, ValueError, Image.DecompressionBombError):
        if metadata.get(field):
            raise ValueError("Invalid user image: " + metadata[field])
        raw = placeholder()
        provenance = {"source": "placeholder", "reason": "invalid downloaded image"}
        compiled = convert_art.convert_two_palette(BytesIO(raw))
    return compiled, {**provenance, "source_sha256": hashlib.sha256(raw).hexdigest(),
                      "converted_sha256": hashlib.sha256(compiled).hexdigest()}
