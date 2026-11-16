"""Desktop state regressions, run with a Python distribution containing Tk."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
try:
    import catalog_manager as ui
except ImportError:
    ui = None


@unittest.skipIf(ui is None, "Tk is unavailable in this interpreter")
class ManagerStateTests(unittest.TestCase):
    def setUp(self):
        self.root = ui.tk.Tk()
        self.root.withdraw()
        self.manager = ui.Manager(self.root, "/tmp/teopce-test-card")

    def tearDown(self):
        self.root.destroy()

    def test_preview_uses_original_cached_art_instead_of_console_thumbnail(self):
        import hashlib
        import json
        from PIL import Image
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            data = base / "card/00TEOPCE/data"
            data.mkdir(parents=True)
            url = "https://example.org/cover.png"
            (data / "ART-PROVENANCE.json").write_text(json.dumps([{"index": 0, "image": "c", "url": url}]))
            cache = base / "cache"
            cache.mkdir()
            path = cache / hashlib.sha256(url.encode()).hexdigest()
            Image.new("RGB", (320, 480), (200, 30, 40)).save(path, format="PNG")
            path.with_name(path.name + ".sha256").write_text(hashlib.sha256(path.read_bytes()).hexdigest())
            image = ui.preview_image(base, 0, cache)
            self.assertEqual(image.size, (128, 192))
            self.assertEqual(image.getpixel((64, 96)), (200, 30, 40))
            # The desktop must preview the image used by this preparation, not a later cache replacement.
            (data / "ART-PROVENANCE.json").write_text(json.dumps([{"index": 0, "image": "c", "url": url,
                "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest()}]))
            Image.new("RGB", (320, 480), "green").save(path, format="PNG")
            path.with_name(path.name + ".sha256").write_text(hashlib.sha256(path.read_bytes()).hexdigest())
            with patch.object(ui, "decode_art", return_value=Image.new("RGB", (96, 72), "blue")) as fallback:
                image = ui.preview_image(base, 0, cache)
            fallback.assert_called_once()
            self.assertEqual(image.getpixel((64, 96)), (0, 0, 255))

    def test_both_panes_are_visible_after_initial_layout(self):
        # Slow mapping must not consume a timer-based split before panes exist.
        self.root.after(80, self.root.quit)
        self.root.mainloop()
        self.root.deiconify()
        self.root.update_idletasks()
        self.root.update()
        body = self.manager.body
        left, right = (self.root.nametowidget(name) for name in body.panes())
        self.assertGreater(left.winfo_width(), body.winfo_width() * 0.4)
        self.assertGreater(right.winfo_width(), body.winfo_width() * 0.3)

    def test_typing_metadata_disables_install_before_save(self):
        self.manager.prepared = Path("/tmp/prepared")
        self.manager.edit_entry = {"index": 0, "path": "game.pce"}
        self.manager.title.set("Unsaved new title")
        self.assertTrue(self.manager.dirty)
        self.assertTrue(self.manager.install_button.instate(["disabled"]))

    def test_dirty_catalog_cannot_install_even_through_direct_command(self):
        self.manager.prepared = Path("/tmp/prepared")
        self.manager.dirty = True
        with patch.object(ui.messagebox, "askokcancel") as confirm:
            self.manager.install()
            confirm.assert_not_called()

    def test_search_matches_title_path_and_media(self):
        self.manager.report = {"entries": [
            {"index": 0, "title": "1943 Kai", "year": 1991, "path": "Japan/1943.pce", "cheat_groups": 2},
            {"index": 1, "title": "Monster Lair", "year": 1990, "path": "CD/Monster/disc.cue", "cheat_groups": 1}]}
        self.manager.search.set("monster")
        self.manager.refresh_games()
        self.assertEqual(self.manager.games.get_children(), ("1",))
        self.manager.search.set("Japan")
        self.manager.refresh_games()
        self.assertEqual(self.manager.games.get_children(), ("0",))

    def test_editing_metadata_marks_installed_generation_stale(self):
        self.manager.report = {"entries": [{"index": 0, "title": "Game", "year": 0,
            "category": 1, "path": "game.pce", "cheat_groups": 0}]}
        self.manager.refresh_games()
        self.manager.games.selection_set("0")
        self.manager.title.set("Edited game")
        self.manager.year.set("")
        self.manager.category.set("original")
        self.manager.prepared = Path("/tmp/prepared")
        with tempfile.TemporaryDirectory() as td:
            with patch.object(self.manager, "overrides_path", return_value=Path(td) / "meta.json"):
                self.assertTrue(self.manager.save_edits())
        self.assertTrue(self.manager.dirty)
        self.manager.set_busy(False)
        self.assertTrue(self.manager.install_button.instate(["disabled"]))

    def test_card_change_clears_previous_game_and_coverage(self):
        self.manager.report = {"entries": [{"index": 0, "title": "Old game", "path": "Old.pce", "year": 0}]}
        self.manager.game_heading.set("Old game")
        self.manager.game_path.set("Old.pce")
        self.manager.card.set("/different-card")
        self.assertIsNone(self.manager.report)
        self.assertEqual(self.manager.game_heading.get(), "Select a game")
        self.assertEqual(self.manager.game_path.get(), "")
        self.assertEqual(self.manager.cover_summary.get(), "—")

    def test_missing_covers_filter_does_not_call_placeholder_a_cover(self):
        self.manager.report = {"entries": [
            {"index": 0, "title": "Missing", "path": "a.pce", "year": 0, "art": {"c": {"source": "placeholder"}}},
            {"index": 1, "title": "Found", "path": "b.pce", "year": 1991, "art": {"c": {"source": "user image"}}}]}
        self.manager.filter.set("Missing covers")
        self.assertEqual(self.manager.games.get_children(), ("0",))
        self.assertEqual(self.manager.cover_summary.get(), "1 / 2")

    def test_progress_events_are_visible_without_marking_catalog_ready(self):
        self.manager.set_busy(True)
        self.manager.events.put(("progress", "Downloading · Cover · Game"))
        self.manager.poll()
        self.assertIn("Downloading", self.manager.activity_detail.get())
        self.assertIn("Cover · Game", self.manager.activity_log.get("1.0", "end"))
        self.assertTrue(self.manager.install_button.instate(["disabled"]))

    def test_prepare_edit_refresh_and_install_through_manager(self):
        import time
        import hashlib
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            card = base / "card"
            (card / "edturbo").mkdir(parents=True)
            original = b"unique test cartridge" * 100
            (card / "Alpha.pce").write_bytes(original)
            self.manager.card.set(str(card))
            self.manager.offline.set(True)
            errors = []
            def finish():
                deadline = time.monotonic() + 20
                while self.manager.busy and time.monotonic() < deadline:
                    self.root.update()
                    time.sleep(.01)
                self.assertFalse(self.manager.busy)
                self.assertEqual(errors, [])
            with patch.object(ui, "HOME", base / "manager-data"), \
                 patch.object(ui.messagebox, "showerror", side_effect=lambda *args: errors.append(args)), \
                 patch.object(ui.messagebox, "askokcancel", return_value=True), \
                 patch.object(ui.messagebox, "showinfo"):
                self.manager.prepare()
                finish()
                self.assertEqual(self.manager.games.get_children(), ("0",))
                self.assertTrue(self.manager.install_button.instate(["!disabled"]))
                self.assertFalse((card / "TeoPCE.pce").exists())
                self.manager.title.set("My Game")
                self.manager.year.set("1994")
                self.assertTrue(self.manager.save_edits())
                self.assertTrue(self.manager.install_button.instate(["disabled"]))
                self.manager.prepare()
                finish()
                self.assertEqual(self.manager.report["entries"][0]["title"], "My Game")
                self.manager.install()
                finish()
                self.assertTrue((card / "TeoPCE.pce").is_file())
                self.assertEqual((card / "Alpha.pce").read_bytes(), original)
                self.assertTrue(list((base / "manager-data/backups").glob("*/RESULT.json")))
                self.assertTrue(self.manager.install_button.instate(["disabled"]))
                self.assertIn("Installed and verified", self.manager.status.get())


if __name__ == "__main__":
    unittest.main()
