# Qt mobile interface

Touch-friendly Qt 6 / QML front end for small screens (Plasma Mobile and similar).
It lives next to the wxWidgets interface and is started with `Cemu --mobile-ui`
(or `CEMU_MOBILE_UI=1`). Build it with `-DENABLE_QT_MOBILE_UI=ON`.

What it does today:

- game list (grid with icons), refresh, launch, game folder management
- controllers: pick the SDL controller for player 1, default mapping, button by button mapping, live button test
- on-screen gamepad over the game (multi-touch, drawn by the core overlay, press virtual keys of the Keyboard device),
  with a layout editor (drag, resize, reset) opened from the controllers page
- game rotation: the game image can be rotated 90 degrees (landscape on a portrait screen) inside the app, without
  rotating the Plasma output. The menus stay as they are
- accounts: list, active account, create / edit / delete, network service, online files check
- title manager: list, install from a folder, delete, convert to .wua
- emulated USB devices: Skylanders portal, Disney Infinity base, LEGO Dimensions toypad
- graphic packs: download / update the community packs, packs of a game, packs for all games, search in all packs
- per game settings (game profile): CPU mode, thread quantum, shader multiplication, precompiled shaders, shared
  libraries, run ahead of the GPU, limit to 30 FPS
- settings, About
- navigation with a gamepad (D-pad / left stick, A = select, B = back, Y = game menu)
- full screen Vulkan render window with keyboard, mouse and touch forwarding

Not ported from the Android port: custom GPU drivers (Mesa is used), hotkeys, saves manager. The sticks of the
on-screen gamepad are digital (8 directions). These stay in the desktop interface; both share the same configuration
files.

## Layout

| File | Role |
| --- | --- |
| `MobileBackend.*` | Bridge to the emulator core. Standard C++ types only, no Qt. |
| `QtMobileUI.*` | Entry point, creates the Qt application and the QML engine. |
| `Controller.*` | Object exposed to QML as `cemu`. |
| `MobileAccounts.cpp`, `MobileGraphicPackDownload.cpp`, `MobileUSBDevices.cpp`, `MobileTitles.cpp`, `MobileTouchGamepad.cpp`, `MobileInputMapping.cpp`, `MobileGameProfile.cpp` | Parts of the bridge, one per feature. |
| `GameListModel.*` | List model behind the game grid. |
| `GameWindow.*` | Native window the emulator renders into. |
| `qml/` | The pages. |

The Qt code never includes Cemu headers and the bridge never includes Qt headers:
mixing them in one translation unit clashes (X11/GTK macros, integer typedefs, `signals`/`slots`).

Leaving a game restarts the process to get back to the list, because stopping a title
and starting another one in the same process is not reliable in the core.

## Menus available in the mobile interface

- **⋮ › Settings**: console language, screen saver, intro sound, VSync, run ahead of the GPU, 30 FPS limit,
  game rotation, scaling and filters, audio output device / volume / channels / latency, overlay and notifications.
- **⋮ › Accounts / Title manager / Graphic packs / Emulated USB devices / About**.
- **Long press on a game**: start, game settings, graphic packs of that game (enable + presets), remove shader caches.

Game rotation and the on-screen gamepad layout are stored in `settings.xml` (`GameRotation`) and
`touch_gamepad_layout.txt` (config folder). Per game settings are stored in `gameProfiles/<title id>.ini`.

Core files touched for these features: `LatteTouchGamepad.*` (new) and `LatteOverlay.cpp` (gamepad),
`VulkanRenderer.cpp` and `RendererOuputShader.cpp` (rotation), `GameProfile.*` and `GX2_Event.cpp` (per game options).

Everything is stored in the same `settings.xml` as the desktop interface.

## PGO (profile-guided optimization)

1. Run the workflow manually (Actions › Run workflow) with `pgo = generate`, install the package.
2. On the phone: `LLVM_PROFILE_FILE="$HOME/cemu-%m.profraw" Cemu --mobile-ui`, play a few minutes with several games, quit the game normally.
3. `sudo pacman -S llvm`, then `llvm-profdata merge -output=cemu.profdata ~/cemu-*.profraw`.
4. Commit `cemu.profdata` to `dist/linux/arch/`, run the workflow with `pgo = use`.

## Built-in frame profiler

`CEMU_PERF_LOG=1 Cemu --mobile-ui` prints one line per second while a game runs:

```
PERF fps=16.2 frame=61.7ms | latte: waiting-for-guest=35.1ms fence=4.2ms async=0.3ms working=22.1ms | guest cores idle%: c0=62 c1=57 c2=60 | ...
```

- `waiting-for-guest`: the GPU command thread has nothing to do (the game thread is the bottleneck)
- `fence`: it waits for a condition in memory
- `working`: it is really processing draw calls (Cemu + Vulkan driver)
- `guest cores idle%`: share of the second a PPC core had no runnable thread
