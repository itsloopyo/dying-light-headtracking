# Dying Light Head Tracking

![Dying Light running with this mod](https://raw.githubusercontent.com/itsloopyo/dying-light-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Dying Light that moves the view with your head while your mouse or controller keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - look around with your head while your aim stays
  where the mouse put it
- **6DOF head tracking** - yaw, pitch and roll plus positional lean on X, Y and Z
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- [Dying Light](https://store.steampowered.com/app/239140/Dying_Light/) on
  Steam. The installer finds the Steam copy on its own. Copies from other stores
  have not been tested; pass their folder to the installer as shown below.
- A head tracking source: [OpenTrack](https://github.com/opentrack/opentrack)
  with a webcam, a phone app, a VR headset or dedicated hardware.
- Windows 10 or 11, 64-bit.

## Installation

### Lopari

Once this mod is available in Lopari, download [Lopari](https://lopari.app),
choose **Dying Light**, and click **Play with head tracking**.

### Standalone Installer

1. Download the installer ZIP from the
   [Releases page](https://github.com/itsloopyo/dying-light-headtracking/releases).
2. Extract it anywhere.
3. Double-click `install.cmd`. It installs Ultimate ASI Loader and the mod next
   to `DyingLightGame.exe`.
4. Configure OpenTrack to output UDP to `127.0.0.1` port `4242` (see
   [Setting Up OpenTrack](#setting-up-opentrack)).
5. Launch the game.

If the installer can't find your game, point it at the game folder, either with
an environment variable:

```powershell
$env:DYING_LIGHT_PATH = "D:\Games\Dying Light"
.\install.cmd
```

or by passing the path directly:

```powershell
.\install.cmd "D:\Games\Dying Light"
```

The installer targets one copy of the game per run. If you have more than one,
run it again with the other copy's folder.

To find the Steam folder, right-click Dying Light in your library and choose
**Manage > Browse local files**.

`DyingLightGame.exe` opens a chooser offering Dying Light and Dying Light: The
Beast. The mod loads either way and stays inactive until Dying Light itself is
running.

### Manual Installation

Everything goes in the game's root folder, next to `DyingLightGame.exe`:

1. From `vendor/ultimate-asi-loader/` in the installer ZIP, copy `dinput8.dll`
   into the game folder and rename it to `winmm.dll`.
2. Copy `plugins/DyingLightHeadTracking.asi` into the game folder.

The mod creates its settings file, `CameraUnlock.ini`, beside
`DyingLightGame.exe` the first time it starts.

Mod managers do not deploy this mod. Vortex and Mod Organizer 2 install into one
fixed subtree per game, and these files have to sit in the game root beside the
executable, which nothing a manager installs can reach. There is no Nexus
download for that reason.

## Setting Up OpenTrack

The mod listens for OpenTrack UDP pose data on port `4242`, on every network
interface.

1. Install [OpenTrack](https://github.com/opentrack/opentrack/releases).
2. Pick a tracker under **Input**.
3. Set **Output** to **UDP over network**, host `127.0.0.1`, port `4242`.
4. Press **Start**. Tracking and the game can start in either order.

Centring is done in the tracker: use OpenTrack's **Center** bind, your phone
app's center button, or SteamVR's reset.

### VR Headset Setup

1. Connect the headset over Air Link, Virtual Desktop or a link cable.
2. Start SteamVR.
3. In OpenTrack, set **Input** to the SteamVR tracker.
4. Leave **Output** on UDP `127.0.0.1`, port `4242`.

### Webcam Setup

Select OpenTrack's **neuralnet tracker** under **Input** and pick your camera in
its settings. It needs no markers and no IR hardware. Use the output settings
above.

### Phone App Setup

The mod accepts one thing: OpenTrack UDP on port `4242`. A phone app works here
if it sends that protocol itself, or ships a PC-side companion that does. Check
your app against that.

What decides the wiring is how much filtering the app does on the phone before
the packet leaves it:

- **An app that filters on-device** can send straight to this PC's LAN IP on
  port `4242`.
- **A raw or lightly filtered feed** sent direct will jitter, because the mod's
  smoothing is sized for a clean signal, not to rescue a noisy one. Send it to
  OpenTrack instead and let OpenTrack's filters and curves clean it up before it
  goes on to `127.0.0.1:4242`.

The test: send direct, hold your head still, and if the view drifts or shakes,
route it through OpenTrack. I made [Headcam](https://headcam.app) so decent
tracking was free for anybody with a phone already in their pocket. It filters
on-device, so it can send direct, and any other app that filters as well works
the same way.

A phone on WiFi is a remote connection and gets `RemoteSmoothing`. So does a
tracker on this same PC that sends to its LAN address instead of `127.0.0.1`,
because the mod tells local from remote by the address the packets arrive on,
not by which machine sent them.

## Controls

The two columns are equivalent - use whichever your keyboard has.

| Action              | Nav-cluster | Chord          |
|---------------------|-------------|----------------|
| Toggle tracking     | `End`       | `Ctrl+Shift+Y` |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+G` |
| Toggle yaw mode     | `Page Down` | `Ctrl+Shift+H` |

Cycle tracking mode steps through full tracking, rotation only, and position
only, then back to full.

Toggle yaw mode switches yaw between horizon-locked (the default) and
camera-local.

The tracking mode and the yaw mode are saved to `CameraUnlock.ini` as soon as
you change them, and come back at the next start. `End` / `Ctrl+Shift+Y`
changes the current session only: whether tracking is on at startup is
`EnableOnStartup`.

Each action's keys are a list in the `[Hotkeys]` section of `CameraUnlock.ini`,
the chord included, so any of them can be rebound or removed.

The game's crosshair follows where your shot lands while your head is turned.
There is no setting or key that turns this off.

## Configuration

The pose is used at 1:1, so sensitivity, deadzones and curves belong in your
tracker's profile.

Apart from creating `CameraUnlock.ini` at startup when there is none, the mod
writes to it only when a hotkey changes the tracking mode or the yaw mode. It
never writes `DyingLightHeadTracking.ini`, and it creates `Defaults.ini` only
when there is none and never changes it. Edit `CameraUnlock.ini` with the game
closed.

<!-- cameraunlock:config -->
The mod reads its settings from `CameraUnlock.ini` in the game folder, and creates the file when it starts and finds none. Edit it with any text editor.

A setting set to `default` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.

`Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.

When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that. Edit it with any text editor.

Earlier versions of the mod kept these settings in `DyingLightHeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `DyingLightHeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `DyingLightHeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.

A setting that the defaults below set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it. `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.

Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:

- Reticle settings, and a key that toggled the reticle.
- A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default.

An older version of the mod reads `DyingLightHeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `DyingLightHeadTracking.ini`.

Deleting only `CameraUnlock.ini` makes the next start read `DyingLightHeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults below. Every setting they set to `default` then follows `Defaults.ini`.

The built-in value of each setting set to `default` below:

- `UdpPort=4242`
- `EnableOnStartup=true`
- `WorldSpaceYaw=true`
- `RotationEnabled=true`
- `DataFreshnessMs=500`
- `LocalSmoothing=0.0`
- `RemoteSmoothing=0.15`
- `PositionEnabled=true`
- `PositionLimitX=0.3`
- `PositionLimitY=0.2`
- `PositionLimitYDown=0.2`
- `PositionLimitZ=0.4`
- `PositionLimitZBack=0.1`
- `CollisionEnabled=true`
- `CollisionReleaseSmoothing=0.9`
- `ToggleKey=End, Ctrl+Shift+Y`
- `CycleTrackingModeKey=PageUp, Ctrl+Shift+G`
- `YawModeKey=PageDown, Ctrl+Shift+H`

With every setting at its default, the file reads:

```ini
; Dying Light head tracking settings.
; Comments start with ; and go on their own line. Text after a value is part of the value.
; Hotkeys are key names such as End, PageUp or Ctrl+Shift+Y. Separate several with commas; leave empty for none.
; A setting set to default takes its value from Defaults.ini, which every head tracking mod
; that keeps its settings in CameraUnlock.ini reads: %AppData%\CameraUnlock\Defaults.ini on
; Windows, $XDG_CONFIG_HOME/CameraUnlock/Defaults.ini (normally ~/.config/CameraUnlock) on
; Linux, under Wine and Proton too, and ~/Library/Application Support/CameraUnlock/Defaults.ini
; on macOS. The log names the file it read. Write a value instead of default to change that
; setting for this game only.

[CameraUnlock]
; Written by the mod. Leave this section in place.
ConfigFormat=1

[Network]
; UDP port the mod receives tracker data on (OpenTrack protocol).
UdpPort=default

[General]
; true: head tracking is on when the game starts. ToggleKey turns it on and off.
EnableOnStartup=default
; true: yaw turns around the world's up axis. false: around the camera's own up axis.
WorldSpaceYaw=default
; true: turning your head turns the view.
; Tracking mode at startup, with PositionEnabled. The mode hotkey changes both.
RotationEnabled=default
; Milliseconds a tracker packet stays current. Once the tracker has sent nothing
; for this long, the mod stops following it until data arrives again.
DataFreshnessMs=default

[Smoothing]
; Smoothing when the tracker runs on this PC. 0 is the least, 1 the most.
LocalSmoothing=default
; Smoothing when the tracker is another device on the network, such as a phone.
; 0 is the least, 1 the most.
RemoteSmoothing=default

[Position]
; true: moving your head moves the view.
; Tracking mode at startup, with RotationEnabled. The mode hotkey changes both.
PositionEnabled=default
; How far, in metres, leaning left or right can move the view.
PositionLimitX=default
; How far, in metres, raising your head can move the view.
PositionLimitY=default
; How far, in metres, lowering your head can move the view.
PositionLimitYDown=default
; How far, in metres, leaning forward can move the view.
PositionLimitZ=default
; How far, in metres, leaning back can move the view.
PositionLimitZBack=default
; true: leaning stops at walls instead of moving the view through them.
CollisionEnabled=default
; How far, in metres, the view is held off a wall when you lean into it.
CollisionMargin=0.15
; How gently the view eases back out after a wall stopped a lean.
; 0 is the quickest, 1 the slowest.
CollisionReleaseSmoothing=default

[Hotkeys]
; Turns head tracking on and off.
ToggleKey=default
; Changes the tracking mode: rotation and position, rotation only, position only.
CycleTrackingModeKey=default
; Switches yaw between the world's up axis and the camera's own (WorldSpaceYaw).
YawModeKey=default

[Diagnostics]
; true: write a detailed trace to the log twice a second, and take a screenshot through
; the game's own renderer whenever a file named DyingLightHeadTracking.shot appears next
; to this one. For troubleshooting; it makes the log grow quickly.
Verbose=false
; true: apply the head pose even where the mod would normally stand down: the front end,
; a pause, a cutscene. It exists to prove the camera hook works on a machine where getting
; into gameplay is awkward, and it is not a way to have head tracking in menus. Leave it
; false to play.
IgnoreGameplayGate=false
```
<!-- /cameraunlock:config -->

## Troubleshooting

The mod writes `DyingLightHeadTracking.log` next to `DyingLightGame.exe`. It
says whether the loader engaged, whether the engine resolved, whether the camera
hook is firing and why tracking is or is not being applied.

**Mod not loading**

- No log file at all means the ASI loader is not loading. Check that
  `winmm.dll` and `DyingLightHeadTracking.asi` are both next to
  `DyingLightGame.exe`.
- A log line saying the mod is staying dormant means an engine function it needs
  is missing on this game build. The game runs unmodified; check the Releases
  page for an updated mod.

**No tracking response**

- Check that your tracker sends to port `4242` and that nothing else on the
  machine has taken that port.
- Tracking only runs in gameplay. If the log says `gate: front end` while you
  are playing, the level is being read as a menu; send the log on Discord.
- Press `End` in case tracking was toggled off.

**Jittery / unstable tracking**

- Raise `LocalSmoothing` or `RemoteSmoothing`, whichever applies to your
  connection (see [Configuration](#configuration)).
- For a phone app, route it through OpenTrack and use its filters.

**Game's Tobii eye tracking options do nothing**

- That is intended. The mod blocks the game from connecting to Tobii EyeX,
  because Extended View turns the camera towards your gaze and would fight your
  head tracking. The log says `the game asked to start EyeX and was refused`
  when this happens. Remove the mod to use the game's eye tracking features.

**Wrong rotation axis**

- The mod applies the pose as the tracker sends it. Check that no axis is
  inverted or remapped in your tracker's profile.
- If yaw feels wrong when looking far up or down, toggle between horizon-locked
  and camera-local yaw with `Page Down`.

## Updating

Download the new release and run `install.cmd` again. Your config is preserved.

## Uninstalling

Run `uninstall.cmd`. This removes the mod files and keeps `CameraUnlock.ini`
and `DyingLightHeadTracking.ini`, so your settings survive a reinstall. Ultimate
ASI Loader is only removed if the installer put it there. Use
`uninstall.cmd /force` to remove it anyway.

## Building from Source

Prerequisites: [pixi](https://pixi.sh), CMake, and Visual Studio with the C++
desktop workload. No copy of the game is needed to build.

```powershell
git clone --recursive https://github.com/itsloopyo/dying-light-headtracking.git
cd dying-light-headtracking
pixi run test      # unit tests
pixi run package   # the installer ZIP, exactly as CI builds it
pixi run install   # build and deploy to every copy of the game on this machine
```

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details.

## Credits

- [Techland](https://techland.net/) - developer of Dying Light
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) by
  ThirteenAG - loads the mod into the game
- [OpenTrack](https://github.com/opentrack/opentrack) - the head tracking
  protocol and software
- [MinHook](https://github.com/TsudaKageyu/minhook) by Tsuda Kageyu - function
  hooking
- [cameraunlock-core](https://github.com/itsloopyo/cameraunlock-core) - shared
  head tracking library

See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for licences.

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by Techland. Use at
your own risk.
