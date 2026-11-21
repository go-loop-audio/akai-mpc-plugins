# Parameters

Order below = `PlateauFx::Param` order = the order to number them in the host.
**Once a plugin ships, only append.** Units are the Rack knob units; use
`PlateauFx::toNative()` / `toNorm()` to go to and from 0..1.

| # | key | range | default | notes |
|---|-----|-------|---------|-------|
| 0 | `dry` | 0..1 | 1 | linear dry level |
| 1 | `wet` | 0..1 | 0.5 | reverb level. Internally x10 against a x0.1 input scale, so net gain is about unity |
| 2 | `predelay` | 0..0.5 s | 0 | |
| 3 | `in_lowcut` | 0..10 | 10 | Full travel: low-cut pitch = 5 - p/2 (p=10 is off, 13.75 Hz); freq = 440 * 2^(pitch - 5). Faithful build: 10 - clamp(p + 5, 0, 10), so 5..10 is off |
| 4 | `in_highcut` | 0..10 | 10 | Full travel: high-cut pitch = 5 + p/2 (p=10 is open, 14 kHz). Faithful build: clamp(p + 5, 0, 10), so 5..10 is open |
| 5 | `size` | 0..1 | 0.5 | normal: size^2 mapped to 0.01..4. Tuned: 0.0025 * 2^(5p) |
| 6 | `diffusion` | 0..10 | 10 | tank allpass gains |
| 7 | `decay` | 0.1..0.9999 | 0.64995 (faithful build: 0.54995) | effective value = p (faithful build: clamp(p + 0.1, 0.1, 0.9999)); loop gain = 1 - (1 - eff)^2 |
| 8 | `rev_lowcut` | 0..10 | 10 | same mapping as `in_lowcut`, inside the tank |
| 9 | `rev_highcut` | 0..10 | 10 | same mapping as `in_highcut`, inside the tank |
| 10 | `mod_speed` | 0..1 | 0 | rate multiplier = 1 + 99 p^2 |
| 11 | `mod_shape` | 0..1 | 0.5 | LFO reverse point 0.001..0.999 |
| 12 | `mod_depth` | 0..16 | 0.5 | samples of allpass delay modulation |
| 13 | `tuned` | 0/1 | 0 | Tuned mode: tiny sizes, "play" the reverb as a pitched resonator |
| 14 | `diffuse_input` | 0/1 | 1 | input diffusers on/off |
| 15 | `hold` | 0/1 | 0 | freeze the tank (about 1 s crossfade) |
| 16 | `clear` | 0/1 | 0 | rising edge: 4 ms fade out, clear tank, 4 ms fade in |

## Quirks inherited from the Rack module

- **Damping knobs and Decay: full travel (differs from Rack).** In Rack, only the
  lower half of each damping knob does anything: the module adds 5 to the knob
  value and clamps (a CV offset; Rack has CV inputs to use the headroom, a
  hardware knob does not). Low cuts sweep 440 Hz -> off over knob 0 -> 5, then stay
  off; high cuts sweep 440 Hz -> open over 0 -> 5, then stay open. Decay has a
  similar +0.1 offset, so the top ~11% of its knob is flat.
  Here the four damping knobs and Decay use their whole throw and reach the same
  extremes Rack does without CV (low cuts 440 Hz -> off, high cuts 440 Hz -> open,
  Decay 0.1 -> 0.9999). The defaults sound identical to Rack's defaults (Decay's
  default sits at about 61%). A given knob position therefore sounds different
  from the same position in Rack. Build with `-DPLATEAU_FULL_TRAVEL=0` for the
  faithful Rack mapping (Decay default 50%); the tests compare that build with
  Valley's code bit for bit.
- **Output level:** hot settings (Tuned mode, long decay) can peak above 1.0
  (2.5 seen in the tests); there is no output clamp or saturator in this engine.
  The Rack module's optional soft-saturation output is not ported.
- **Input sensitivity:** fixed at 0 dB (Rack's default; its -18 dB menu option is not ported).
- **Setting all parameters before the first block matters:** the input high-pass
  inside `Dattorro` is silent until its cutoff has been set. `PlateauFx` does this
  in its constructor.

## In the MPC plugin

`src/PlateauPlugin.cpp` shows friendlier units than the Rack knob units above; `PlateauPlugin::toEngine()` converts.
The parameter order and ids are the same as the engine's, and defaults equal the Rack module's.

| Parameter | Shown as | Engine value |
|-----------|----------|--------------|
| Dry, Wet, Size, Mod Rate | 0..100 % | % / 100 |
| Pre-delay | 0..500 ms | ms / 1000 (seconds) |
| Diffusion | 0..100 % | % / 10 |
| Decay | 0..100 % | 0.1 + % / 100 * 0.8999 (default about 61 %) |
| Mod Shape | -100..+100 % | (% + 100) / 200 |
| Mod Depth | 0..100 % | % * 0.16 (default 3.125 % = Rack's 0.5) |
| In/Rev Low Cut, In/Rev High Cut | Hz, "Off" or "Open" | Rack knob units 0..10 |
| Tuned, In Diffuse, Hold, Clear | On / Off | 0 / 1 |

**Hold** and **Clear** are switches. Clear acts on the Off-to-On change (4 ms fade out, empty the tank, 4 ms
fade in), so turn it off again to re-arm it.
