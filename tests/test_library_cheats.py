"""Content-bound cheat acquisition through the public builder."""
import hashlib
import importlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from test_prepare_library import LibraryPreparationTests
import build_cheat_groups as codec


class CheatLibraryTests(unittest.TestCase):
    fixture = LibraryPreparationTests.fixture
    run_builder = LibraryPreparationTests.run_builder
    def record(self, game, kind=1, path="old/game.pce", required_path=False):
        return {"title": "Test game", "kind": kind, "path": path,
                "content_identity": hashlib.sha256(game).hexdigest(),
                "size": len(game), "crc32": zlib.crc32(game), "requires_path": required_path,
                "sources": ["https://example.org/cheats/test"],
                "groups": [{"id": 123, "name": "Infinite lives",
                            "codes": [[0xF82000, 9, 2]], "reason": 0}]}

    def database(self, base, entries):
        path = base / "cheats.json"
        path.write_text(json.dumps({"version": 1, "entries": entries}))
        return path

    def test_builder_acquires_relocated_hucard_and_preserves_its_package(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, blobs = self.fixture(base)
            raw = blobs["Cartridges/Unknown.pce"]
            db = self.database(base, [self.record(raw)])
            first = base / "first"
            result = self.run_builder(library, first, "--cheat-database", db)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((first / "REPORT.json").read_text())
            entry = next(x for x in report["entries"] if x["path"] == "Cartridges/Unknown.pce")
            self.assertEqual((entry["cheat_groups"], entry["cheat_held"]), (1, 0))
            pack = first / "card/00TEOPCE/cheats" / (entry["key"] + ".tcg")
            expected = pack.read_bytes()
            self.assertEqual(codec.decode_package(expected, int(entry["key"], 16), len(raw), zlib.crc32(raw))[0]["id"], 123)
            # A later database update must not invalidate saved selections.
            document = json.loads(db.read_text())
            document["entries"][0]["groups"][0]["codes"][0][1] = 10
            db.write_text(json.dumps(document))
            metadata = base / "metadata.json"
            metadata.write_text(json.dumps({"version": 1, "games": {"Cartridges/Unknown.pce": {"title": "Edited title", "year": 1999}}}))
            second = base / "second"
            result = self.run_builder(library, second, "--previous", first / "card", "--cheat-database", db, "--metadata", metadata)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((second / "card/00TEOPCE/cheats" / pack.name).read_bytes(), expected)
            changed = next(e for e in json.loads((second / "REPORT.json").read_text())["entries"] if e["path"] == entry["path"])
            self.assertEqual((changed["index"], changed["title"], changed["year"]), (entry["index"], "Edited title", 1999))
            (library / "Cartridges/Unknown.pce").write_bytes(b"different revision")
            third = base / "third"
            self.assertEqual(self.run_builder(library, third, "--cheat-database", db).returncode, 0)
            self.assertEqual(json.loads((third / "REPORT.json").read_text())["cheat_packages"], 0)

    def test_changed_bundled_license_stops_card_preparation(self):
        from contextlib import redirect_stdout
        from io import StringIO
        import shutil
        from types import SimpleNamespace
        from unittest.mock import patch
        import prepare_library as builder
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, _ = self.fixture(base)
            shadow = base / "source"
            shutil.copytree(ROOT / "resources", shadow / "resources")
            (shadow / "resources/cheats/LICENSE-CC-BY-SA-4.0.txt").write_bytes(b"changed license")
            args = SimpleNamespace(library=library, output=base / "output", previous=None,
                                   reference=None, cache=base / "cache", rom=None, metadata=None,
                                   offline=True, data_only=True, cheat_database=None)
            with patch.object(builder, "ROOT", shadow), redirect_stdout(StringIO()):
                with self.assertRaisesRegex(ValueError, "license checksum"):
                    builder.prepare(args)
            self.assertFalse(args.output.exists())

    def test_cd_policy_requires_recorded_path_and_complete_disc_identity(self):
        module = importlib.import_module("library_cheats")
        raw = b'FILE "track.bin" BINARY\n'
        record = self.record(raw, 2, "CD/Protected/disc.cue", True)
        with tempfile.TemporaryDirectory() as td:
            db = module.Database([self.database(Path(td), [record])])
            game = {"kind": 2, "path": "Other/disc.cue", "content_identity": record["content_identity"],
                    "fingerprint": {"size": len(raw), "crc32": zlib.crc32(raw)}}
            row = {"key": 111}
            package, reason, sources = db.package(row, game)
            self.assertIsNone(package)
            self.assertIn(record["path"], reason)
            game["path"] = record["path"]
            self.assertIsNotNone(db.package(row, game)[0])
            game["content_identity"] = hashlib.sha256(b"changed audio track").hexdigest()
            self.assertIsNone(db.package(row, game)[0])

    def test_explicit_user_library_can_supply_more_groups_than_the_bundled_subset(self):
        import library_cheats
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            record = self.record(b"exact game")
            bundled = self.database(base, [record])
            extended = json.loads(json.dumps(record))
            extended["groups"].append({"id": 456, "name": "Second owned code", "codes": [[0xF82001, 2, 2]], "reason": 0})
            custom = base / "custom.json"
            custom.write_text(json.dumps({"version": 1, "entries": [extended]}))
            database = library_cheats.Database([bundled], preferred=custom)
            self.assertEqual(len(database.entries[1, record["content_identity"]]["groups"]), 2)
            with self.assertRaisesRegex(ValueError, "Conflicting"):
                library_cheats.Database([bundled, custom])

    def test_card_carries_attribution_when_a_later_library_has_no_matching_group(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, blobs = self.fixture(base)
            record = self.record(blobs["Cartridges/Unknown.pce"])
            db = self.database(base, [record])
            document = json.loads(db.read_text())
            document.update(data_license="CC-BY-SA-4.0", attribution={"project": "Fixture authors"})
            db.write_text(json.dumps(document))
            first = base / "first"
            result = self.run_builder(library, first, "--cheat-database", db)
            self.assertEqual(result.returncode, 0, result.stderr)
            data = first / "card/00TEOPCE/data"
            self.assertTrue((data / "CC-BY-SA-4.0.txt").is_file())
            ledger = json.loads((data / "CHEAT-SOURCES.json").read_text())
            item = next(iter(ledger["packages"].values()))
            self.assertEqual(item["sources"], ["https://example.org/cheats/test"])
            self.assertTrue(any(source.get("data_license") == "CC-BY-SA-4.0" for source in item["libraries"]))
            db.write_text(json.dumps({"version": 1, "entries": []}))
            second = base / "second"
            result = self.run_builder(library, second, "--previous", first / "card", "--cheat-database", db)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads((second / "card/00TEOPCE/data/CHEAT-SOURCES.json").read_text()), ledger)

    def test_same_card_update_hashes_each_game_file_once(self):
        from collections import Counter
        from contextlib import redirect_stdout
        from io import StringIO
        from types import SimpleNamespace
        from unittest.mock import patch
        import prepare_library as builder
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, blobs = self.fixture(base)
            first = base / "first"
            self.assertEqual(self.run_builder(library, first).returncode, 0)
            card = (first / "card").resolve()
            names = {name for name in blobs if "Broken" not in name}
            observed = Counter()
            fingerprint = builder.media.fingerprint
            def record(path):
                path = Path(path)
                if path.is_relative_to(card) and path.relative_to(card).as_posix() in names:
                    observed[path.relative_to(card).as_posix()] += 1
                return fingerprint(path)
            args = SimpleNamespace(library=card, output=base / "second", previous=None,
                                   reference=None, cache=base / "cache", rom=None, metadata=None,
                                   offline=True, data_only=True, cheat_database=None)
            with patch.object(builder.media, "fingerprint", side_effect=record), redirect_stdout(StringIO()):
                builder.prepare(args)
            self.assertEqual(observed, Counter({name: 1 for name in names}))

    def test_install_rejects_games_added_after_preparation(self):
        import library_install
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, _ = self.fixture(base)
            (library / "edturbo").mkdir()
            output = base / "prepared"
            self.assertEqual(self.run_builder(library, output, "--data-only").returncode, 0)
            (library / "New game.pce").write_bytes(b"new owned game")
            with self.assertRaisesRegex(ValueError, "game list changed"):
                library_install.install(output / "card", library, base / "backups")
            self.assertFalse((library / "TeoPCE.pce").exists())
            (library / "New game.pce").unlink()
            # An earlier skipped CUE can become valid when its track arrives.
            (library / "CD/Broken/absent.bin").write_bytes(bytes(2352 * 2))
            with self.assertRaisesRegex(ValueError, "game list changed"):
                library_install.install(output / "card", library, base / "backups")

    def test_export_rejects_cd_tracks_differing_from_complete_reference(self):
        import export_cheat_library as exporter
        import prepare_library as builder
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, blobs = self.fixture(base)
            first = base / "reference"
            self.assertEqual(self.run_builder(library, first).returncode, 0)
            card = first / "card"
            rows, identities = builder.launch_rows(card, builder.catalog_at(card))
            row = next(r for r in rows if r["kind"] == 2)
            raw = codec.encode_package(row["key"], *identities[row["path"]],
                [{"id": 1, "name": "Test", "codes": [(0xF82000, 3, 2)], "reason": 0}])
            (card / "00TEOPCE/cheats" / ("%08X.tcg" % row["key"])).write_bytes(raw)
            (card / "00TEOPCE/data/CD-CHEAT-PROVENANCE.json").write_text(json.dumps({"prepared": [
                {"path": row["path"], "source_url": "https://example.org/test"}]}))
            (library / "CD/Good/track.bin").write_bytes(b"x" * len(blobs["CD/Good/track.bin"]))
            report = exporter.export(card, library, base / "cheats.json")
            self.assertEqual(report["entries"], 0)
            self.assertIn("reference", report["excluded"][0]["reason"])

    def test_invalid_and_conflicting_database_stop_before_output(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            library, blobs = self.fixture(base)
            entry = self.record(blobs["Cartridges/Unknown.pce"])
            duplicate = json.loads(json.dumps(entry))
            duplicate["groups"][0]["codes"][0][1] = 15
            for entries in [[entry, duplicate], [{**entry, "groups": [{"bad": "schema"}]}]]:
                db = self.database(base, entries)
                result = self.run_builder(library, base / "output", "--cheat-database", db)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((base / "output").exists())


del LibraryPreparationTests

if __name__ == "__main__":
    unittest.main()
