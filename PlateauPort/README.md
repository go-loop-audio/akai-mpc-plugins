# PlateauPort

**GLA Plateau** by Go Loop Audio: a plate reverb insert effect for Akai MPC standalone hardware, based on
**Plateau** from ValleyRackFree (Dale Johnson / Valley Audio Soft, after Dattorro 1997), built with Akai's
[MPC Plugin SDK](https://github.com/inmusic-software/mpc-plugin-builder-sdk).
GPL-3.0 - see `NOTICE.md` for credits and the licence checks to do before you publish.

## Status

| Part | State |
|------|-------|
| DSP (Dattorro tank, diffusers, filters, LFOs) | Done. Valley's code, one memory patch (`PATCHES.md`) |
| Engine `src/engine/PlateauFx` | Done. Bit-identical to the Valley module (see Tests) |
| SDK plugin: `src/PlateauPlugin.cpp`, `plugin.json`, `ui.json` | Done. Builds natively; passes the SDK host simulator (20 passed, 0 warnings, 0 failures) |
| 32-bit and 64-bit ARM builds | **Not built yet.** No ARM toolchain where this was written. One command on your Mac, below |
| On an MPC | **Not tested.** CPU, sound and the touch screen layout are unchecked on hardware |

## Build for the MPC

```
cd /Users/simonbosley/Programming/GoLoopAudio/akai-mpc-plugins/mpc-plugin-builder-sdk-main
python3 tools/build.py ../PlateauPort
```

The first run downloads the Zig compiler (about 50 MB). The result is the folder
`PlateauPort/build/Go Loop Audio - VST - GLA Plateau/`. Then, per the SDK's `docs/install-and-test.md`:

1. MPC OS 3.10.1 or later, with Preferences > Enhancements (Beta) > *Enable Enhancements* and *External Plugin Support* on.
2. Copy that folder into a folder named exactly `VST2 Plugins` at the root of a USB drive or SD card (or use `tools/mpc_remote.py`).
3. On the MPC, add GLA Plateau in an insert slot; it is listed under External Plugins.

Save your project first, and test on a project you can afford to lose: third-party plugins have no crash protection.

**Names:** product `GLA Plateau`, manufacturer `Go Loop Audio`, uid `GLPt`. They appear in `plugin.json` and in
`makeInfo()` in `src/PlateauPlugin.cpp` and must match; if you ever change one, change both. The uid must stay the
same once projects use the plugin, or saved projects will not find it. The result folder is
`Go Loop Audio - VST - GLA Plateau`.

## Using it

Three pages, each row of four is one Q-Link group:

| Page | Row 1 | Row 2 |
|------|-------|-------|
| MAIN | Size, Decay, Diffusion, Pre-delay | Dry, Wet, Hold, Clear |
| TONE | In Low Cut, In High Cut, Rev Low Cut, Rev High Cut | In Diffuse, Tuned |
| MOD  | Mod Rate, Mod Shape, Mod Depth | |

`PARAMS.md` explains every parameter, including the quirks inherited from the Rack module (Hold fades over about 1 s; Clear is a switch: turn it on to empty the tank, off to re-arm).

## Skin

`skins/` holds a hand-written skin, and because it exists `build.py` copies it verbatim and does **not** run
`gen_ui.py`. It is generated, not edited by hand: `ui.json` (layout) -> `tools/make_skin.py` -> `skins/` + `assets/`.
The result is committed, so a normal build needs none of the tool's dependencies. To regenerate after editing
`ui.json` or the art (needs Pillow, `rsvg-convert`, and `build/params.json` from `make sdk-test`):

```
python3 tools/make_skin.py --sdk /Users/simonbosley/Programming/GoLoopAudio/akai-mpc-plugins/mpc-plugin-builder-sdk-main
```

Look: Valley's panel grey, Rogan knobs from ValleyRackFree's artwork (`skin-art/rogan/`) in Valley's colour code
(white levels, green filters, blue space, red modulation), "GLA Plateau" header and "Go Loop Audio" footer.
**Clear is a normal switch** for now: turn it on to empty the tank, off to re-arm. A momentary version
(`ResetOnGestureEnd` in the skin) was tried and did not work on the One+; it is parked for later, see `MOMENTARY` in `tools/make_skin.py`.

## Layout

```
plugin.json, ui.json                  SDK manifest and touch-screen layout
skins/, assets/, tools/make_skin.py    the generated skin, its source images, the generator (see Skin)
skin-art/                             panel and knob artwork derived from ValleyRackFree
src/PlateauPlugin.cpp                 the plugin; the only .cpp directly in src/ (the SDK compiles just those)
src/engine/PlateauFx.{hpp,cpp}        host-neutral engine
src/Plateau, src/dsp, src/utilities   Valley DSP (unchanged except PATCHES.md)
upstream-ref/                         untouched Valley copy + extracted Plateau code, for the tests
test/, Makefile                       tests
PARAMS.md, PATCHES.md, NOTICE.md      docs
```

`PlateauPlugin.cpp` pulls the engine and DSP `.cpp` files in with `#include`, because `build.py` only compiles
files directly in `src/`. That keeps Valley's files byte-identical to upstream.

## Build options

Pass to the SDK build with `--define NAME=VALUE`.

| Define | Default | Meaning |
|--------|---------|---------|
| `PLATEAU_FULL_TRAVEL` | 1 | 1 = the four damping knobs and Decay use their whole throw and reach the same range Rack does without CV. 0 = Rack's exact knob mapping (upper half of the damping knobs dead). The value text follows. |
| `PLATEAU_ANTI_DENORMAL` | 0 in the plugin | The SDK framework already flushes denormals around `process()`. The standalone engine defaults to 1; without either, a dying tail ran 26x slower on x86. |

## Tests

Needs `g++` and `python3` with `numpy`.

```
make test                                                       # engine, about 10 s, no SDK needed
make sdk-test SDK=/path/to/mpc-plugin-builder-sdk-main          # plugin layer + the SDK's host simulator
```

1. **Against the real Valley code.** `test/ref_upstream.cpp` compiles the original, unmodified
   `Plateau::process()` / `getParameters()` text and `Dattorro` from Valley, with only the VCV Rack base types stubbed.
   It and `PlateauFx` run 6 scenarios of 4 s (defaults, long/big/modulated, Tuned mode, dark and undiffused,
   Hold, Clear with mid-run parameter moves) at block sizes 128 and 37. Output must be **bit-identical**.
2. **Behaviour and robustness** (`test/test_engine.cpp`, 72 checks): silence, decay, stereo, dry/wet, pre-delay timing,
   Hold, Clear, `reset()`, in-place processing, a 30 s random-parameter fuzz, memory, CPU, denormal cost.
3. **Plugin layer** (`test/test_plugin_mapping.cpp`): the friendly units (%, ms) map exactly onto the engine's
   ranges and defaults, and the value text (Hz, Off, Open) reads correctly.
4. **SDK host simulator**: the MPC's load sequence, block sizes 32 and 512, state save/restore, in-place
   processing, parameter sweep, timing.

Measured on a PC, not an MPC: about 10 us per 128-frame block (0.35% of the 2.9 ms block budget), 1.80 MB heap per instance.

## Next steps

1. Run the build above, install on your MPC and listen.
2. Check CPU on the device. The 32-bit MPCs are the tight case (the SDK suggests staying well under 15% per instance).
   If it is high, a `float` build of the DSP is the next lever.
3. Check the touch-screen skin on the device (see Skin below).
