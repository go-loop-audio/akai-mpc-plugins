# Changes against ValleyRackFree

`diff -ru upstream-ref/<dir> src/<dir>` shows all of it. Only one DSP file changed.

| File | Change | Why |
|------|--------|-----|
| `src/Plateau/Dattorro.cpp`, `Dattorro.hpp` | Pre-delay buffer is `sampleRate * 1 s + 16` samples instead of a fixed 192010 | Saves about 1.2 MB per instance. Plateau never asks for more than 1 s. Output is bit-identical (verified). |

Everything else under `src/` (except `src/engine/` and `src/PlateauPlugin.cpp`, which are new) is byte-identical to upstream.

## Upstream behaviour kept on purpose (so it sounds like Plateau)

- **Hold fades over about 1 s, not 2 ms.** The header sets a 2 ms fade, then
  `Dattorro1997Tank::setSampleRate()` overwrites `fadeStep` with `1/sampleRate`.
- **The tank LFO phase offsets do nothing.** `TriSawLFO::process()` never reads
  `phase`, so the four modulators start in sync and only drift apart by their rates.
- **The tank is hard-limited to 44.1 kHz.**
- **Input is summed to mono** (L + R) before the reverb; the stereo image comes
  from the tank's output taps.
- **Damping knobs have a dead half** (see PARAMS.md).
