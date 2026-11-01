# FirstVST Delay

A **VST 2.4 audio plug-in** — a stereo ping-pong delay — written from scratch
in C++ against the Steinberg VST SDK 2.4. This repo is the answer to
"*can you make a VST?*": yes — here is a real, loadable plug-in with
parameters, automation, preset persistence and 32/64-bit audio processing,
plus a headless host that verifies it actually works.

```
first-vst/
├── CMakeLists.txt        # builds the plug-in and the test host
├── src/
│   ├── FirstVSTDelay.h   # the plug-in class (AudioEffectX)
│   ├── FirstVSTDelay.cpp # DSP + parameters + persistence
│   └── plugin_main.cpp   # VST entry points / exported symbols
├── host-test/
│   └── host_test.cpp     # headless VST host — loads the .so and tests it
└── sdk/                  # git submodule: VST SDK 2.4 (mirror, see below)
```

## What it does

- **Stereo ping-pong delay**: two delay lines cross-feed their echoes, so a
  sound thrown into the left channel bounces left → right → left → …
- **Parameters** (all automatable):
  | # | Name     | Range                |
  |---|----------|----------------------|
  | 0 | Time     | 0.1 ms – 1000 ms (log) |
  | 1 | Feedback | 0 – 95 %             |
  | 2 | Mix      | 0 – 100 %            |
- **Preset persistence** through VST 2.4 chunks (`effGetChunk`/`effSetChunk`),
  so hosts can save/restore presets.
- **Latency reporting** (`effGetTailSize`) — the host compensates for the
  delay length.
- **32-bit and 64-bit processing** (`processReplacing` /
  `processDoubleReplacing`), stereo I/O, speaker arrangement negotiation.
- No GUI by design — the plug-in is fully controllable from any host
  (automation, MIDI/OSC mapping, presets).

## Building

Requirements: CMake ≥ 3.16 and a C++11 compiler (gcc, clang or MSVC).

```sh
git clone --recursive https://github.com/<you>/first-vst.git
cd first-vst

cmake -S . -B build
cmake --build build
```

The plug-in is written to:

- **Linux**: `build/FirstVSTDelay.so`
- **macOS**: `build/FirstVSTDelay.vst`
- **Windows**: `build/FirstVSTDelay.dll`

> The VST SDK 2.4 is fetched as a git submodule (`git submodule update --init`)
> from a public mirror of the SDK. If you prefer the official SDK, download it
> from Steinberg and build with `-DVST2_SDK_DIR=<path-to-sdk>`.
> Note: the SDK itself is under Steinberg's license; this project's own code is MIT (see LICENSE).

## Testing

A headless host loads the built plug-in exactly like a DAW would
(`dlopen` → `VSTPluginMain`) and checks the protocol end to end: flags,
parameter names/displays, the impulse response of the delay (echo positions
and amplitudes in both float and double precision), latency, chunk
round-trips, program names and identification strings.

```sh
ctest --test-dir build            # via CTest
# or directly:
cmake --build build --target check
```

You should see `ALL CHECKS PASSED`.

## Using it in a DAW

Copy the built plug-in into your host's VST plug-in directory:

- **Linux**: e.g. `~/.vst/plugins/` or Reaper's `~/.Rack/fx/`
- **macOS**: `~/Library/Audio/Plug-Ins/VST/`
- **Windows**: anywhere (e.g. your host's plug-in folder)

Then insert **FirstVST Delay** — route a signal through it and dial in
Time / Feedback / Mix. Since it reports its latency, the host will keep the
dry path time-aligned.

## How it works

- The plug-in class subclasses `AudioEffectX` (VST SDK 2.4) and overrides the
  standard hooks: `open/close`, `setParameter/getParameter`,
  `getParameterName/Display/Label`, `processReplacing/processDoubleReplacing`,
  `getChunk/setChunk`, `getGetTailSize` (the SDK's intentionally misspelled
  method — kept verbatim for API compatibility), and identification methods.
- DSP: two `std::vector<double>` delay lines with fractional read positions
  (linear interpolation). Each delay line is driven by its own input plus the
  echo coming back from the other line — that cross-feed is the ping-pong.
- Buffers are sized to the current delay length and only grow when the
  delay time increases beyond the current capacity.
- `plugin_main.cpp` implements `createEffectInstance()` (called by the SDK's
  `VSTPluginMain`) and exports the legacy `audioMaster*` symbols that ancient
  VST 1.0 hosts expect.

## Extending it

The structure is intentionally small so you can fork it into something real:

- `FirstVSTDelay::processImpl()` — the audio core (template on sample type).
  Change it to build a filter, reverb, distortion, synth…
- Add parameters in `FirstVSTDelay::FirstVSTDelay()` + the
  `getParameter*` methods + extend the chunk format in
  `FirstVSTDelay.cpp`.
- `FirstVSTDelay::getEffectName/getVendorString/getProductString` — your
  branding.

## License

Project code: MIT (see LICENSE). The VST SDK in `sdk/` is under Steinberg's
license and is provided as a submodule, not vendored.
