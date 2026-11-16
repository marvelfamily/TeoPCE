"""Licensed data must preserve complete, already-bound cheat groups."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import import_libretro_cheats as importer


class LibretroImportTests(unittest.TestCase):
    def fixture(self, base, raw, title="Test (Japan)", kind=2):
        family = "NEC - PC Engine CD - TurboGrafx-CD" if kind == 2 else "NEC - PC Engine - TurboGrafx 16"
        relative = "cht/" + family + "/" + title + ".cht"
        path = base / relative
        path.parent.mkdir(parents=True)
        path.write_bytes(raw)
        url = importer.BASE_URL + quote(relative)
        (base / "LICENSE").write_bytes(b"license fixture")
        manifest = {"revision": importer.REVISION, "license": "CC-BY-SA-4.0",
                    "license_sha256": hashlib.sha256(b"license fixture").hexdigest(),
                    "files": [{"path": relative, "url": url,
                               "sha256": hashlib.sha256(raw).hexdigest(),
                               "git_blob": hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()}]}
        entry = {"title": title, "kind": kind, "path": "CD/Test/disc.cue",
                 "content_identity": "a" * 64, "size": 100, "crc32": 123,
                 "requires_path": kind == 2, "sources": ["https://example.org/original"],
                 "groups": [{"id": 42, "name": "Existing code", "codes": [[0xF82010, 9, 2], [0xF82011, 2, 2]], "reason": 2}]}
        return {"version": 1, "entries": [entry]}, manifest

    def test_complete_group_retains_binding_id_flags_and_address_form(self):
        raw = b'cheats = 1\ncheat0_desc = "Licensed group"\ncheat0_code = "1f0010:09+1f0011:02"\n'
        with tempfile.TemporaryDirectory() as td:
            reference, manifest = self.fixture(Path(td), raw)
            result, report = importer.convert(reference, Path(td), manifest)
        actual = result["entries"][0]
        self.assertEqual(actual["content_identity"], "a" * 64)
        self.assertTrue(actual["requires_path"])
        self.assertEqual(actual["groups"], [{"id": 42, "name": "Licensed group", "codes": [[0xF82010, 9, 2], [0xF82011, 2, 2]], "reason": 2}])
        self.assertEqual(report["groups"], 1)
        self.assertEqual(result["data_license"], "CC-BY-SA-4.0")

    def test_different_value_and_partial_source_do_not_supply_a_group(self):
        for code in ("1f0010:08+1f0011:02", "1f0010:09+invalid"):
            with self.subTest(code=code), tempfile.TemporaryDirectory() as td:
                raw = ('cheats = 1\ncheat0_desc = "Partial"\ncheat0_code = "' + code + '"\n').encode()
                reference, manifest = self.fixture(Path(td), raw)
                result, report = importer.convert(reference, Path(td), manifest)
                self.assertEqual(result["entries"], [])
                self.assertEqual(len(report["excluded"]), 1)

    def test_another_region_does_not_supply_matching_addresses(self):
        raw = b'cheats = 1\ncheat0_desc = "Group"\ncheat0_code = "1f0010:09+1f0011:02"\n'
        with tempfile.TemporaryDirectory() as td:
            reference, manifest = self.fixture(Path(td), raw, title="Test (USA)")
            reference["entries"][0]["title"] = "Test (Japan)"
            result, _ = importer.convert(reference, Path(td), manifest)
            self.assertEqual(result["entries"], [])

    def test_corrupt_source_stops_before_conversion(self):
        with tempfile.TemporaryDirectory() as td:
            reference, manifest = self.fixture(Path(td), b'cheats = 0\n')
            (Path(td) / manifest["files"][0]["path"]).write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, "checksum"):
                importer.convert(reference, Path(td), manifest)

    def test_changed_source_sha_cannot_override_the_recorded_git_blob(self):
        with tempfile.TemporaryDirectory() as td:
            reference, manifest = self.fixture(Path(td), b'cheats = 0\n')
            raw = b'cheats = 0\n\n'
            (Path(td) / manifest["files"][0]["path"]).write_bytes(raw)
            manifest["files"][0]["sha256"] = hashlib.sha256(raw).hexdigest()
            with self.assertRaisesRegex(ValueError, "Git blob"):
                importer.convert(reference, Path(td), manifest)

    def test_changed_license_is_rejected_during_reconstruction(self):
        with tempfile.TemporaryDirectory() as td:
            reference, manifest = self.fixture(Path(td), b'cheats = 0\n')
            (Path(td) / "LICENSE").write_bytes(b"changed license")
            with self.assertRaisesRegex(ValueError, "license checksum"):
                importer.convert(reference, Path(td), manifest)

    def test_direct_retroarch_byte_write_requires_complete_supported_fields(self):
        raw = b'cheats = 3\ncheat0_desc = "Byte"\ncheat0_handler = 1\ncheat0_cheat_type = 1\ncheat0_memory_search_size = 3\ncheat0_address = 16\ncheat0_value = 9\ncheat1_handler = 1\ncheat1_cheat_type = 1\ncheat1_memory_search_size = 3\ncheat1_address = 17\ncheat2_handler = 1\ncheat2_cheat_type = 1\ncheat2_memory_search_size = 4\ncheat2_address = 17\ncheat2_value = 2\n'
        groups = importer.parse_cheats(raw)
        self.assertEqual(groups, [{"name": "Byte", "codes": [(0x1F0010, 9)]}])

    def test_bundled_library_reconstructs_from_licensed_sources(self):
        base = ROOT / "resources/cheats"
        reference = json.loads((base / "library.json").read_text())
        self.assertEqual(reference.get("data_license"), "CC-BY-SA-4.0")
        manifest = json.loads((base / "SOURCES.json").read_text())
        result, report = importer.convert(reference, base / "upstream", manifest)
        self.assertEqual(report["excluded"], [])
        self.assertEqual(result["entries"], reference["entries"])


if __name__ == "__main__":
    unittest.main()
