# Sources and notices

## Krikzz SDK

TeoPCE uses the modified Turbo EverDrive SDK from https://github.com/krikzz/turbo-ed-pub . The MIT notice is in licenses/KRIKZZ-LICENSE.txt.

## cc65 runtime

The included ROM was built with cc65 V2.19 and lib/nes.lib. The cc65 V2.19 notice is retained in licenses/CC65-LICENSE.txt, obtained from https://github.com/cc65/cc65/blob/V2.19/LICENSE . The temporary toolchain reports V2.19 - N/A, so the precise build commit remains unrecorded. The historical compiler build commit remains unknown, so byte-identical reproduction of that bundled binary has not been demonstrated. The included native sources can be built with an installed cc65 toolchain.

## Libretro Database

Factual metadata files are included from libretro/libretro-database at revision 969e31a324119a17add184e8b716315a74adb51b. The data license is CC BY-SA 4.0, retained in licenses/LIBRETRO-DATABASE-LICENSE.txt. Original source URLs and SHA-256 digests appear in resources/database-cache/SOURCES.json. The builder adapts metadata into TeoPCE records, normalizes genres and omits ambiguous matches. Original authors and contributors are credited through the upstream project https://github.com/libretro/libretro-database . Derived metadata retains the source's attribution/share-alike conditions.

## Artwork

Images are fetched by the user at preparation time from libretro-thumbnails repositories. The pinned family repository revisions are defined in tools/artwork.py. Artwork provenance records the source URL and original/converted digests in the prepared card's ART-PROVENANCE.json. Rights belong to their respective holders. Thumbnail availability does not grant permission to redistribute every image. No downloaded game artwork is included in this kit.

## Python and Pillow

The source distribution uses separately installed Python with Pillow. Packaged standalone applications include the runtime and dependency notices listed below.

## Cheat data

The bundled cheat data is adapted from libretro/libretro-database at revision 969e31a324119a17add184e8b716315a74adb51b under CC BY-SA 4.0. Attribution belongs to the Libretro Database contributors and any creators credited in the retained original files. The license text is resources/cheats/LICENSE-CC-BY-SA-4.0.txt. The exact original files, their URLs, Git blob hashes and SHA-256 digests are retained under resources/cheats/upstream/ and resources/cheats/SOURCES.json.

TeoPCE selected 1,177 complete groups for 347 known-content releases, normalized labels and RAM aliases, and added content bindings, stable group IDs and startup-policy restrictions. Address/value sequences match complete source groups exactly. The derived library retains CC BY-SA 4.0. The data's share-alike terms apply to the adapted database. TeoPCE-owned program code uses MIT.

Prepared cards carry CC-BY-SA-4.0.txt, METADATA-SOURCES.json and a checksum-bound CHEAT-SOURCES.json inside 00TEOPCE/data/. Attribution follows preserved packages on subsequent preparation.

## TeoPCE code and user content

TeoPCE-owned code is licensed under MIT. The license is in LICENSE. Games, BIOS, firmware, user saves, private saves and downloaded game art are supplied separately by the user. The bundled cheat library contains codes and fingerprints. Games, BIOS and firmware remain separate user inputs.

## Standalone manager runtime

The standalone manager bundles CPython, Tcl/Tk, Pillow, libtommath and the PyInstaller bootloader. Their notices are retained in licenses/. PyInstaller's bootloader exception applies to distribution of frozen applications. The local macOS build uses CPython 3.14.7, Tcl/Tk 9.1.0, Pillow 12.3.0 and PyInstaller 6.22.3. It is ad-hoc signed and has not been notarized. Source build dependencies are declared separately.

## Windows runtime

The Windows x64 build uses CPython 3.12.10, Tcl/Tk 8.6, Pillow 12.3.0 and PyInstaller 6.22.3. Runtime and wheel notices from that build are retained in licenses/windows/. It was built on Windows 11 ARM64 using the system x64 application emulation.
