#!/usr/bin/env python3
"""Match source artwork to an existing catalog without fuzzy guessing."""
import argparse
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
from io import BytesIO
import json
from pathlib import Path
import re
import subprocess
import time
import unicodedata
from urllib.parse import quote
from urllib.request import Request, urlopen

REPOS = {
    "huc": ("NEC_-_PC_Engine_-_TurboGrafx_16", "33cbdb64bd99347ea47a5390584214f18d772b1f"),
    "cd": ("NEC_-_PC_Engine_CD_-_TurboGrafx-CD", "341caf6e4bd096586edec34a2cab593c9ba2f3b5"),
}


def names_by_crc(text):
    result = defaultdict(set)
    for block in re.findall(r"(?ms)^game\s*\(\s*(.*?)^\)", text):
        name = re.search(r'(?m)^\s*comment\s+"([^"]+)"', block)
        crc = re.search(r"\bcrc\s+([0-9a-fA-F]{8})\b", block)
        if name and crc:
            result[crc.group(1).lower()].add(name.group(1))
    return dict(result)


# Libretro's filename exception keeps every other title/region/version character.
# https://docs.libretro.com/guides/roms-playlists-thumbnails/#thumbnail-naming-convention
_FILENAME_REPLACEMENTS = str.maketrans({char: "_" for char in "&*/:`<>?\\|"})


def _text_key(text):
    return unicodedata.normalize("NFC", unicodedata.normalize("NFC", text).casefold())


def _filename_key(filename):
    return _text_key(filename.translate(_FILENAME_REPLACEMENTS))


def _unique_thumbnail_paths(paths):
    candidates = defaultdict(set)
    for source in paths:
        directory, separator, filename = source.partition("/")
        if not separator or "/" in filename or directory not in ("Named_Boxarts", "Named_Snaps"):
            continue
        candidates[(directory, _filename_key(filename))].add(source)
    return {key: next(iter(sources)) for key, sources in candidates.items() if len(sources) == 1}


def match_jobs(entries, indexes, crc_names):
    paths = {family: _unique_thumbnail_paths(index) for family, index in indexes.items()}
    resolved = []
    identities = defaultdict(set)
    for index, entry in enumerate(entries):
        path = entry["launch_path"]
        if path.lower().endswith(".sgx") or path.lower().startswith("03 supergrafx/"):
            continue
        family = "cd" if path.lower().endswith(".cue") else "huc"
        matches = crc_names.get(str(entry.get("original_crc32", "")).lower())
        if matches and len(matches) != 1:
            continue
        name = next(iter(matches)) if matches else entry["title"]
        match = "crc" if matches else "exact title"
        key = _filename_key(name + ".png")
        # Distinct names can lose information under the documented replacement.
        identities[(family, key)].add(_text_key(name))
        resolved.append((index, entry, family, key, match))
    jobs = []
    for index, entry, family, key, match in resolved:
        if len(identities[(family, key)]) != 1:
            continue
        for code, directory in (("c", "Named_Boxarts"), ("s", "Named_Snaps")):
            source = paths[family].get((directory, key))
            if source is not None:
                jobs.append({"index": index, "image": code, "family": family,
                             "path": source, "match": match, "title": entry["title"]})
    return jobs


def repo_index(path):
    output = subprocess.check_output(["git", "-C", str(path), "-c", "core.quotePath=false",
                                      "ls-tree", "-r", "--name-only", "-z", "HEAD"])
    return set(output.decode("utf-8").split("\0"))


def fetch_group(key, jobs, output):
    from convert_art import convert
    family, source = key
    repo, revision = REPOS[family]
    url = f"https://raw.githubusercontent.com/libretro-thumbnails/{repo}/{revision}/{quote(source)}"
    last_error = None
    for attempt in range(3):
        try:
            request = Request(url, headers={"User-Agent": "TeoPCE-catalog/1"})
            with urlopen(request, timeout=25) as response:
                data = response.read(12 * 1024 * 1024 + 1)
            if len(data) > 12 * 1024 * 1024:
                raise ValueError("image exceeds 12 MiB")
            compiled = convert(BytesIO(data))
            if len(compiled) != 3488:
                raise ValueError("unexpected converted image size")
            for job in jobs:
                dest = output / f'{job["index"]:06d}-{job["image"]}.bin'
                temporary = dest.with_suffix(".tmp")
                temporary.write_bytes(compiled)
                temporary.replace(dest)
            return {"ok": True, "url": url, "source_sha256": hashlib.sha256(data).hexdigest(),
                    "converted_sha256": hashlib.sha256(compiled).hexdigest(), "jobs": jobs}
        except Exception as exc:
            last_error = str(exc)
            if attempt < 2:
                time.sleep(0.3 * (attempt + 1))
    return {"ok": False, "url": url, "error": last_error, "jobs": jobs}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog", type=Path)
    parser.add_argument("genres", type=Path)
    parser.add_argument("huc_repository", type=Path)
    parser.add_argument("cd_repository", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--workers", type=int, default=10)
    args = parser.parse_args()
    entries = json.loads(args.catalog.read_text(encoding="utf-8"))["entries"]
    entries.sort(key=lambda e: (e["title"].casefold(), e["launch_path"].casefold()))
    indexes = {"huc": repo_index(args.huc_repository), "cd": repo_index(args.cd_repository)}
    jobs = match_jobs(entries, indexes, names_by_crc(args.genres.read_text(encoding="utf-8")))
    groups = defaultdict(list)
    for job in jobs:
        groups[(job["family"], job["path"])].append(job)
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    print(f"{len(jobs)} matched images, {len(groups)} unique downloads", flush=True)
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = [pool.submit(fetch_group, key, group, args.output) for key, group in groups.items()]
        for future in as_completed(futures):
            results.append(future.result())
            if len(results) % 100 == 0:
                print(f"Processed {len(results)}/{len(futures)}", flush=True)
    results.sort(key=lambda item: item["url"])
    manifest = {"repositories": REPOS, "matched_images": len(jobs),
                "successful_images": sum(len(r["jobs"]) for r in results if r["ok"]),
                "failed_images": sum(len(r["jobs"]) for r in results if not r["ok"]),
                "results": results}
    manifest_path = args.output.parent / "artwork-manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f'Saved {manifest["successful_images"]} images; {manifest["failed_images"]} downloads failed', flush=True)


if __name__ == "__main__":
    main()
