#!/usr/bin/env python3
"""Desktop card preparation and installation for TeoPCE."""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import queue
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from types import SimpleNamespace

from PIL import Image, ImageOps, ImageTk
import prepare_library
import library_install

HOME = Path.home() / "TeoPCE"
BUNDLE = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parents[1]))
if getattr(sys, "frozen", False):
    prepare_library.ROOT = BUNDLE


def card_candidates():
    if sys.platform == "win32":
        import ctypes
        mask = ctypes.windll.kernel32.GetLogicalDrives()
        roots = [Path(chr(65 + i) + ":\\") for i in range(26)
                 if mask & (1 << i) and ctypes.windll.kernel32.GetDriveTypeW(chr(65 + i) + ":\\") == 2]
    elif sys.platform == "darwin":
        roots = list(Path("/Volumes").iterdir())
    else:
        roots = [p for parent in [Path("/media") / Path.home().name, Path("/run/media") / Path.home().name]
                 if parent.exists() for p in parent.iterdir()]
    return [p for p in roots if (p / "edturbo").is_dir()]


class Manager:
    def __init__(self, root, initial=None):
        self.root = root
        self.events = queue.Queue()
        self.busy = False
        self.dirty = False
        self.edits_pending = False
        self.loading_game = False
        self.edit_entry = None
        self.prepared = None
        self.report = None
        self.image = None
        self.card = tk.StringVar(value=initial or "")
        self.search = tk.StringVar()
        self.offline = tk.BooleanVar(value=False)
        self.cheat_database = None
        self.status = tk.StringVar(value="Choose your card, then prepare its catalog.")
        self.summary = tk.StringVar(value="Your library will appear here.")
        self.game_heading = tk.StringVar(value="Select a game")
        self.game_facts = tk.StringVar(value="Covers, game details and compatible cheats")
        self.game_path = tk.StringVar()
        self.cheat_status = tk.StringVar(value="Prepare the catalog to see compatible codes.")
        self.title = tk.StringVar()
        self.year = tk.StringVar()
        self.category = tk.StringVar()
        self.filter = tk.StringVar(value="All games")
        self.page_title = tk.StringVar()
        self.page_subtitle = tk.StringVar()
        self.cover_summary = tk.StringVar(value="—")
        self.details_summary = tk.StringVar(value="—")
        self.codes_summary = tk.StringVar(value="—")
        self.art_status = tk.StringVar()
        self.activity_heading = tk.StringVar(value="Ready when you are")
        self.activity_detail = tk.StringVar(value="Choose your card and prepare a collection to see the activity here.")
        self.source_note = tk.StringVar()
        from catalog_manager_view import build_view
        build_view(self)
        self.filter.trace_add("write", lambda *args: self.refresh_games())
        self.offline.trace_add("write", lambda *args: self.update_sources())
        self.update_sources()
        self.path_entry.configure(state="readonly")
        for variable in [self.title, self.year, self.category]:
            variable.trace_add("write", lambda *args: self.changed())
        self.description.bind("<<Modified>>", self.changed)
        self.card.trace_add("write", lambda *args: self.invalidate())
        self.search.trace_add("write", lambda *args: self.refresh_games())
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.bind("<Command-f>" if sys.platform == "darwin" else "<Control-f>", self.find_game)
        root.bind("<Escape>", lambda e: self.search.set(""))
        if not self.card.get():
            candidates = card_candidates()
            if len(candidates) == 1:
                self.card.set(str(candidates[0]))
        root.after(100, self.poll)

    def navigate(self, page):
        titles = {
            "card": ("Your card", "A visual collection for your PC Engine."),
            "collection": ("Your collection", "Review covers, details and compatible cheats before installing."),
            "activity": ("Behind the scenes", "See what is being read, downloaded and prepared."),
        }
        self.page_title.set(titles[page][0])
        self.page_subtitle.set(titles[page][1])
        self.pages[page].tkraise()
        for key, button in self.nav.items():
            button.configure(style="Selected.Nav.TButton" if key == page else "Nav.TButton")

    def find_game(self, event=None):
        self.navigate("collection")
        self.search_entry.focus_set()
        return "break"

    def log_activity(self, text):
        self.activity_detail.set(text)
        self.activity_log.configure(state="normal")
        self.activity_log.insert("end", datetime.now().strftime("%H:%M:%S") + "   " + text + "\n")
        # Keep the visible log bounded for large collections.
        if int(self.activity_log.index("end-1c").split(".")[0]) > 1600:
            self.activity_log.delete("1.0", "201.0")
        self.activity_log.see("end")
        self.activity_log.configure(state="disabled")

    def update_sources(self):
        if self.offline.get():
            self.source_note.set("Offline: no downloads. Included data and cached images are used; missing artwork can be added later.")
        else:
            self.source_note.set("Online: artwork comes from Libretro Thumbnails; missing database files may also be downloaded. Games stay on your card.")

    def clear_game(self):
        self.edit_entry = None
        self.loading_game = True
        for variable in (self.title, self.year, self.category, self.game_path, self.art_status):
            variable.set("")
        self.game_heading.set("Select a game")
        self.game_facts.set("Choose a game from your collection.")
        self.preview.configure(image="", text="No game selected", height=5)
        self.image = None
        for widget in (self.description, self.overview_text):
            widget.configure(state="normal")
            widget.delete("1.0", "end")
        self.overview_text.configure(state="disabled")
        self.description.edit_modified(False)
        self.code_list.delete(*self.code_list.get_children())
        self.cheat_status.set("Compatible cheats appear here after preparation.")
        self.loading_game = False

    def update_coverage(self):
        entries = self.report.get("entries", []) if self.report else []
        if not entries:
            for variable in (self.cover_summary, self.details_summary, self.codes_summary):
                variable.set("—")
            return
        self.cover_summary.set("%d / %d" % (sum(has_cover(e) for e in entries), len(entries)))
        self.details_summary.set("%d / %d" % (sum(bool(e.get("year")) for e in entries), len(entries)))
        self.codes_summary.set("%d / %d" % (sum(bool(e.get("cheat_groups", 0) + e.get("cheat_held", 0)) for e in entries), len(entries)))

    def changed(self, event=None):
        if event is not None:
            if not self.description.edit_modified():
                return
            self.description.edit_modified(False)
        if self.loading_game or self.busy or self.edit_entry is None:
            return
        self.edits_pending = True
        self.dirty = True
        self.install_button.configure(state="disabled")
        self.status.set("Unsaved edits. Save them, then prepare the catalog.")

    def invalidate(self):
        self.edit_entry = None
        self.edits_pending = False
        self.prepared = None
        self.report = None
        self.games.delete(*self.games.get_children())
        self.install_button.configure(state="disabled")
        self.dirty = False
        self.clear_game()
        self.update_coverage()
        self.summary.set("Prepare your card to see its games.")
        self.status.set("Ready to prepare. Your card is updated only when you install.")
        self.prepare_button.configure(text="Prepare collection", style="Accent.TButton")

    def choose_card(self):
        if self.edits_pending and not messagebox.askyesno("Unsaved edits", "Discard unsaved edits and choose another card?"):
            return
        path = filedialog.askdirectory(title="Choose the card containing edturbo")
        if path:
            self.card.set(path)

    def set_busy(self, busy):
        self.busy = busy
        for control in [self.prepare_button, self.browse, self.path_entry, self.import_button, self.offline_control, self.title_entry, self.year_entry, self.save_button, self.cover_button]:
            control.configure(state="disabled" if busy else "normal")
        ready = not busy and self.prepared and not self.dirty
        self.install_button.configure(state="normal" if ready else "disabled", style="Accent.TButton" if ready else "TButton")
        self.prepare_button.configure(style="TButton" if ready else "Accent.TButton")
        self.path_entry.configure(state="disabled" if busy else "readonly")
        self.category_entry.configure(state="disabled" if busy else "readonly")
        self.description.configure(state="disabled" if busy else "normal")
        if busy:
            self.progress.configure(mode="indeterminate")
            self.progress.start(15)
        else:
            self.progress.stop()
            self.progress.configure(mode="determinate", value=0)

    def overrides_path(self):
        key = hashlib.sha256(str(Path(self.card.get()).resolve()).encode()).hexdigest()[:16]
        return HOME / "metadata" / (key + ".json")

    def prepare(self):
        if self.busy:
            return
        if self.edits_pending and not self.save_edits():
            return
        card = Path(self.card.get()).expanduser().resolve()
        if not (card / "edturbo").is_dir():
            messagebox.showerror("Choose a card", "Select the root folder of your card containing edturbo.")
            return
        output = HOME / "prepared" / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
        metadata = self.overrides_path()
        args = SimpleNamespace(library=card, output=output, previous=None, reference=None,
                               rom=None, metadata=metadata if metadata.is_file() else None,
                               cheat_database=self.cheat_database, cache=Path.home() / ".cache/teopce-library", offline=self.offline.get(), data_only=True)
        self.dirty = True
        self.set_busy(True)
        self.status.set("Preparing on this computer. Your card has not been changed.")
        self.activity_heading.set("Preparing your collection")
        self.log_activity("Starting offline preparation" if args.offline else "Starting preparation · online artwork enabled")
        self.navigate("activity")
        def run():
            try:
                report = prepare_library.prepare(args, progress=lambda message: self.events.put(("progress", message)))
                self.events.put(("prepared", (output, report)))
            except Exception as error:
                self.events.put(("error", str(error)))
        threading.Thread(target=run, daemon=True).start()

    def install(self):
        if self.busy or not self.prepared or self.dirty:
            return
        if not messagebox.askokcancel("Install TeoPCE", "Write the prepared TeoPCE program and catalog to this card? A verified backup will be saved on your computer."):
            return
        prepared = self.prepared / "card"
        target = Path(self.card.get())
        self.set_busy(True)
        self.status.set("Installing. Keep the card connected until verification finishes.")
        self.activity_heading.set("Updating your card")
        self.log_activity("Checking the card, saving a backup and installing TeoPCE…")
        self.navigate("activity")
        def run():
            try:
                self.events.put(("installed", library_install.install(prepared, target, HOME / "backups")))
            except Exception as error:
                self.events.put(("error", str(error)))
        threading.Thread(target=run, daemon=True).start()

    def poll(self):
        try:
            while True:
                kind, value = self.events.get_nowait()
                if kind == "progress":
                    self.log_activity(value)
                elif kind == "error":
                    self.set_busy(False)
                    self.status.set("Operation stopped. See Activity for details.")
                    self.activity_heading.set("Something needs attention")
                    self.log_activity(value)
                    self.navigate("activity")
                    messagebox.showerror("Operation stopped", value)
                elif kind == "prepared":
                    self.prepared, self.report = value
                    self.set_busy(False)
                    self.dirty = False
                    self.search.set("")
                    self.filter.set("All games")
                    self.refresh_games()
                    children = self.games.get_children()
                    if children:
                        self.games.selection_set(children[0])
                        self.select_game()
                    self.set_busy(False)
                    skipped = len(self.report["skipped"])
                    self.status.set("%d games ready · %d skipped. Review, then install." % (self.report["games"], skipped))
                    self.activity_heading.set("Collection prepared")
                    self.log_activity(self.status.get())
                    self.prepare_button.configure(text="Refresh collection")
                    self.navigate("collection")
                elif kind == "installed":
                    self.set_busy(False)
                    self.dirty = True  # A new preparation is required after a card write.
                    self.install_button.configure(state="disabled")
                    self.activity_heading.set("Your card is ready")
                    self.status.set("Installed and verified. Safely eject the card, then start TeoPCE.pce on your console.")
                    self.log_activity(self.status.get())
                    messagebox.showinfo("Card ready", self.status.get() + "\n\nBackup\n" + value["backup"])
        except queue.Empty:
            pass
        self.root.after(100, self.poll)

    def refresh_games(self):
        selected = self.games.selection()
        self.games.delete(*self.games.get_children())
        self.update_coverage()
        if self.report is None:
            return
        query = self.search.get().strip().casefold()
        mode = self.filter.get()
        for entry in self.report["entries"]:
            kind = "CD-ROM" if entry["path"].lower().endswith(".cue") else "HuCard"
            count = entry.get("cheat_groups", 0) + entry.get("cheat_held", 0)
            matches = (mode == "All games" or mode == kind or mode == "Missing covers" and not has_cover(entry)
                       or mode == "With cheats" and count)
            if matches and query in (entry["title"] + " " + entry["path"] + " " + kind).casefold():
                self.games.insert("", "end", iid=str(entry["index"]),
                                  values=(entry["title"], entry["year"] or "—", "Ready" if has_cover(entry) else "Missing", count or "—"),
                                  tags=("odd",) if len(self.games.get_children()) % 2 else ())
        children = self.games.get_children()
        if selected and selected[0] in children:
            self.games.selection_set(selected[0])
        self.summary.set("%d of %d games · search by title or folder" % (len(children), len(self.report["entries"])))

    def import_cheats(self):
        if self.busy:
            return
        selected = filedialog.askopenfilename(title="Import a TeoPCE cheat library", filetypes=[("TeoPCE cheat library", "*.json")])
        if not selected:
            return
        try:
            import library_cheats
            library_cheats.Database([Path(selected)])
            self.cheat_database = Path(selected)
            self.dirty = True
            self.install_button.configure(state="disabled")
            self.status.set("Cheat library selected. Prepare the catalog to match its games.")
        except (OSError, ValueError) as error:
            messagebox.showerror("Invalid cheat library", str(error))

    def selected(self):
        items = self.games.selection()
        if not items or self.report is None:
            return None
        return self.report["entries"][int(items[0])]

    def select_game(self, event=None):
        entry = self.selected()
        if self.busy or entry is None or self.prepared is None:
            return
        if self.edits_pending and self.edit_entry is not None:
            if entry["index"] == self.edit_entry["index"]:
                return
            answer = messagebox.askyesnocancel("Unsaved edits", "Save changes to the previous game?")
            if answer is None or answer and not self.save_edits():
                self.games.selection_set(str(self.edit_entry["index"]))
                return
            self.edits_pending = False
        self.edit_entry = entry
        self.loading_game = True
        self.title.set(entry["title"])
        self.year.set(str(entry["year"]) if entry["year"] else "")
        self.category.set(["original", "translations", "homebrew", "museum", "tools", "unlicensed"][entry["category"] - 1])
        path = self.prepared / "card/00TEOPCE/data/desc" / ("%06d.txt" % entry["index"])
        self.description.delete("1.0", "end")
        self.description.insert("1.0", entry.get("editable_description", path.read_text()))
        self.description.edit_modified(False)
        self.loading_game = False
        if has_cover(entry):
            self.image = ImageTk.PhotoImage(preview_image(self.prepared, entry["index"], size=(300, 220)))
            self.preview.configure(image=self.image, text="", height=0)
        else:
            self.image = None
            self.preview.configure(image="", text="No cover found\n\nAdd your own in Edit", height=9)
        source = entry.get("art", {}).get("c", {}).get("source", "")
        self.art_status.set({"libretro-thumbnails": "Cover · Libretro Thumbnails", "user image": "Cover · your image",
                             "placeholder": "Artwork missing · you can add it later", "owned exact-content reference": "Cover · preserved from your card"}.get(source, "Cover · prepared artwork"))
        self.game_heading.set(entry["title"])
        kind = "CD-ROM" if entry["path"].lower().endswith(".cue") else "HuCard"
        self.game_facts.set(" · ".join(str(x) for x in [kind, entry["year"] or "", entry.get("cheat_groups", 0) and str(entry["cheat_groups"]) + (" code group" if entry["cheat_groups"] == 1 else " code groups")] if x))
        self.game_path.set(entry["path"])
        self.overview_text.configure(state="normal")
        self.overview_text.delete("1.0", "end")
        desktop_text = " ".join(path.read_text().split())
        for label in ("Developer", "Publisher"):
            desktop_text = desktop_text.replace(label + " - ", "\n" + label + ": ")
        self.overview_text.insert("1.0", desktop_text.strip())
        self.overview_text.configure(state="disabled")
        self.code_list.delete(*self.code_list.get_children())
        package = self.prepared / "card/00TEOPCE/cheats" / (entry["key"] + ".tcg")
        if package.is_file():
            import build_cheat_groups as codec
            raw = package.read_bytes()
            _, _, _, _, key, size, crc, *_ = codec.HEADER.unpack_from(raw)
            for group in codec.decode_package(raw, key, size, crc):
                self.code_list.insert("", "end", values=(group["name"] + (" · held" if group["reason"] else ""),))
            self.cheat_status.set("Select and activate these groups on the console. Existing selections are preserved.")
        else:
            reason = entry.get("cheats", "")
            self.cheat_status.set("This CD cheat pack needs the game at:\n" + reason.split("requires path ", 1)[1]
                                  if "requires path " in reason else
                                  "No compatible codes found for this game version. You can still play it normally.")

    def save_edits(self):
        entry = self.edit_entry or self.selected()
        if entry is None or self.busy:
            return
        try:
            year = int(self.year.get()) if self.year.get().strip() else 0
            if year != 0 and not 1970 <= year <= 2099:
                raise ValueError("Use a year from 1970 to 2099, or leave it empty.")
            if not self.title.get().strip():
                raise ValueError("Enter the game title.")
            path = self.overrides_path()
            document = json.loads(path.read_text()) if path.is_file() else {"version": 1, "games": {}}
            meta = document["games"].setdefault(entry["path"], {})
            meta.update(title=self.title.get().strip(), year=year, category=self.category.get(), description=self.description.get("1.0", "end").strip())
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(document, ensure_ascii=False, indent=2) + "\n")
            self.dirty = True
            self.edits_pending = False
            self.status.set("Edits saved. Prepare the catalog again to include them.")
            self.install_button.configure(state="disabled")
            return True
        except (ValueError, OSError) as error:
            messagebox.showerror("Could not save edits", str(error))
            return False

    def choose_cover(self):
        entry = self.selected()
        if entry is None or self.busy:
            return
        image = filedialog.askopenfilename(title="Choose a cover", filetypes=[("Images", "*.png *.jpg *.jpeg")])
        if image:
            if not self.save_edits():
                return
            path = self.overrides_path()
            document = json.loads(path.read_text())
            document["games"][entry["path"]]["cover"] = image
            path.write_text(json.dumps(document, ensure_ascii=False, indent=2) + "\n")

    def show_report(self):
        if self.report is None:
            messagebox.showinfo("Report", "Prepare a catalog to see the report.")
            return
        window = tk.Toplevel(self.root)
        window.title("Preparation report")
        window.geometry("780x540")
        text = tk.Text(window, wrap="word", padx=16, pady=16)
        text.pack(fill="both", expand=True)
        summary = "%d games · %d skipped\n\n" % (self.report["games"], len(self.report["skipped"]))
        summary += "\n".join(item["path"] + "\n" + item["reason"] + "\n" for item in self.report["skipped"])
        summary += "\n" + "\n".join(self.report["warnings"])
        text.insert("1.0", summary or "Preparation complete")
        text.configure(state="disabled")

    def close(self):
        if self.busy:
            messagebox.showinfo("Operation in progress", "Keep the manager open until preparation or installation finishes.")
        else:
            if self.edits_pending:
                answer = messagebox.askyesnocancel("Unsaved edits", "Save changes before closing?")
                if answer is None or answer and not self.save_edits():
                    return
            self.root.destroy()


