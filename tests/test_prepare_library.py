"""Exercise portable preparation through its public command-line entry point."""
from io import BytesIO
import importlib.util
import shutil
import zlib
from PIL import Image
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LibraryPreparationTests(unittest.TestCase):
    def run_builder(self, library, output, *extra):
        return subprocess.run([sys.executable, "-B", str(ROOT / "tools/prepare_library.py"),
                               "--library", str(library), "--output", str(output),
                               "--offline", *map(str, extra)], capture_output=True, text=True)

    def check_native_readers(self, card, base):
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("C compiler unavailable for native reader integration")
        exe = base / "reader"
        modules = ["catalog", "find", "recent", "preferences", "pack", "card_io",
                   "launch", "game_meta", "cheat_index"]
        command = [compiler, "-std=c99", '-DTEO_APP_ROOT="00TEOPCE"',
                   "-Wno-unknown-pragmas", "-I", str(ROOT / "rom"),
                   "-I", str(ROOT / "vendor/krikzz"), str(ROOT / "tests/library_card_reader.c")]
        command += [str(ROOT / "rom" / (name + ".c")) for name in modules]
        result = subprocess.run(command + ["-o", str(exe)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = subprocess.run([str(exe), str(card)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("visible 3", result.stdout)
        self.assertIn("category 1 2", result.stdout)
        self.assertIn("category 2 1", result.stdout)
        self.assertIn("launch-preflight 3", result.stdout)

    def fixture(self, base):
        library = base / "My collection"
        library.mkdir()
        blobs = {
            "Cartridges/Unknown.pce": b"own unknown cartridge" * 100,
            "Translations/English adventure.pce": b"own translated cartridge" * 100,
            "CD/Good/disc.cue": b'FILE "track.bin" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n',
            "CD/Good/track.bin": bytes(2352 * 2),
            "CD/Broken/disc.cue": b'FILE "absent.bin" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n',
        }
        for relative, data in blobs.items():
            p = library / relative
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)
        return library, blobs

    def test_mixed_collection_repeat_and_bad_cue_report(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, blobs = self.fixture(base)
            output = base / "prepared"
            result = self.run_builder(library, output)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((output / "REPORT.json").read_text())
            self.assertEqual(report["games"], 3)
            self.assertEqual(len(report["skipped"]), 1)
            card = output / "card"
            self.check_native_readers(card, base)
            self.assertFalse((card / "CD/Broken/disc.cue").exists())
            for relative, data in blobs.items():
                self.assertEqual((library / relative).read_bytes(), data)
            self.assertTrue((card / "00TEOPCE/data/art/000000-c.bin").is_file())
            user = card / "00TEOPCE/user"
            user.mkdir(exist_ok=True)
            (user / "prefs0.bin").write_bytes(b"existing preferences")
            native = card / "edturbo/gamedata/disc.cue"
            native.mkdir(parents=True)
            (native / "bram_exp.brm").write_bytes(b"existing native save")
            (native / "cheats.txt").write_bytes(b"native cheat carrier")
            second = base / "second"
            result = self.run_builder(library, second, "--previous", card)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((second / "card/00TEOPCE/user/prefs0.bin").read_bytes(), b"existing preferences")
            self.assertEqual((second / "card/edturbo/gamedata/disc.cue/bram_exp.brm").read_bytes(), b"existing native save")
            self.assertEqual((second / "card/edturbo/gamedata/disc.cue/cheats.txt").read_bytes(), b"native cheat carrier")
            for name in ["catalog2.bin", "find.bin", "launch.bin", "years.bin", "cheats.idx"]:
                self.assertEqual((card / "00TEOPCE/data" / name).read_bytes(),
                                 (second / "card/00TEOPCE/data" / name).read_bytes())
            self.assertNotEqual(self.run_builder(library, second).returncode, 0)

    def test_hash_metadata_custom_images_and_exact_reference_cheats(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, blobs = self.fixture(base)
            cache = base / "cache"
            cache.mkdir()
            revision = "969e31a324119a17add184e8b716315a74adb51b"
            family = "NEC%20-%20PC%20Engine%20-%20TurboGrafx%2016.dat"
            rom = blobs["Cartridges/Unknown.pce"]
            crc = zlib.crc32(rom)
            sha1 = hashlib.sha1(rom).hexdigest()
            raw = ('game (\n name "Known (Japan)"\n rom ( name "Known (Japan).pce" size %d crc %08X sha1 %s )\n)\n' % (len(rom), crc, sha1)).encode()
            def cached(url, payload):
                key = hashlib.sha256(url.encode()).hexdigest()
                (cache / key).write_bytes(payload)
                (cache / (key + ".sha256")).write_text(hashlib.sha256(payload).hexdigest())
            prefix = "https://raw.githubusercontent.com/libretro/libretro-database/" + revision + "/metadat/"
            cached(prefix + "no-intro/" + family, raw)
            for field, value in [("genre", "Puzzle"), ("releaseyear", "1993")]:
                cached(prefix + field + "/" + family,
                       ('game (\n comment "Known (Japan)"\n %s "%s"\n rom ( crc %08X )\n)\n' % (field, value, crc)).encode())
            image = base / "cover.png"
            Image.new("RGB", (160, 220), (180, 40, 60)).save(image)
            metadata = base / "metadata.json"
            metadata.write_text(json.dumps({"version": 1, "games": {
                "Translations/English adventure.pce": {"year": 2025, "category": "translations",
                    "description": "A user-provided description.", "cover": "cover.png"}}}))
            first = base / "first"
            result = self.run_builder(library, first, "--metadata", metadata, "--cache", cache)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((first / "REPORT.json").read_text())
            known = next(e for e in report["entries"] if e["path"] == "Cartridges/Unknown.pce")
            self.assertEqual((known["title"], known["year"], known["match"]), ("Known (Japan)", 1993, "sha1+size"))
            self.assertEqual(report["art"]["user image"], 1)
            spec = importlib.util.spec_from_file_location("test_library_codec", ROOT / "tools/build_cheat_groups.py")
            codec = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(codec)
            key = int(known["key"], 16)
            package = codec.encode_package(key, len(rom), crc, [{"id": 123, "name": "Owned test code",
                "codes": [(0xF82000, 9, 2)], "reason": 0}])
            pack = first / "card/00TEOPCE/cheats" / ("%08X.tcg" % key)
            pack.write_bytes(package)
            second = base / "second"
            result = self.run_builder(library, second, "--reference", first / "card", "--cache", cache)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((second / "card/00TEOPCE/cheats" / pack.name).read_bytes(), package)
            self.assertEqual(json.loads((second / "REPORT.json").read_text())["cheat_packages"], 1)
            self.check_native_readers(second / "card", base)
            # Same filename with different content cannot import the old cheat package.
            (library / "Cartridges/Unknown.pce").write_bytes(b"different version" * 100)
            third = base / "third"
            result = self.run_builder(library, third, "--reference", first / "card", "--cache", cache)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads((third / "REPORT.json").read_text())["cheat_packages"], 0)

    def test_append_preserves_indices_and_missing_previous_game_stops(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, _ = self.fixture(base)
            first = base / "first"
            self.assertEqual(self.run_builder(library, first).returncode, 0)
            before = (first / "card/00TEOPCE/data/catalog2.bin").read_bytes()
            (library / "A newly added.pce").write_bytes(b"owned new game" * 100)
            second = base / "second"
            result = self.run_builder(library, second, "--previous", first / "card")
            self.assertEqual(result.returncode, 0, result.stderr)
            after = (second / "card/00TEOPCE/data/catalog2.bin").read_bytes()
            self.assertEqual(before[16:], after[16:len(before)])
            self.assertEqual(struct.unpack_from("<H", after, 6)[0], 4)
            (library / "Cartridges/Unknown.pce").unlink()
            result = self.run_builder(library, base / "third", "--previous", second / "card")
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((base / "third").exists())

    def test_raw_sector_requirement_symlink_and_basename_collision(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, _ = self.fixture(base)
            bad = library / "CD/ISO"
            bad.mkdir()
            (bad / "disc.cue").write_text('FILE "track.iso" BINARY\n TRACK 01 MODE1/2048\n INDEX 01 00:00:00\n')
            (bad / "track.iso").write_bytes(bytes(2048))
            for folder, data in [("Duplicate A", b"version one"), ("Duplicate B", b"version two")]:
                p = library / folder / "same.pce"
                p.parent.mkdir()
                p.write_bytes(data)
            link = library / "symlink.pce"
            try:
                link.symlink_to(library / "Cartridges/Unknown.pce")
            except OSError:
                pass
            output = base / "result"
            result = self.run_builder(library, output)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((output / "REPORT.json").read_text())
            self.assertEqual(report["games"], 3)
            skipped = {r["path"]: r["reason"] for r in report["skipped"]}
            self.assertIn("MODE1/2048", skipped["CD/ISO/disc.cue"])
            self.assertIn("basename", skipped["Duplicate A/same.pce"])
            if link.is_symlink():
                self.assertIn("Symbolic", skipped["symlink.pce"])

    def test_data_only_install_backup_rollback_and_changed_card_rejection(self):
        import importlib
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, blobs = self.fixture(base)
            (library / "edturbo").mkdir()
            (library / "edturbo/settings.dat").write_bytes(b"native settings")
            output = base / "data-only"
            result = self.run_builder(library, output, "--data-only")
            self.assertEqual(result.returncode, 0, result.stderr)
            card = output / "card"
            self.assertFalse((card / "Cartridges/Unknown.pce").exists())
            sys.path.insert(0, str(ROOT / "tools"))
            try:
                installer = importlib.import_module("library_install")
                result = installer.install(card, library, base / "backups")
                self.assertEqual(result["status"], "INSTALLED_VERIFIED")
                self.assertEqual((library / "edturbo/settings.dat").read_bytes(), b"native settings")
                before = {p.relative_to(library).as_posix(): p.read_bytes()
                          for p in library.rglob("*") if p.is_file()}
                next_output = base / "next"
                result = self.run_builder(library, next_output, "--data-only")
                self.assertEqual(result.returncode, 0, result.stderr)
                original_rename = installer.os.rename
                def fail_application_promotion(source, target):
                    if Path(source).name == "00TEOPCE" and Path(source).parent.name.startswith(".teopce-stage-"):
                        raise OSError("injected promotion failure")
                    return original_rename(source, target)
                with patch.object(installer.os, "rename", side_effect=fail_application_promotion):
                    with self.assertRaises(OSError):
                        installer.install(next_output / "card", library, base / "backups")
                after = {p.relative_to(library).as_posix(): p.read_bytes()
                         for p in library.rglob("*") if p.is_file()}
                self.assertEqual(before, after)
                (library / "Cartridges/Unknown.pce").write_bytes(b"changed since preparation")
                with self.assertRaisesRegex(ValueError, "changed"):
                    installer.install(next_output / "card", library, base / "backups")
            finally:
                sys.path.remove(str(ROOT / "tools"))

    def test_existing_identity_change_requires_explicit_migration(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, _ = self.fixture(base)
            first = base / "first"
            self.assertEqual(self.run_builder(library, first).returncode, 0)
            (library / "Cartridges/Unknown.pce").write_bytes(b"changed revision")
            output = base / "second"
            result = self.run_builder(library, output, "--previous", first / "card")
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())
            self.assertIn("identity", result.stderr.lower())


if __name__ == "__main__":
    unittest.main()
