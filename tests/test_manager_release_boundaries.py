"""Regressions reproduced by independent release QA."""
import json
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import prepare_library as builder
import library_install
from PIL import Image


class ReleaseBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = Path(self.temp.name)
        self.library = self.base / "games"
        self.library.mkdir()
        (self.library / "Alpha.pce").write_bytes(b"test cartridge" * 100)
        (self.library / "edturbo").mkdir()

    def tearDown(self):
        self.temp.cleanup()

    def args(self, name, **changes):
        values = dict(library=self.library, output=self.base / name,
                      previous=None, reference=None, rom=None, metadata=None,
                      cache=self.base / "cache", offline=True, data_only=False,
                      cheat_database=None)
        values.update(changes)
        return SimpleNamespace(**values)

    def test_first_install_creates_empty_stock_game_directory_and_refresh_preserves_it(self):
        config = self.args("first", data_only=True)
        builder.prepare(config)
        backups = self.base / "backups"
        library_install.install(config.output / "card", self.library, backups)
        directory = self.library / "edturbo/gamedata"
        self.assertTrue(directory.is_dir())
        self.assertEqual(list(directory.iterdir()), [])
        save = directory / "keep.brm"
        save.write_bytes(b"native save")
        update = self.args("update", data_only=True)
        builder.prepare(update)
        library_install.install(update.output / "card", self.library, backups)
        self.assertEqual(save.read_bytes(), b"native save")

    def test_install_refuses_file_in_place_of_stock_directory(self):
        config = self.args("first", data_only=True)
        builder.prepare(config)
        path = self.library / "edturbo/gamedata"
        path.write_bytes(b"preserve")
        with self.assertRaisesRegex(ValueError, "gamedata"):
            library_install.install(config.output / "card", self.library, self.base / "backups")
        self.assertEqual(path.read_bytes(), b"preserve")
        self.assertFalse((self.library / "TeoPCE.pce").exists())

    def test_cover_only_override_preserves_curated_metadata_and_description(self):
        metadata = self.base / "metadata.json"
        metadata.write_text(json.dumps({"version": 1, "games": {"Alpha.pce": {
            "title": "My Curated Title", "year": 1993, "genre": "Action",
            "region": "Japan", "category": "homebrew", "description": "My own description."}}}))
        first = self.args("first", metadata=metadata)
        builder.prepare(first)
        Image.new("RGB", (48, 64), "red").save(self.base / "cover.png")
        metadata.write_text(json.dumps({"version": 1, "games": {"Alpha.pce": {"cover": "cover.png"}}}))
        second = self.args("second", previous=first.output / "card", metadata=metadata)
        report = builder.prepare(second)
        self.assertEqual((report["entries"][0]["title"], report["entries"][0]["year"]), ("My Curated Title", 1993))
        self.assertEqual(report["entries"][0]["category"], 3)
        old = first.output / "card/00TEOPCE/data/desc/000000.txt"
        new = second.output / "card/00TEOPCE/data/desc/000000.txt"
        self.assertEqual(old.read_bytes(), new.read_bytes())

    def test_cache_cannot_write_into_previous_card(self):
        first = self.args("first")
        builder.prepare(first)
        previous = first.output / "card"
        cache = previous / "nested-cache"
        with self.assertRaisesRegex(ValueError, "Cache must"):
            builder.prepare(self.args("second", previous=previous, cache=cache))
        self.assertFalse(cache.exists())

    def test_backup_cannot_be_inside_prepared_package(self):
        first = self.args("first")
        builder.prepare(first)
        backups = first.output / "card/00TEOPCE/backups"
        with self.assertRaisesRegex(ValueError, "Backups must"):
            library_install.install(first.output / "card", self.library, backups)
        self.assertFalse(backups.exists())
        self.assertFalse((self.library / "TeoPCE.pce").exists())

    def test_first_install_package_cannot_replace_a_newer_profile(self):
        old = self.args("old", data_only=True)
        current = self.args("current", data_only=True)
        for config in (old, current):
            builder.prepare(config)
        backups = self.base / "backups"
        library_install.install(current.output / "card", self.library, backups)
        profile = self.library / "00TEOPCE/user/prefs0.bin"
        profile.write_bytes(b"settings saved since preparation")
        with self.assertRaisesRegex(ValueError, "[Pp]repare again"):
            library_install.install(old.output / "card", self.library, backups)
        self.assertEqual(profile.read_bytes(), b"settings saved since preparation")
        self.assertEqual(len(list(backups.iterdir())), 1)
        update = self.args("update", data_only=True)
        builder.prepare(update)
        library_install.install(update.output / "card", self.library, backups)
        self.assertEqual(profile.read_bytes(), b"settings saved since preparation")

    def test_first_install_preserves_partial_data_and_unrecognized_rom(self):
        config = self.args("first", data_only=True)
        builder.prepare(config)
        prepared = config.output / "card"
        backups = self.base / "backups"
        rom = self.library / "TeoPCE.pce"
        rom.write_bytes(b"previous launcher")
        with self.assertRaisesRegex(ValueError, "[Bb]ack up"):
            library_install.install(prepared, self.library, backups)
        self.assertEqual(rom.read_bytes(), b"previous launcher")
        rom.write_bytes((prepared / "TeoPCE.pce").read_bytes())
        data = self.library / "00TEOPCE"
        data.mkdir()
        (data / "kept.txt").write_bytes(b"previous setup")
        with self.assertRaisesRegex(ValueError, "[Bb]ack up"):
            library_install.install(prepared, self.library, backups)
        self.assertEqual((data / "kept.txt").read_bytes(), b"previous setup")
        self.assertFalse(backups.exists())
        data.rename(self.base / "saved partial setup")
        result = library_install.install(prepared, self.library, backups)
        self.assertEqual(result["status"], "INSTALLED_VERIFIED")

    def test_manager_refresh_leaves_native_saves_only_on_the_card(self):
        first = self.args("first", data_only=True)
        builder.prepare(first)
        backups = self.base / "backups"
        library_install.install(first.output / "card", self.library, backups)
        native = self.library / "edturbo/gamedata/disc.cue/bram_exp.brm"
        native.parent.mkdir(parents=True)
        native.write_bytes(b"existing CD save")
        update = self.args("update", data_only=True)
        builder.prepare(update)
        self.assertFalse((update.output / "card/edturbo").exists())
        library_install.install(update.output / "card", self.library, backups)
        self.assertEqual(native.read_bytes(), b"existing CD save")

    def test_nested_symlink_is_rejected_before_copying_external_data(self):
        first = self.args("first")
        builder.prepare(first)
        private = self.base / "private"
        private.mkdir()
        (private / "sentinel.txt").write_text("never copy me")
        user = self.library / "00TEOPCE/user"
        user.mkdir(parents=True)
        try:
            (user / "outside").symlink_to(private, target_is_directory=True)
        except OSError:
            self.skipTest("Creating symlinks is unavailable")
        backups = self.base / "backups"
        with self.assertRaisesRegex(ValueError, "symbolic links"):
            library_install.install(first.output / "card", self.library, backups)
        self.assertFalse(list(backups.rglob("sentinel.txt")))
        self.assertFalse((self.library / "TeoPCE.pce").exists())

    def test_progress_is_factual_and_offline_art_is_reported_missing(self):
        events = []
        report = builder.prepare(self.args("first"), progress=events.append)
        self.assertIn("Reading games", events[0])
        self.assertTrue(any("Cover" in e and "Alpha" in e for e in events))
        self.assertTrue(any("offline" in e.lower() for e in events))
        self.assertEqual(report["entries"][0]["art"]["c"]["source"], "placeholder")
        self.assertTrue(any("Verifying" in e for e in events))

    def test_card_verification_ignores_mac_sidecars_but_rejects_unknown_codes(self):
        args = self.args("first", data_only=True)
        builder.prepare(args)
        card = args.output / "card"
        cheats = card / "00TEOPCE/cheats"
        (cheats / "._DEADBEEF.tcg").write_bytes(b"AppleDouble metadata")
        builder.verify_result(card)
        (cheats / "DEADBEEF.tcg").write_bytes(b"foreign package")
        with self.assertRaisesRegex(ValueError, "unknown package key"):
            builder.verify_result(card)

    def test_card_staging_does_not_require_host_file_flags(self):
        from unittest.mock import patch
        args = self.args("first", data_only=True)
        builder.prepare(args)
        with patch.object(library_install.shutil, "copystat", side_effect=OSError(22, "unsupported flags")):
            result = library_install.install(args.output / "card", self.library, self.base / "backups")
        self.assertEqual(result["status"], "INSTALLED_VERIFIED")
        self.assertTrue((self.library / "00TEOPCE/user/cc").is_dir())
        self.assertTrue((self.library / "00TEOPCE/user/cd").is_dir())

    def test_publisher_only_update_retains_prose(self):
        metadata = self.base / "metadata.json"
        metadata.write_text(json.dumps({"version": 1, "games": {"Alpha.pce": {
            "description": "My original story.", "publisher": "Old Press"}}}))
        first = self.args("first", metadata=metadata)
        builder.prepare(first)
        metadata.write_text(json.dumps({"version": 1, "games": {"Alpha.pce": {"publisher": "New Press"}}}))
        second = self.args("second", previous=first.output / "card", metadata=metadata)
        report = builder.prepare(second)
        self.assertEqual(report["entries"][0]["editable_description"], "My original story.")
        text = (second.output / "card/00TEOPCE/data/desc/000000.txt").read_text().replace("\n", " ")
        self.assertIn("My original story.", text)
        self.assertIn("New Press", text)
        self.assertNotIn("Old Press", text)

    def test_legacy_placeholder_is_missing_with_both_encodings(self):
        import library_sources
        from io import BytesIO
        import convert_art
        for legacy in (False, True):
            with self.subTest(legacy=legacy):
                first = self.args("first-" + str(legacy))
                builder.prepare(first)
                data = first.output / "card/00TEOPCE/data"
                (data / "ART-PROVENANCE.json").unlink()
                if legacy:
                    (data / "art/000000-c.bin").write_bytes(convert_art.convert(BytesIO(library_sources.placeholder())))
                second = self.args("second-" + str(legacy), previous=first.output / "card")
                report = builder.prepare(second)
                self.assertEqual(report["entries"][0]["art"]["c"]["source"], "placeholder")

    def test_symlink_added_during_staging_is_rejected_before_promotion(self):
        from unittest.mock import patch
        first = self.args("first")
        builder.prepare(first)
        private = self.base / "private"
        private.mkdir()
        original_copy = builder.copy_tree_checked
        def copy(source, destination, *args, **kwargs):
            if Path(source) == first.output.resolve() / "card/00TEOPCE":
                try:
                    (Path(source) / "user/late-link").symlink_to(private, target_is_directory=True)
                except OSError:
                    self.skipTest("Creating symlinks is unavailable")
            return original_copy(source, destination, *args, **kwargs)
        with patch.object(builder, "copy_tree_checked", side_effect=copy):
            with self.assertRaisesRegex(ValueError, "symbolic links"):
                library_install.install(first.output / "card", self.library, self.base / "backups")
        self.assertFalse((self.library / "TeoPCE.pce").exists())
        self.assertFalse(list(self.library.glob(".teopce-*")))


if __name__ == "__main__":
    unittest.main()
