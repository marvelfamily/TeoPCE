# About the screenshots

[Project home](../README.md) · [Visual tour](TOUR.md)

The gallery illustrates the released interface using real rendered screens. No AI-generated interface, replacement text, invented success state or painted-in artwork is used.

## Console captures

The `console-*.png` images come from a fresh preview build of **TeoPCE 0.9.13**, running its actual UI/input code through the Mednafen PCE Fast libretro core. The six-game personal sample catalog contains real artwork and supplied descriptions. Its content and cheat counts are sample-specific and can differ from a fresh manager-generated collection.

Video frames are **352 × 243**, saved at **1056 × 729** by repeating each source pixel exactly three times in each direction. PNG is lossless. This keeps the original pixels sharp without smoothing, AI upscaling, colour replacement or retouching. A TV/scaler can present a different aspect ratio or colour rendition.

The UI preview does not access a physical EverDrive SD card. Favourite/history captures show actual session interactions. Cartridge launch, persistent writes and cheat application are **not** demonstrated by these images. The native cheat state and file-information screens were omitted because the preview cannot represent their hardware state; the tour documents those functions in text.

## Desktop captures

`catalog-manager.png` shows the packaged macOS **0.2.0-rc4** initial screen. The other `manager-*.png` files are native Retina captures of the unchanged RC4 source interface. Its real preparation worker scanned a separate five-game sample, read game content, matched the bundled facts/cheats and used downloaded/cached artwork. No report values were fabricated, and no personal SD was installed for these captures.

The sample games are 1943 Kai (Japan), Air Zonk (USA), Alien Crush (USA), Blazing Lazers (USA) and Bomberman '94 (Japan). All five happened to match cover/year/cheat data. **5/5 is a sample result, not a coverage guarantee.** The empty description editor is a user-input field; descriptions are not automatically downloaded.

The capture service returns JPEG. Only the top **64-pixel system title bar** was cropped off to exclude the system control indicator; the application content was not repainted or resized. The cropped images are exported as PNG to avoid another lossy encoding. Collection, edit, cheats and activity images are **2240 × 1520**; setup is **2480 × 1680**. This does not undo compression in the original capture. The Mac screenshots are not Windows screenshots.

## Attribution and reproducibility

Game names, cover art and game imagery belong to their respective rights holders. Small in-interface examples are included solely to explain TeoPCE; no raw artwork collection, game, BIOS or personal catalog is distributed. Code's MIT license does not relicense the depicted game artwork.

[CAPTURES.json](images/CAPTURES.json) records console build/core hashes, input frame sequences and image hashes, plus desktop image hashes. It contains no private filesystem paths or game files. See [THIRD-PARTY.md](../THIRD-PARTY.md) for runtime and data-source attribution.
