# FirstVST — VST 2.4 plug-ins

Two real, loadable **VST 2.4 audio plug-ins** written from scratch in C++
against the Steinberg VST SDK 2.4, plus headless hosts that verify them
end to end:

1. **FirstVST Delay** — a stereo ping-pong delay ("can you make a VST?": yes).
2. **Tobjosh Reverb** — a 360° room-audio reverb with a full custom GUI
   (glowing knobs, preset buttons, bypass LED, value displays) and easy
   features: 4 knobs, 4 presets, double-click reset, arrow/wheel nudging.

```
first-vst/
├── CMakeLists.txt        # builds both plug-ins, test hosts and UI tools
├── src/
│   ├── FirstVSTDelay.*   # plug-in 1: ping-pong delay (AudioEffectX)
│   ├── plugin_main.cpp   # VST entry point for FirstVST Delay
│   ├── TobjoshReverb.*   # plug-in 2: 360° reverb (AudioEffectX)
│   ├── tobjosh_plugin_main.cpp
│   ├── presets.h         # Rooms / Hall / Cathedral / Space
│   ├── reverb/
│   │   └── ReverbEngine.*  # FDN reverb DSP (pure, no VST deps)
│   └── gui/              # self-contained UI framework
│       ├── Raster.*          # software rasterizer + 5x7 bitmap font
│       ├── DisplayDriver.h   # abstract render surface
│       ├── SoftwareDisplayDriver.h  # in-memory driver (headless tests)
│       ├── Win32DisplayDriver.h    # GDI driver (Windows)
│       ├── MacDisplayDriver.h      # CoreGraphics driver (macOS)
│       ├── Controls.*        # Knob, Toggle, Button, Label, UI container
│       ├── ReverbUI.*        # the Tobjosh Reverb control surface
│       ├── Editor.*          # AEffEditor (child window on real hosts)
│       └── EditorMac.mm      # macOS Cocoa bridge
├── host-test/
│   ├── host_test.cpp         # headless host for FirstVST Delay
│   ├── tobjosh_host_test.cpp # headless host for Tobjosh Reverb (audio)
│   ├── ui_test.cpp           # headless UI tests (drag, presets, layout…)
│   └── ui_shot.cpp           # renders the UI to a PPM screenshot
└── sdk/                  # git submodule: VST SDK 2.4 (mirror, see below)
```

## Tobjosh Reverb

A **360° room-audio reverb**: the wet signal is positioned anywhere around
the listener with the 360° knob — equal-power panning, with the back
hemisphere folded into the front with attenuation (the room really feels
like it rotates). Early reflections pan around the room; the diffuse tail
is a 4-tap feedback delay network whose decay time follows the Size knob
(T60 ≈ Size), and Damping low-passes the tail.

- **Parameters**:
  | # | Name     | Range        |
  |---|----------|--------------|
  | 0 | Size     | 0.2 – 6.0 s (decay time) |
  | 1 | Damping  | 0 – 100 % (tail brightness) |
  | 2 | 360°     | 0 – 360° (reverb position) |
  | 3 | Mix      | 0 – 100 % (wet amount) |
- **Presets**: Rooms, Hall, Cathedral, Space (VST programs + UI buttons).
- **Bypass** LED toggle (soft bypass, automatable).
- **GUI**: dark theme, glowing rotary knobs (drag, double-click to reset,
  arrow keys / mouse wheel to nudge), value displays, preset buttons.
  The plug-in creates its own child window on real hosts (VST 2.4 hands the
  editor the parent window handle; there is no `effEditMouse` opcode).
- **Chunk persistence** (`effGetChunk`/`effSetChunk`) for host presets.
- **32-bit and 64-bit processing**, stereo I/O.

### Platform notes

- **Windows / macOS**: full editor window (GDI / CoreGraphics drivers).
- **Linux**: audio-only build — the engine and the whole UI framework are
  exercised by the headless tests, but no editor window is created.

## FirstVST Delay

A stereo ping-pong delay: two delay lines cross-feed their echoes, so a
sound thrown into the left channel bounces left → right → left → …

- **Parameters**: Time (0.1 ms – 1000 ms, log), Feedback (0 – 95 %), Mix.
- **Preset persistence** through VST 2.4 chunks.
- **Latency reporting** (`effGetTailSize`) so hosts keep the dry path
  time-aligned.
- **32/64-bit processing**, stereo I/O, speaker arrangement negotiation.

## Building

Requirements: CMake ≥ 3.16 and a C++11 compiler (gcc, clang or MSVC).

