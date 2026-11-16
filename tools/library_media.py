"""Read-only library scanning and source verification for portable preparation."""
import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import zlib
import wave

import build_launch_metadata as launch
import build_catalog
import build_index

IGNORED_DIRS = {"00teopce", "edturbo", ".spotlight-v100", ".fseventsd", ".trashes"}


def regular(path):
    path = Path(path).absolute()
    if path.is_symlink():
        raise ValueError("Symbolic links are unsupported: " + str(path))
    if not path.is_file():
        raise ValueError("File is missing: " + str(path))
    return path


def read_file(path, limit):
    path = regular(path)
    if path.stat().st_size > limit:
        raise ValueError("File exceeds size limit: " + str(path))
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError("File exceeds size limit: " + str(path))
    return data


def fingerprint(path):
    path = regular(path)
    sha256 = hashlib.sha256()
    sha1 = hashlib.sha1()
    md5 = hashlib.md5()
    crc = 0
    count = 0
    with path.open("rb") as stream:
        before = os.fstat(stream.fileno())
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            count += len(block)
            sha256.update(block)
            sha1.update(block)
            md5.update(block)
            crc = zlib.crc32(block, crc)
        after = os.fstat(stream.fileno())
    keys = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
    if keys(before) != keys(after) or count != before.st_size:
        raise ValueError("Source changed while reading: " + str(path))
    return {"size": count, "sha256": sha256.hexdigest(), "sha1": sha1.hexdigest(),
            "md5": md5.hexdigest(), "crc32": crc & 0xffffffff}


def safe_relative(path):
    if not isinstance(path, str) or not path or len(path.encode("ascii")) >= 128:
        raise ValueError("Game paths must be printable ASCII and shorter than 128 bytes")
    build_index._validate_path(path, 0)
    if any(part.startswith(" ") for part in path.split("/")):
        raise ValueError("A path segment starts with a space: " + path)
    if any(ord(c) > 126 for c in path):
        raise ValueError("Game paths must be printable ASCII: " + path)
    return path


def cue_files(root, relative):
    raw = read_file(root / relative, launch.MAX_CUE)
    text = raw.decode("utf-8-sig")
    names = []
    for line in text.splitlines():
        match = re.fullmatch(r'\s*FILE\s+(?:"([^"\n]+)"|(\S+))\s+(BINARY|WAVE)\s*', line, re.I)
        if not match:
            continue
        name = match[1] or match[2]
        if "\\" in name or any(ord(c) < 32 or ord(c) > 126 for c in name):
            raise ValueError("CUE track path must use printable ASCII and / separators")
        pieces = name.split("/")
        if PurePosixPath(name).is_absolute() or any(p in ("", ".", "..") for p in pieces):
            raise ValueError("Unsafe CUE track path: " + name)
        if any(c in name for c in ':*?"<>|'):
            raise ValueError("Unsafe CUE track path: " + name)
        track = (PurePosixPath(relative).parent / name).as_posix()
        names.append(track)
    if not names:
        raise ValueError("CUE has no supported FILE statements")
    if re.search(r"\bMODE1/2048\b", text, re.I):
        raise ValueError("MODE1/2048 needs a raw-sector conversion for this EverDrive workflow")
    return raw, list(dict.fromkeys(names))


def inspect_game(root, relative, fingerprints):
    safe_relative(relative)
    path = root / relative
    kind = 2 if path.suffix.lower() == ".cue" else 3 if path.suffix.lower() == ".sgx" or "supergrafx" in relative.split("/")[0].casefold() else 1
    names = [relative]
    if kind == 2:
        raw, tracks = cue_files(root, relative)
        names += tracks
    for name in names:
        if name not in fingerprints:
            fingerprints[name] = fingerprint(root / name)
    fp = fingerprints[relative]
    limit = launch.MAX_CUE if kind == 2 else launch.MAX_ROM
    if not 0 < fp["size"] <= limit:
        raise ValueError("Game size is outside supported media bounds")
    if kind == 2:
        one = build_catalog.make_records([{"title": "Validation", "launch_path": relative,
                                         "genre": "Unknown", "region": ""}], {})
        full, _ = build_index.build_pack(one)
        pins = [{"path": name, **fingerprints[name]} for name in names]
        _, proof = launch.verify_cd_sources(full, root, pins)
        data_files = [r["path"] for r in proof[0]["tracks"] if r["type"] == "BINARY"]
        identity = hashlib.sha256(raw + b"\0" + b"".join(
            bytes.fromhex(fingerprints[name]["sha256"]) for name in names[1:])).hexdigest()
    else:
        data_files = [relative]
        identity = fp["sha256"]
    return {"path": relative, "kind": kind, "files": names, "data_files": data_files,
            "content_identity": identity, "fingerprint": fp}


def scan(root, progress=print, fingerprints=None):
    root = Path(root).absolute()
    if root.is_symlink() or not root.is_dir():
        raise ValueError("Choose an existing regular library directory")
    candidates = []
    skipped = []
    for directory, dirs, files in os.walk(root, followlinks=False):
        dirs[:] = sorted(d for d in dirs if d.casefold() not in IGNORED_DIRS
                         and not d.startswith((".", "_"))
                         and not (Path(directory) / d).is_symlink())
        for name in sorted(files):
            if name.startswith("."):
                continue
            p = Path(directory) / name
            relative = p.relative_to(root).as_posix()
            if p.suffix.lower() in (".pce", ".sgx", ".cue") and name.casefold() != "teopce.pce":
                candidates.append(relative)
            elif p.suffix.lower() in (".zip", ".7z", ".chd", ".ccd", ".m3u"):
                skipped.append({"path": relative, "reason": "Extract or convert to PCE / CUE + tracks first"})
    if len(candidates) > 1200:
        raise ValueError("The console supports at most 1200 catalog entries")
    games = []
    fingerprints = {} if fingerprints is None else fingerprints
    canonical = set()
    for number, relative in enumerate(sorted(candidates), 1):
        try:
            if relative.casefold() in canonical:
                raise ValueError("Case-insensitive duplicate path")
            canonical.add(relative.casefold())
            games.append(inspect_game(root, relative, fingerprints))
        except (OSError, ValueError, UnicodeError, wave.Error) as error:
            skipped.append({"path": relative, "reason": str(error)})
        progress("Checked game %d/%d" % (number, len(candidates)))
    # Native HuCard saves and carriers use basenames. Different revisions with
    # the same basename must not silently share that native namespace.
    basenames = {}
    for game in games:
        if game["kind"] == 1:
            basenames.setdefault(Path(game["path"]).name.casefold(), []).append(game)
    conflicting = {g["path"] for group in basenames.values()
                   if len(group) > 1 and len({g["content_identity"] for g in group}) > 1
                   for g in group}
    for path in sorted(conflicting):
        skipped.append({"path": path, "reason": "Different HuCards share a native save basename; rename explicitly"})
    games = [g for g in games if g["path"] not in conflicting]
    if not games:
        raise ValueError("No supported games passed validation")
    return games, fingerprints, skipped


def copy_verified(source, destination, expected):
    regular(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    total = 0
    with source.open("rb") as src, destination.open("xb") as dst:
        for block in iter(lambda: src.read(1024 * 1024), b""):
            total += len(block)
            digest.update(block)
            dst.write(block)
    if total != expected["size"] or digest.hexdigest() != expected["sha256"]:
        raise ValueError("Source changed during copy: " + str(source))
