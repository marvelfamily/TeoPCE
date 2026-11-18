"""Original desktop presentation for TeoPCE; behavior stays in the manager."""
import sys
import tkinter as tk
from tkinter import ttk

PAPER = "#f4f1eb"
WHITE = "#ffffff"
INK = "#20343e"
MUTED = "#596970"
LINE = "#d9ddd9"
ORANGE = "#b64421"
TEAL = "#22685e"
RAIL = "#192e38"


def build_view(m):
    root = m.root
    font = "Helvetica Neue" if sys.platform == "darwin" else "Segoe UI"
    m.font = font
    root.title("TeoPCE Catalog Manager 0.2.0-rc6")
    root.geometry("1240x840")
    root.minsize(1120, 760)
    root.configure(bg=PAPER)
    style = ttk.Style(root)
    style.theme_use("clam")
    style.configure(".", background=PAPER, foreground=INK, font=(font, 12))
    style.configure("TFrame", borderwidth=0)
    style.configure("Card.TFrame", background=WHITE)
    style.configure("TLabel", borderwidth=0, background=PAPER)
    style.configure("Card.TLabel", background=WHITE)
    style.configure("Muted.TLabel", foreground=MUTED)
    style.configure("TButton", padding=(15, 10), background="#e5e8e3", foreground=INK,
                    borderwidth=0, relief="flat", focuscolor=TEAL)
    style.map("TButton", background=[("active", "#d5ddd7")], foreground=[("disabled", "#86918d")])
    style.configure("Accent.TButton", background=ORANGE, foreground=WHITE)
    style.map("Accent.TButton", background=[("disabled", "#dfc3b8"), ("active", "#983719")],
              foreground=[("disabled", "#76594d")])
    style.configure("Nav.TButton", background=RAIL, foreground="#bdc9ce", anchor="w", padding=(16, 13))
    style.map("Nav.TButton", background=[("active", "#29434e")], foreground=[("active", WHITE)])
    style.configure("Selected.Nav.TButton", background="#34505a", foreground=WHITE)
    style.configure("TEntry", fieldbackground=WHITE, foreground=INK, insertcolor=INK,
                    bordercolor=LINE, lightcolor=LINE, darkcolor=LINE, padding=9)
    style.configure("TCombobox", fieldbackground=WHITE, foreground=INK, padding=9, arrowcolor=MUTED)
    style.map("TCombobox", fieldbackground=[("readonly", WHITE)], selectbackground=[("readonly", WHITE)],
              selectforeground=[("readonly", INK)])
    style.configure("TCheckbutton", background=WHITE, foreground=INK, padding=3)
    style.map("TCheckbutton", background=[("active", WHITE)])
    style.configure("Treeview", background=WHITE, fieldbackground=WHITE, foreground=INK,
                    rowheight=42, borderwidth=0, bordercolor=WHITE, lightcolor=WHITE, darkcolor=WHITE, relief="flat", font=(font, 12))
    style.configure("Treeview.Heading", background="#e9ece7", foreground=MUTED,
                    font=(font, 10, "bold"), padding=(10, 10), borderwidth=0, relief="flat")
    style.map("Treeview", background=[("selected", "#d8eae5")], foreground=[("selected", "#123f36")])
    style.configure("TNotebook", background=WHITE, borderwidth=0, bordercolor=WHITE, lightcolor=WHITE, darkcolor=WHITE, tabmargins=(0, 0, 0, 12))
    style.configure("TNotebook.Tab", padding=(12, 8), background=WHITE, foreground=MUTED, borderwidth=0, bordercolor=WHITE, lightcolor=WHITE, darkcolor=WHITE)
    style.map("TNotebook.Tab", background=[("selected", "#e2eee8")], foreground=[("selected", "#174e43")])
    style.configure("Vertical.TScrollbar", background="#c9d1cd", troughcolor=WHITE,
                    borderwidth=0, arrowcolor=MUTED)
    style.configure("Horizontal.TProgressbar", background=TEAL, troughcolor="#e0e6df",
                    borderwidth=0, thickness=4)
    root.option_add("*TCombobox*Listbox.background", WHITE)
    root.option_add("*TCombobox*Listbox.foreground", INK)

    def label(parent, text=None, variable=None, size=12, color=INK, bold=False, bg=PAPER, **kw):
        widget = tk.Label(parent, text=text, textvariable=variable, font=(font, size, "bold" if bold else "normal"),
                          bg=bg, fg=color, anchor="w", justify="left", borderwidth=0, **kw)
        return widget

    rail = tk.Frame(root, bg=RAIL, width=190)
    rail.pack(side="left", fill="y")
    rail.pack_propagate(False)
    brand = tk.Canvas(rail, height=76, width=150, bg=RAIL, highlightthickness=0)
    brand.pack(padx=20, pady=(26, 0))
    # A simple original cartridge mark: paired contacts echo the two console buttons.
    brand.create_rectangle(3, 5, 34, 39, outline="#e8aa65", width=2)
    brand.create_rectangle(8, 10, 29, 23, fill="#e8aa65", outline="")
    for x in (11, 22):
        brand.create_line(x, 32, x, 38, fill="#e8aa65", width=3)
    brand.create_text(47, 23, text="TeoPCE", fill=WHITE, font=(font, 21, "bold"), anchor="w")
    label(rail, "CATALOG MANAGER", size=9, color="#9eafb5", bg=RAIL).pack(anchor="w", padx=24, pady=(0, 36))
    m.nav = {}
    for key, text in [("card", "01    Your card"), ("collection", "02    Collection"), ("activity", "03    Activity")]:
        button = ttk.Button(rail, text=text, style="Nav.TButton", command=lambda k=key: m.navigate(k))
        button.pack(fill="x", padx=12, pady=3)
        m.nav[key] = button
    label(rail, "PC Engine\nTurbo EverDrive Pro", color="#c4cfd3", size=11, bg=RAIL).pack(side="bottom", anchor="w", padx=24, pady=(0, 16))
    label(rail, "DESKTOP  0.2 · RC6", color="#8ea2aa", size=9, bg=RAIL).pack(side="bottom", anchor="w", padx=24, pady=(0, 12))

    shell = ttk.Frame(root, padding=(28, 24, 28, 0))
    shell.pack(side="left", fill="both", expand=True)
    header = ttk.Frame(shell)
    header.pack(fill="x", pady=(0, 18))
    label(header, variable=m.page_title, size=25, bold=True).pack(anchor="w")
    label(header, variable=m.page_subtitle, color=MUTED, size=11).pack(anchor="w", pady=(5, 0))

    footer = ttk.Frame(shell, padding=(0, 14, 0, 18))
    footer.pack(side="bottom", fill="x")
    line = tk.Frame(footer, height=1, bg=LINE)
    line.pack(fill="x", pady=(0, 12))
    actions = ttk.Frame(footer)
    actions.pack(side="right")
    m.prepare_button = ttk.Button(actions, text="Prepare collection", style="Accent.TButton", command=m.prepare)
    m.prepare_button.pack(side="left", padx=(8, 0))
    m.install_button = ttk.Button(actions, text="Install on card", command=m.install, state="disabled")
    m.install_button.pack(side="left", padx=(8, 0))
    label(footer, variable=m.status, color=MUTED, size=11, wraplength=430).pack(side="left", fill="x", expand=True)

    deck = ttk.Frame(shell)
    deck.pack(fill="both", expand=True)
    deck.rowconfigure(0, weight=1)
    deck.columnconfigure(0, weight=1)
    m.pages = {}
    for key in m.nav:
        frame = ttk.Frame(deck)
        frame.grid(row=0, column=0, sticky="nsew")
        m.pages[key] = frame

    card = m.pages["card"]
    intro = ttk.Frame(card, style="Card.TFrame", padding=24)
    intro.pack(fill="x")
    label(intro, "Your games, ready to explore.", size=23, bold=True, bg=WHITE).pack(anchor="w")
    label(intro, "Build a visual menu for the games already on your SD card.", color=MUTED, bg=WHITE).pack(anchor="w", pady=(8, 22))
    label(intro, "SD CARD", size=10, color=TEAL, bold=True, bg=WHITE).pack(anchor="w")
    paths = ttk.Frame(intro, style="Card.TFrame")
    paths.pack(fill="x", pady=(8, 8))
    m.path_entry = ttk.Entry(paths, textvariable=m.card)
    m.path_entry.pack(side="left", fill="x", expand=True)
    m.browse = ttk.Button(paths, text="Choose card…", command=m.choose_card)
    m.browse.pack(side="left", padx=(12, 0))
    label(intro, "Choose the card with your games and the EverDrive folder (edturbo).", size=11, color=MUTED, bg=WHITE).pack(anchor="w")
    label(card, "What goes into your collection", size=16, bold=True).pack(anchor="w", pady=(24, 12))
    sources = ttk.Frame(card)
    sources.pack(fill="x")
    info = [
        ("01", "Game details", "INCLUDED", "Names, years and publishers\nfrom the bundled Libretro database.\nAdd your own descriptions in Edit."),
        ("02", "Covers & screenshots", "ONLINE", "Downloaded from Libretro Thumbnails.\nSaved on this computer for reuse.\nYou can also choose your own cover."),
        ("03", "Compatible cheats", "INCLUDED", "Matched to the exact game version.\nAvailable codes appear automatically.\nActivate them on your console."),
    ]
    for col, (n, title, tag, body) in enumerate(info):
        sources.columnconfigure(col, weight=1, uniform="source")
        box = ttk.Frame(sources, style="Card.TFrame", padding=16)
        box.grid(row=0, column=col, sticky="nsew", padx=(0 if col == 0 else 10, 0))
        label(box, n + "   /   " + tag, size=9, color=TEAL, bold=True, bg=WHITE).pack(anchor="w")
        label(box, title, size=13, bold=True, bg=WHITE).pack(anchor="w", pady=(10, 8))
        text = label(box, body, size=11, color=MUTED, bg=WHITE, wraplength=242)
        text.pack(anchor="w")
        box.bind("<Configure>", lambda e, w=text: w.configure(wraplength=max(130, e.width - 32)))
    options = ttk.Frame(card, style="Card.TFrame", padding=(16, 12))
    options.pack(fill="x", pady=(14, 0))
    m.offline_control = ttk.Checkbutton(options, text="Work offline · use included and previously downloaded data", variable=m.offline)
    m.offline_control.pack(side="left")
    m.import_button = ttk.Button(options, text="Import extra cheats…", command=m.import_cheats)
    m.import_button.pack(side="right")
    label(card, variable=m.source_note, color=MUTED, size=11, wraplength=860).pack(anchor="w", pady=(12, 0))

    collection = m.pages["collection"]
    stats = ttk.Frame(collection)
    stats.pack(fill="x", pady=(0, 14))
    for i, (var, title) in enumerate([(m.cover_summary, "COVERS"), (m.details_summary, "YEARS KNOWN"), (m.codes_summary, "WITH CHEATS")]):
        stats.columnconfigure(i, weight=1, uniform="stats")
        box = ttk.Frame(stats, style="Card.TFrame", padding=(16, 10))
        box.grid(row=0, column=i, sticky="ew", padx=(0 if i == 0 else 10, 0))
        label(box, title, size=9, bold=True, color=MUTED, bg=WHITE).pack(anchor="w")
        label(box, variable=var, size=19, bold=True, bg=WHITE).pack(anchor="w", pady=(3, 0))
    m.body = ttk.Panedwindow(collection, orient="horizontal")
    m.body.pack(fill="both", expand=True)
    left = ttk.Frame(m.body)
    right = ttk.Frame(m.body, style="Card.TFrame", padding=18)
    m.body.add(left, weight=3)
    m.body.add(right, weight=2)
    search = ttk.Frame(left)
    search.pack(fill="x", pady=(0, 10))
    label(search, "Search", color=MUTED, size=11).pack(side="left", padx=(0, 8))
    m.search_entry = ttk.Entry(search, textvariable=m.search)
    m.search_entry.pack(side="left", fill="x", expand=True)
    m.filter_control = ttk.Combobox(search, textvariable=m.filter, state="readonly", width=15,
                                  values=["All games", "Missing covers", "With cheats", "HuCard", "CD-ROM"])
    m.filter_control.pack(side="right", padx=(8, 0))
    label(left, variable=m.summary, color=MUTED, size=10).pack(anchor="w", pady=(0, 8))
    m.games = ttk.Treeview(left, columns=("title", "year", "art", "cheats"), show="headings", selectmode="browse")
    for name, title, width in [("title", "GAME", 240), ("year", "Year", 58), ("art", "Cover", 70), ("cheats", "Codes", 60)]:
        m.games.heading(name, text=title)
        m.games.column(name, width=width, minwidth=160 if name == "title" else width,
                       stretch=name == "title", anchor="w" if name == "title" else "center")
    scroll = ttk.Scrollbar(left, command=m.games.yview)
    m.games.configure(yscrollcommand=scroll.set)
    scroll.pack(side="right", fill="y")
    m.games.pack(fill="both", expand=True)
    m.games.tag_configure("odd", background="#f8faf7")
    m.games.bind("<<TreeviewSelect>>", m.select_game)
    label(right, variable=m.game_heading, size=18, bold=True, bg=WHITE, wraplength=330).pack(anchor="w")
    label(right, variable=m.game_facts, color=MUTED, size=11, bg=WHITE, wraplength=330).pack(anchor="w", pady=(6, 12))
    m.tabs = ttk.Notebook(right)
    m.tabs.pack(fill="both", expand=True)
    overview, edit, codes = [ttk.Frame(m.tabs, style="Card.TFrame") for _ in range(3)]
    for frame, title in [(overview, "Game card"), (edit, "Edit"), (codes, "Cheats")]:
        m.tabs.add(frame, text=title)
    m.preview = tk.Label(overview, text="Your collection will appear here", bg="#edf0e9", fg=MUTED,
                         font=(font, 13), height=1, anchor="center")
    m.preview.pack(fill="x", pady=(0, 8))
    label(overview, variable=m.art_status, size=10, color=TEAL, bg=WHITE, wraplength=330).pack(anchor="w", pady=(0, 10))
    m.overview_text = tk.Text(overview, height=4, width=28, highlightthickness=0, wrap="word",
                             bg=WHITE, fg=INK, relief="flat", font=(font, 12), state="disabled")
    m.overview_text.pack(fill="both", expand=True)
    label(overview, variable=m.game_path, size=9, color=MUTED, bg=WHITE, wraplength=330).pack(fill="x", pady=(8, 0))
    for title, var, attr in [("Title", m.title, "title_entry"), ("Year · leave empty if unknown", m.year, "year_entry")]:
        label(edit, title, size=11, bg=WHITE).pack(anchor="w", pady=(0, 4))
        field = ttk.Entry(edit, textvariable=var)
        field.pack(fill="x", pady=(0, 10))
        setattr(m, attr, field)
    label(edit, "Category", size=11, bg=WHITE).pack(anchor="w", pady=(0, 4))
    m.category_entry = ttk.Combobox(edit, textvariable=m.category, state="readonly",
                                  values=["original", "translations", "homebrew", "museum", "tools", "unlicensed"])
    m.category_entry.pack(fill="x", pady=(0, 10))
    controls = ttk.Frame(edit, style="Card.TFrame")
    controls.pack(side="bottom", fill="x", pady=(10, 0))
    m.cover_button = ttk.Button(controls, text="Choose cover…", command=m.choose_cover)
    m.cover_button.pack(side="left")
    m.save_button = ttk.Button(controls, text="Save edits", command=m.save_edits)
    m.save_button.pack(side="right")
    label(edit, "Description", size=11, bg=WHITE).pack(anchor="w", pady=(0, 4))
    m.description = tk.Text(edit, height=3, width=28, highlightthickness=1, highlightbackground=LINE,
                            wrap="word", bg="#f8faf7", fg=INK, insertbackground=INK, relief="flat",
                            padx=8, pady=8, font=(font, 12))
    m.description.pack(fill="both", expand=True)
    label(codes, variable=m.cheat_status, color=MUTED, size=11, bg=WHITE, wraplength=330).pack(fill="x", pady=(0, 12))
    m.code_list = ttk.Treeview(codes, columns=("name",), show="headings", selectmode="browse")
    m.code_list.heading("name", text="AVAILABLE ON YOUR CONSOLE")
    m.code_list.column("name", width=280, minwidth=180)
    scroll = ttk.Scrollbar(codes, command=m.code_list.yview)
    m.code_list.configure(yscrollcommand=scroll.set)
    scroll.pack(side="right", fill="y")
    m.code_list.pack(fill="both", expand=True)

    activity = m.pages["activity"]
    progress_box = ttk.Frame(activity, style="Card.TFrame", padding=22)
    progress_box.pack(fill="x", pady=(0, 16))
    label(progress_box, variable=m.activity_heading, size=20, bold=True, bg=WHITE).pack(anchor="w")
    label(progress_box, variable=m.activity_detail, color=MUTED, bg=WHITE, wraplength=810).pack(anchor="w", pady=(8, 16))
    m.progress = ttk.Progressbar(progress_box, mode="determinate")
    m.progress.pack(fill="x")
    label(activity, "ACTIVITY", size=10, color=MUTED, bold=True).pack(anchor="w", pady=(0, 8))
    log_frame = ttk.Frame(activity)
    log_frame.pack(fill="both", expand=True)
    m.activity_log = tk.Text(log_frame, bg=WHITE, fg=INK, font=(font, 11), wrap="word",
                             padx=16, pady=14, relief="flat", highlightthickness=0, state="disabled")
    scroll = ttk.Scrollbar(log_frame, command=m.activity_log.yview)
    m.activity_log.configure(yscrollcommand=scroll.set)
    scroll.pack(side="right", fill="y")
    m.activity_log.pack(fill="both", expand=True)
    ttk.Button(activity, text="View skipped games & warnings", command=m.show_report).pack(anchor="e", pady=(10, 0))

    def split(event):
        if event.width > 1:
            m.body.sashpos(0, int(event.width * .56))
            m.body.unbind("<Configure>")
    m.body.bind("<Configure>", split)
    m.navigate("card")
