# Contributing to TeoPCE

Thanks for helping improve the experience on real PC Engine hardware.

## Report a problem

Use the issue forms. Include manager and ROM versions, operating system/CPU, console and cartridge, native menu/firmware version, exact steps and error text. For a launch failure, say whether the same image works through Krikzz's menu with all codes off. For cheats, list the precise group names, game region/revision, selection and any stage/character requirement.

Attach a small, redacted report or screenshot when useful. Do not upload games, BIOS, firmware, saves, personal paths, secrets or card dumps. A filename and content hash can identify a revision without distributing it.

## Submit a change

1. Open an issue first for a substantial behaviour change; small focused fixes can go straight to a pull request.
2. Keep the change at the layer that owns the behaviour. Preserve native launch/save paths and the pairing between ROM, catalog, content identity and saved profiles.
3. For a functional fix, include a regression test using synthetic fixtures. Never weaken input validation or drop a whole-group/combination check just to make a code selectable.
4. Run `python -m unittest discover -s tests -v`; report the result and skips. Explain hardware-dependent behaviour separately.
5. Update relevant English/Russian instructions and third-party notices if behaviour or distributed content changes.

Describe the problem, resulting behaviour, tests and remaining uncertainty. Mark hardware testing as performed only when it actually happened. A successful boot is not proof that a cheat works at every stage.

Your TeoPCE code contribution is under the repository's MIT license. Data contributions must have traceable sources and compatible distribution terms; the adapted Libretro database remains CC BY-SA 4.0. Do not add downloaded game artwork without establishing permission to distribute it.

Build instructions and component map: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).
