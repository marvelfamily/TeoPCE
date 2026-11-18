"""Verified application-data installation with a complete host-side rollback copy."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import uuid

import library_media as media
import prepare_library


def install(prepared, target, backups):
    target = Path(target).resolve(strict=True)
    lock = target / ".teopce-install.lock"
    with lock.open("x"):
        pass
    try:
        return _install(prepared, target, backups)
    finally:
        lock.unlink(missing_ok=True)


def _install(prepared, target, backups):
    prepared = Path(prepared).resolve(strict=True)
    target = Path(target).resolve(strict=True)
    backups = Path(backups).resolve()
    if any(backups == p or p in backups.parents for p in (target, prepared)):
        raise ValueError("Backups must be outside the card and prepared package")
    for root in (target / "00TEOPCE", target / "TeoPCE.pce", prepared / "00TEOPCE"):
        if root.is_symlink() or any(p.is_symlink() for p in root.rglob("*")):
            raise ValueError("Application data must not contain symbolic links")
    if not target.is_dir() or not (target / "edturbo").is_dir():
        raise ValueError("Select a card containing the edturbo folder")
    native_data = target / "edturbo/gamedata"
    if native_data.resolve() != native_data or (native_data.exists() and not native_data.is_dir()):
        raise ValueError("edturbo/gamedata must be a directory on this card")
    prepare_library.verify_result(prepared)
    rom = media.read_file(prepared / "TeoPCE.pce", 131072)
    if hashlib.sha256(rom).hexdigest() != prepare_library.ROM_SHA256:
        raise ValueError("The prepared ROM does not match this release")
    state = json.loads(media.read_file(prepared / "00TEOPCE/data/library-state.json", 16 * 1024 * 1024))
    verified_files = {}
    for relative, expected in state["files"].items():
        actual = media.fingerprint(target / relative)
        if actual["sha256"] != expected["sha256"]:
            raise ValueError("The card changed after preparation: " + relative)
        verified_files[relative] = actual
    games, _, _ = media.scan(target, progress=lambda message: None, fingerprints=verified_files)
    if {game["path"]: game["content_identity"] for game in games} != state["games"]:
        raise ValueError("The game list changed after preparation; prepare again")
    baseline = state.get("previous_app_files")
    if baseline is None:
        # A first-install package has no profiles from this card to carry forward.
        app_data = target / "00TEOPCE"
        existing_rom = target / "TeoPCE.pce"
        if (app_data / "data/catalog2.bin").is_file():
            raise ValueError("This card already has a TeoPCE collection. Prepare again to keep its settings and profiles.")
        if app_data.exists() or (existing_rom.exists() and not (
                existing_rom.is_file() and media.fingerprint(existing_rom)["sha256"] == prepare_library.ROM_SHA256)):
            raise ValueError("Incomplete TeoPCE installation. Back up and move the existing TeoPCE files off the card, then prepare again.")
    else:
        actual = {p.relative_to(target).as_posix(): media.fingerprint(p)["sha256"]
                  for p in (target / "00TEOPCE").rglob("*") if p.is_file() and not p.name.startswith(".")}
        actual["TeoPCE.pce"] = media.fingerprint(target / "TeoPCE.pce")["sha256"]
        if actual != baseline:
            raise ValueError("Application data changed after preparation; prepare again")
    backups.mkdir(parents=True, exist_ok=True)
    backup = backups / ("installation-" + uuid.uuid4().hex)
    backup.mkdir()
    originals = ["TeoPCE.pce", "00TEOPCE"]
    present = []
    for name in originals:
        source = target / name
        if source.is_symlink():
            raise ValueError("Application paths must not be symbolic links")
        if source.is_dir():
            shutil.copytree(source, backup / name, symlinks=True)
            for p in source.rglob("*"):
                if p.is_symlink():
                    raise ValueError("Application data must not contain symbolic links")
                if p.is_file() and media.fingerprint(p)["sha256"] != media.fingerprint(backup / name / p.relative_to(source))["sha256"]:
                    raise ValueError("Backup verification failed")
            present.append(name)
        elif source.is_file():
            media.copy_verified(source, backup / name, media.fingerprint(source))
            present.append(name)
    token = uuid.uuid4().hex
    stage = target / (".teopce-stage-" + token)
    stage.mkdir()
    promoted = []
    cleanup = False
    try:
        for name in originals:
            source = prepared / name
            if source.is_dir():
                # FAT/exFAT store the console bytes, not the Mac source's file flags.
                prepare_library.copy_tree_checked(source, stage / name)
                if any(p.is_symlink() for p in (stage / name).rglob("*")):
                    raise ValueError("Application data must not contain symbolic links")
                for p in source.rglob("*"):
                    if any(part.startswith(".") for part in p.relative_to(source).parts):
                        continue
                    if p.is_file() and media.fingerprint(p)["sha256"] != media.fingerprint(stage / name / p.relative_to(source))["sha256"]:
                        raise ValueError("Staged write verification failed")
            else:
                media.copy_verified(source, stage / name, media.fingerprint(source))
        for name in originals:
            destination = target / name
            if destination.exists():
                os.rename(destination, stage / (name + ".old"))
            try:
                os.rename(stage / name, destination)
            except BaseException:
                if (stage / (name + ".old")).exists():
                    os.rename(stage / (name + ".old"), destination)
                raise
            promoted.append(name)
        prepare_library.verify_result(target)
        # The stock OS normally creates this directory after the first game.
        # Make a fresh installation usable before any per-game data exists.
        native_data.mkdir(exist_ok=True)
        result = {"status": "INSTALLED_VERIFIED", "version": prepare_library.ROM_VERSION, "backup": str(backup)}
        (backup / "RESULT.json").write_text(json.dumps(result, indent=2) + "\n")
        cleanup = True
        return result
    except BaseException:
        for name in reversed(promoted):
            destination = target / name
            if destination.is_dir():
                shutil.rmtree(destination)
            elif destination.exists():
                destination.unlink()
            if (stage / (name + ".old")).exists():
                os.rename(stage / (name + ".old"), destination)
        cleanup = True
        raise
    finally:
        if cleanup:
            shutil.rmtree(stage, ignore_errors=True)