def has_cover(entry):
    return entry.get("art", {}).get("c", {}).get("source") not in (None, "placeholder")


def preview_image(prepared, index, cache=None, size=(256, 192)):
    """Use original downloaded art for desktop review, with a console-art fallback."""
    from io import BytesIO
    import library_sources
    data = prepared / "card/00TEOPCE/data"
    cache = cache or Path.home() / ".cache/teopce-library"
    try:
        ledger = json.loads((data / "ART-PROVENANCE.json").read_text())
        source = next(item for item in ledger if item["index"] == index and item["image"] == "c")
        raw = None
        if source.get("url"):
            raw = library_sources.download(source["url"], cache, offline=True, limit=12 * 1024 * 1024)
        elif source.get("source") == "user image" and Path(source["path"]).is_absolute():
            raw = library_sources.media.read_file(Path(source["path"]), 12 * 1024 * 1024)
        if raw and (not source.get("source_sha256") or hashlib.sha256(raw).hexdigest() == source["source_sha256"]):
            with Image.open(BytesIO(raw)) as image:
                return ImageOps.contain(image.convert("RGB"), size, Image.Resampling.LANCZOS)
    except (OSError, ValueError, KeyError, StopIteration, Image.DecompressionBombError):
        pass
    return ImageOps.contain(decode_art(data / "art" / ("%06d-c.bin" % index)), size, Image.Resampling.NEAREST)


def decode_art(path):
    import struct
    raw = path.read_bytes()
    prepare_library.validate_art(raw)
    modern = len(raw) == 3550
    palette = struct.unpack_from("<32H" if modern else "<16H", raw, 16 if modern else 0)
    mask = raw[80:94] if modern else bytes(14)
    tiles = raw[94:] if modern else raw[32:]
    image = Image.new("RGB", (96, 72))
    pixels = image.load()
    for tile in range(108):
        bank = 16 if mask[tile // 8] & (1 << (tile % 8)) else 0
        for y in range(8):
            for x in range(8):
                color = sum(((tiles[tile * 32 + (plane // 2) * 16 + y * 2 + plane % 2] >> (7 - x)) & 1) << plane for plane in range(4))
                word = palette[bank + color]
                pixels[tile % 12 * 8 + x, tile // 12 * 8 + y] = tuple((v * 255 + 3) // 7 for v in [(word >> 3) & 7, (word >> 6) & 7, word & 7])
    return image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--card")
    parser.add_argument("--smoke", action="store_true")
    args = parser.parse_args()
    root = tk.Tk()
    Manager(root, args.card)
    if args.smoke:
        root.after(1000, root.destroy)
    root.mainloop()


if __name__ == "__main__":
    main()
