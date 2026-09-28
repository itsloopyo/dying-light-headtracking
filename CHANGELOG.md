# Changelog

## [Unreleased]

### Added

- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.
- Head tracking carries on while you aim down the sights. By default leaning eases out while the sights are up, so it never takes your eye off them, and comes back when you lower them. The mod does not read the aim button: it takes the sights to be up while the game zooms the view in, as it does when you aim. The game also starts each level zoomed in and widens the view over the first seconds, so leaning stays eased out until that ends.
- True free look, `[Position] TrueFreeLook` in `CameraUnlock.ini`, off by default: leaning stays on while you aim, so the weapon stays put and your head moves around it. `Insert` or `Ctrl+Shift+U` (`[Hotkeys] TrueFreeLookKey`) switches it in game, and the mod saves the new value to `CameraUnlock.ini` straight away, so it comes back at the next start.

### Changed

- Settings move to `CameraUnlock.ini`, beside `DyingLightGame.exe`. Earlier versions of the mod kept these settings in `DyingLightHeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `DyingLightHeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `DyingLightHeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as `default` when you never changed it from the default of the earlier version that wrote `DyingLightHeadTracking.ini`, as far as the file shows which version that was, because `DyingLightHeadTracking.ini` does not hold it or holds that default. It then follows `Defaults.ini`, so it takes the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none, which can differ from the default earlier versions used. A setting you changed is written with the value imported for it, or as `default` where that value equals its default at that start.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- A number in `DyingLightHeadTracking.ini` that is not a number the mod can use (`nan`, `inf`) is written as `default` where the defaults the README shows set that setting to `default`, and as the built-in value elsewhere.
- A number in `DyingLightHeadTracking.ini` outside the range a setting takes is brought to the nearest end of that range, and the log says so. Earlier versions took a position limit of any size, and the five position limits take 0 to 10, so a limit above 10 is imported as 10.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - Reticle settings, and a key that toggled the reticle.
  - The `CollisionEnabled=0` that earlier versions shipped because the lean collision check was untested. It is written as `default`, so it follows `Defaults.ini`. A `CollisionEnabled=1` you set is carried over.
  - A hotkey set to Ctrl, Shift or Alt on its own. That key goes down before the key of any chord made with it, so the hotkey is left unbound, and it keeps its Ctrl+Shift chord where it has one.
- An older version of the mod reads `DyingLightHeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `DyingLightHeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `DyingLightHeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. The import carries over each key you had bound and each chord you had switched on or off, and now each one can be changed or removed like any other key.
- Settings are renamed in `CameraUnlock.ini`: `[General] Port` is `[Network] UdpPort`; the `[Position]` limits are `PositionLimitX`, `PositionLimitY`, `PositionLimitYDown`, `PositionLimitZ` and `PositionLimitZBack`; `[Collision] CollisionEnabled`, `CollisionRadius` and `CollisionReleaseSmoothing` are `[Position] CollisionEnabled`, `CollisionMargin` and `CollisionReleaseSmoothing`; and `[Hotkeys] Toggle`, `CycleMode` and `YawMode` with `ChordToggle`, `ChordCycleMode` and `ChordYawMode` are `ToggleKey`, `CycleTrackingModeKey` and `YawModeKey`. `[Position] Enabled` chose the tracking mode at startup; that is now the pair `RotationEnabled` and `PositionEnabled`. `DataFreshnessMs`, the two smoothing settings and `[Diagnostics] Verbose` and `IgnoreGameplayGate` keep their names. The import carries every one of these values over.
- The lean collision check (`CollisionEnabled`) is now on by default, where v0.1.0 shipped it off. `CollisionMargin` keeps the 0.15 metres earlier versions shipped, and a margin you set is carried over.
- Where `DyingLightHeadTracking.ini` put the toggle, the tracking mode or the yaw mode key on Insert, that hotkey keeps Insert, and `TrueFreeLookKey` is written as `Ctrl+Shift+U` alone, so Insert does only what it did before. Otherwise `TrueFreeLookKey` is written as `default`.
- The tracking mode that Page Up or Ctrl+Shift+G selects, and the yaw mode that Page Down or Ctrl+Shift+H selects, are now saved to `CameraUnlock.ini` as soon as you change them and come back at the next start. End still changes the current session only.
- The installer no longer copies a default `DyingLightHeadTracking.ini` into the game folder, and `uninstall.cmd` keeps `CameraUnlock.ini` and `DyingLightHeadTracking.ini`, so your settings survive a reinstall. Earlier versions deleted `DyingLightHeadTracking.ini` on uninstall.
- A `DyingLightHeadTracking.ini` that earlier versions refused to start with, a `[General] Port` outside 1024 to 65535, or a game folder whose path neither the ANSI code page nor an 8.3 short name can spell within 260 characters, is not imported, and this version does not start until it is fixed, as earlier versions did not. A `UdpPort` in `CameraUnlock.ini` takes any port from 1 to 65535.
- When `CameraUnlock.ini` cannot be created, for example because the game folder cannot be written, the mod runs on the settings it read from `DyingLightHeadTracking.ini`, or on its defaults where there is none, saves nothing that session, and tries again at the next start. Earlier versions did not start at all when there was no `DyingLightHeadTracking.ini` and they could not create one.

### Removed

- The key that toggled the reticle, and the reticle settings: `[General] ShowReticle`, `[Hotkeys] Reticle` and `[Hotkeys] ChordReticle`. The game's crosshair always follows where the shot lands now, and no setting turns that off. Insert and Ctrl+Shift+U switch true free look instead.

## [0.1.0] - 2026-09-19

### Other

- Hello world

## [0.0.0] - 2026-09-17

### Added
- Initial release. Head tracking for Dying Light over the OpenTrack UDP
  protocol: your head moves the view while your mouse or controller keeps
  aiming.