```sh
git clone --recursive https://github.com/<you>/first-vst.git
cd first-vst

cmake -S . -B build
cmake --build build
```

The plug-ins are written to:

- **Linux**: `build/FirstVSTDelay.so`, `build/TobjoshReverb.so`
- **macOS**: `build/FirstVSTDelay.vst`, `build/TobjoshReverb.vst`
- **Windows**: `build/FirstVSTDelay.dll`, `build/TobjoshReverb.dll`

> The VST SDK 2.4 is fetched as a git submodule (`git submodule update --init`)
> from a public mirror of the SDK. If you prefer the official SDK, download it
> from Steinberg and build with `-DVST2_SDK_DIR=<path-to-sdk>`.
> Note: the SDK itself is under Steinberg's license; this project's own code is MIT (see LICENSE).

## Testing

```sh
ctest --test-dir build            # all suites
# or directly:
cmake --build build --target check
```

The headless hosts load the built plug-ins exactly like a DAW would
(`dlopen` → `VSTPluginMain`) and check the protocol end to end:

- **FirstVST Delay**: flags, parameter names/displays, the impulse response
  of the delay (echo positions and amplitudes, float + double), latency,
  chunk round-trips, program names, identification strings.
- **Tobjosh Reverb (audio)**: parameters, programs (Rooms/Hall/Cathedral/Space),
  chunk round-trips, reverb decay (T60 ≈ Size, in float + double), 360°
  panning (center / right / left / back folding), bypass passthrough.
- **Tobjosh Reverb (UI)**: renders the editor headlessly and checks the
  dark theme, layout (no overlapping controls), knob drag, double-click
  reset, arrow/wheel nudging, preset buttons, bypass LED, the full-circle
  360° knob.

You should see `ALL CHECKS PASSED`.

A UI screenshot can be rendered with:

```sh
./build/ui-shot tobjosh_reverb_ui.ppm
```

## Using them in a DAW

Copy the built plug-ins into your host's VST plug-in directory:

- **Linux**: e.g. `~/.vst/plugins/` or Reaper's `~/.Rack/fx/`
- **macOS**: `~/Library/Audio/Plug-Ins/VST/`
- **Windows**: anywhere (e.g. your host's plug-in folder)

Then insert **FirstVST Delay** or **Tobjosh Reverb** — route a signal
through it and dial in the parameters. The delay reports its latency so
the host keeps the dry path time-aligned.

## How it works

- Both plug-ins subclass `AudioEffectX` (VST SDK 2.4) and override the
  standard hooks: `open/close`, `setParameter/getParameter`,
  `getParameterName/Display/Label`, `processReplacing/processDoubleReplacing`,
  `getChunk/setChunk`, and identification methods.
- **Tobjosh Reverb DSP** (`src/reverb/ReverbEngine.cpp`): 6 early-reflection
  taps (4–34 ms, room-spread pan angles) + a 4-tap feedback delay network
  with a reciprocal feedback matrix and per-tap damping low-passes. The
  per-tap feedback gain `g = exp(-6.9 · tapLen / size)` makes the overall
  tail decay T60 ≈ Size. Every wet signal is panned with equal-power
  panning at `angle`, folding the back hemisphere forward with
  `|sin(angle)|` attenuation.
- **Tobjosh Reverb GUI** (`src/gui/`): a self-contained framework — a small
  RGBA rasterizer with a 5×7 bitmap font, an abstract `DisplayDriver`
  surface, controls (Knob with drag/double-click/arrow/wheel, Toggle LED,
  Button, Label), the `ReverbUI` layout, and an `AEffEditor` that creates
  its own child window on real hosts. The same UI runs headlessly against
  the software display driver, which is how the UI tests work.
- **FirstVST Delay DSP**: two `std::vector<double>` delay lines with
  fractional read positions (linear interpolation), cross-fed for the
  ping-pong.

## Extending it

The structure is intentionally small so you can fork it into something real:

- `ReverbEngine::process()` — the reverb audio core. Change the taps, the
  feedback matrix or the panning model.
- `src/gui/Controls.*` — add new widgets; they automatically work in the
  headless tests and on real hosts.
- `TobjoshReverb.cpp` — add parameters, programs or chunk fields.
- `FirstVSTDelay::processImpl()` — the delay audio core (template on
  sample type).

## License

Project code: MIT (see LICENSE). The VST SDK in `sdk/` is under Steinberg's
license and is provided as a submodule, not vendored.
