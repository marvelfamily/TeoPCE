# Catalog Manager 0.2.0-rc6 · TeoPCE 0.9.15

## On the console

Black Hole Assault (Japan) reached combat on a CoreGrafx with a Turbo EverDrive Pro and all cheats off on 0.9.15. The same spare card had shown `DA / Stage 24` with 0.9.14. Batman (HuCard) also launched on that console with 0.9.14.

## What went wrong in RC4

RC4 was tested on an established personal card. Its desktop checks could build a new catalog, but its integration test substituted a successful cheat check before handing off a game. It missed two conditions that are normal on a fresh card. Ordinary HuCard launches could stop at a missing BRAM journal, and an ordinary CD launch with no per-game cheat folder could leave a pending recovery transaction. The intermediate 0.9.14 corrected that transaction and the HuCard launch. A later CD check still read the missing folder as an unsupported cheat combination. Version 0.9.15 shares the same file lookup between the two CD stages.

## Checks for RC6

- The local public source suite passed 62 tests on Mac. It includes both CD stages, an interrupted first launch, active codes, conflicting state, first install and refresh. Rebuilding the console ROM matched the distributed bytes.
- Using the manager on disposable copies, we prepared a HuCard-only collection and a CD-only collection in different paths, starting with no native game-data directory. We also refreshed an RC4 installation and compared its games, native files and TeoPCE user files byte for byte.
- For each resulting catalog entry a host replay ran the production preparation, file checks and launch code up to the native start call. The replay simulated cartridge status and RAM. The console test above is separate.
- The desktop build workflow runs the public suite and starts the packaged app on macOS and Windows. Its logs give the per-platform results and any compiler-dependent skips. [Build history](https://github.com/marvelfamily/TeoPCE/actions/workflows/release.yml)

## Compatibility

Use a PC Engine or CoreGrafx with **Turbo EverDrive Pro** and a card that already plays your games through the Krikzz menu. The launcher recognizes native `edturbo/menu.dat` files with CRC32 `3d7d8293` or `3a8c8ed7`. Supply your own working CD BIOS. The manager accepts supported HuCards and CUE sheets with their tracks in your own folder structure. Some CD cheat packs require a specific game revision and path.

Only the listed console configurations and games have the hardware coverage described here. The Mac download is for Apple Silicon, with an ad hoc signature. The Windows application is x64 and unsigned. Manual testing of every packaged GUI operation and every game remains open.

[Installation and RC4 recovery](docs/USER-GUIDE.md) · [Machine-readable results](VERIFICATION.json)
