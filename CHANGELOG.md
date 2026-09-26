# Changelog

## [Unreleased]

### Added

- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.

### Changed

- Settings move to `CameraUnlock.ini`, beside `DyingLightGame.exe`. Earlier versions of the mod kept these settings in `DyingLightHeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `DyingLightHeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `DyingLightHeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - Reticle settings, and a key that toggled the reticle.
  - The `CollisionEnabled=0` that earlier versions shipped because the lean collision check was untested. It is written as `default`, so it follows `Defaults.ini`. A `CollisionEnabled=1` you set is carried over.
- An older version of the mod reads `DyingLightHeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `DyingLightHeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `DyingLightHeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. The import carries over each key you had bound and each chord you had switched on or off, and now each one can be changed or removed like any other key.
- Settings are renamed in `CameraUnlock.ini`: `[General] Port` is `[Network] UdpPort`; the `[Position]` limits are `PositionLimitX`, `PositionLimitY`, `PositionLimitYDown`, `PositionLimitZ` and `PositionLimitZBack`; `[Collision] CollisionEnabled`, `CollisionRadius` and `CollisionReleaseSmoothing` are `[Position] CollisionEnabled`, `CollisionMargin` and `CollisionReleaseSmoothing`; and `[Hotkeys] Toggle`, `CycleMode` and `YawMode` with `ChordToggle`, `ChordCycleMode` and `ChordYawMode` are `ToggleKey`, `CycleTrackingModeKey` and `YawModeKey`. `[Position] Enabled` chose the tracking mode at startup; that is now the pair `RotationEnabled` and `PositionEnabled`. `DataFreshnessMs`, the two smoothing settings and `[Diagnostics] Verbose` and `IgnoreGameplayGate` keep their names. The import carries every one of these values over.
- The lean collision check (`CollisionEnabled`) is now on by default, where v0.1.0 shipped it off. `CollisionMargin` keeps the 0.15 metres earlier versions shipped, and a margin you set is carried over.
- The tracking mode that Page Up or Ctrl+Shift+G selects, and the yaw mode that Page Down or Ctrl+Shift+H selects, are now saved to `CameraUnlock.ini` as soon as you change them and come back at the next start. End still changes the current session only.
- The installer no longer copies a default `DyingLightHeadTracking.ini` into the game folder, and `uninstall.cmd` keeps `CameraUnlock.ini` and `DyingLightHeadTracking.ini`, so your settings survive a reinstall. Earlier versions deleted `DyingLightHeadTracking.ini` on uninstall.
- A `DyingLightHeadTracking.ini` whose position limits include one above 10 is not imported, because `CameraUnlock.ini` cannot hold that value. The mod runs on the file's values with the same exceptions as an imported file: the game's crosshair follows the aim, and a `CollisionEnabled=0` takes its value from `Defaults.ini`. It creates no `CameraUnlock.ini`, saves nothing that session, and says so in the log at every start until the value is fixed.
- A `DyingLightHeadTracking.ini` that earlier versions refused to start with, a `[General] Port` outside 1024 to 65535, or a game folder whose path neither the ANSI code page nor an 8.3 short name can spell within 260 characters, is not imported either, and this version does not start until it is fixed, as earlier versions did not. A `UdpPort` in `CameraUnlock.ini` takes any port from 1 to 65535.
- When `CameraUnlock.ini` cannot be created, for example because the game folder cannot be written, the mod runs on the settings it read from `DyingLightHeadTracking.ini`, or on its defaults where there is none, saves nothing that session, and tries again at the next start. Earlier versions did not start at all when there was no `DyingLightHeadTracking.ini` and they could not create one.

### Removed

- The key that toggled the reticle, and the reticle settings: `[General] ShowReticle`, `[Hotkeys] Reticle` and `[Hotkeys] ChordReticle`. The game's crosshair always follows where the shot lands now, and no setting turns that off. Insert and Ctrl+Shift+U no longer do anything in this mod.

## [0.1.0] - 2026-09-19

### Other

- Hello world

## [0.0.0] - 2026-09-17

### Added
- Initial release. Head tracking for Dying Light over the OpenTrack UDP
  protocol: your head moves the view while your mouse or controller keeps
  aiming.
