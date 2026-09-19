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
3. Copy `plugins/DyingLightHeadTracking.ini` into the game folder. This is
   optional, the mod writes a default one on first run.

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

| Action                        | Nav-cluster | Chord          |
|-------------------------------|-------------|----------------|
| Toggle tracking               | `End`       | `Ctrl+Shift+Y` |
| Cycle tracking mode           | `Page Up`   | `Ctrl+Shift+G` |
| Toggle yaw mode               | `Page Down` | `Ctrl+Shift+H` |
| Toggle crosshair compensation | `Insert`    | `Ctrl+Shift+U` |

Cycle tracking mode steps through full tracking, rotation only, and position
only, then back to full.

Toggle yaw mode switches yaw between horizon-locked (the default) and
camera-local.

## Configuration

`DyingLightHeadTracking.ini` sits next to `DyingLightGame.exe` and is written
with its defaults on first run. Delete it to get them back. The pose is used at
1:1, so sensitivity, deadzones and curves belong in your tracker's profile.

```ini
[General]
EnableOnStartup=1
; UDP port the tracker sends OpenTrack packets to.
Port=4242
; How old the newest packet may be before the view holds its last pose.
DataFreshnessMs=500
; Yaw mode: 1 = horizon-locked (default), 0 = camera-local.
WorldSpaceYaw=1
; Move the crosshair onto where the shot actually lands. With this off the
; game's own centre-screen crosshair is left alone and will not match.
ShowReticle=1

[Smoothing]
; 0.0 (responsive) - 1.0 (heavy). Covers rotation and position.
; LocalSmoothing applies to a tracker sending to 127.0.0.1 on this PC;
; RemoteSmoothing applies to any other address, including this PC's own
; network address and a phone on your network.
LocalSmoothing=0
RemoteSmoothing=0.15

[Position]
; Positional tracking, in metres. LimitY bounds travel up and LimitYDown
; down; LimitZ bounds leaning toward the screen and LimitZBack leaning away.
Enabled=1
LimitX=0.3
LimitY=0.2
LimitYDown=0.2
LimitZ=0.4
LimitZBack=0.1

[Collision]
; Stop a lean at walls and props instead of pushing the view through them.
CollisionEnabled=0
; How far the leaned view is held off a surface, in metres.
CollisionRadius=0.15
; How gently a lean opens back up once an obstruction clears (0.0 - 1.0).
CollisionReleaseSmoothing=0.9

[Diagnostics]
; Detailed trace in the log twice a second, plus a screenshot through the
; game's renderer whenever DyingLightHeadTracking.shot appears next to the ini.
Verbose=0
; Applies the pose in menus, pauses and cutscenes too. For testing the camera
; hook only; leave it off to play.
IgnoreGameplayGate=0

[Hotkeys]
; Virtual-key codes. End = toggle, Page Up = cycle tracking mode,
; Page Down = yaw mode, Insert = crosshair compensation.
Toggle=0x23
CycleMode=0x21
YawMode=0x22
Reticle=0x2D
; Chord alternatives: Ctrl+Shift+Y / G / H / U for the same four actions.
ChordToggle=1
ChordCycleMode=1
ChordYawMode=1
ChordReticle=1
```

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

Run `uninstall.cmd`. This removes the mod files. Ultimate ASI Loader is only
removed if the installer put it there. Use `uninstall.cmd /force` to remove it
anyway.

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
