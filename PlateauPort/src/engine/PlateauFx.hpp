// PlateauFx - host-neutral stereo plate reverb engine.
//
// This is the Plateau module from ValleyRackFree (Dale Johnson, Valley Audio
// Soft) with the VCV Rack parts removed: it wraps the unchanged Dattorro DSP
// and reproduces the module's parameter maths (Plateau::getParameters and
// Plateau::process, with every CV input at 0 V).
//
// Licence: GPL-3.0-or-later, see ../../LICENSE and ../../NOTICE.md.
//
// Usage (44.1 kHz only - the Dattorro tank clamps itself to 44.1 kHz):
//
//     PlateauFx fx;
//     fx.setParam(PlateauFx::P_WET, 0.7f);      // native units, see kParams[]
//     fx.process(inL, inR, outL, outR, 128);    // any block size, in-place ok
//
// Parameters are in the "native" units of the Rack knobs. toNative()/toNorm()
// convert to and from a 0..1 value for hosts that only deal in normalised
// parameters. The ORDER of PlateauFx::Param is the order a host should number
// the parameters in; once a plugin ships, only append.
#pragma once

#include "../Plateau/Dattorro.hpp"
#include "../dsp/modulation/LinearEnvelope.hpp"

// Plateau's four damping knobs span 0..10 but the Rack module adds 5 and
// clamps (see Plateau::getParameters), so half of each knob's travel does
// nothing: low-cut knobs only act below the middle, high-cut only below it too.
//   0 (default): faithful - the same knob position sounds the same as in Rack.
//   1          : use the whole knob throw for the same set of cutoffs.
#ifndef PLATEAU_FULL_TRAVEL_DAMP
#define PLATEAU_FULL_TRAVEL_DAMP 0
#endif

// A dying reverb tail passes through the denormal range (< 1e-308 in double),
// where many CPUs - 32-bit ARM VFP in particular - are 10-200x slower. Measured
// on x86: one 128-frame block took 223x longer. With this on, a +/-1e-20
// alternating-sign signal (-400 dB, no DC) is added to the reverb input so the
// tail never decays that far. Output is then not bit-identical to Valley's
// module (differences ~1e-20) and silence in gives ~1e-20 out instead of 0.
// Set to 0 if the host already flushes denormals (e.g. JUCE ScopedNoDenormals).
#ifndef PLATEAU_ANTI_DENORMAL
#define PLATEAU_ANTI_DENORMAL 1
#endif

class PlateauFx {
public:
    enum Param {
        P_DRY,           // 0..1     linear dry level
        P_WET,           // 0..1     reverb level (x10 internally, see process)
        P_PREDELAY,      // 0..0.5   seconds
        P_IN_LOWCUT,     // 0..10    input low cut  (Rack knob units)
        P_IN_HIGHCUT,    // 0..10    input high cut
        P_SIZE,          // 0..1
        P_DIFFUSION,     // 0..10    tank diffusion
        P_DECAY,         // 0.1..0.9999
        P_REV_LOWCUT,    // 0..10    tank low cut
        P_REV_HIGHCUT,   // 0..10    tank high cut
        P_MOD_SPEED,     // 0..1
        P_MOD_SHAPE,     // 0..1
        P_MOD_DEPTH,     // 0..16
        P_TUNED,         // 0/1      Tuned mode
        P_DIFFUSE_INPUT, // 0/1      input diffusion on/off
        P_HOLD,          // 0/1      freeze the tank
        P_CLEAR,         // 0/1      rising edge clears the tank (fades out/in)
        NUM_PARAMS
    };

    struct ParamInfo {
        const char* key;   // stable identifier, never change once shipped
        const char* name;  // display name
        float min, max, def;
        bool isSwitch;
    };
    static const ParamInfo kParams[NUM_PARAMS];

    static constexpr double kSampleRate = 44100.0;

    PlateauFx();

    // Native-unit parameter access. Values are clamped to the parameter range.
    void  setParam(int id, float nativeValue);
    float getParam(int id) const;

    static float toNative(int id, float norm01);
    static float toNorm(int id, float nativeValue);

    // Effective cutoff "pitch" (0..10, freq = 440 * 2^(pitch - 5)) that one of
    // the four damping parameters (P_IN_LOWCUT, P_IN_HIGHCUT, P_REV_LOWCUT,
    // P_REV_HIGHCUT) produces for a native value. Used by the engine itself and
    // by hosts that want to show the cutoff in Hz.
    static float dampPitch(int id, float nativeValue);
    static bool  isLowCut(int id) { return id == P_IN_LOWCUT || id == P_REV_LOWCUT; }

    // Process n frames. Input and output buffers may alias.
    void process(const float* inL, const float* inR,
                 float* outL, float* outR, int n);

    // Silence the tail immediately and restore default internal state.
    // Parameters are kept.
    void reset();

private:
    float params_[NUM_PARAMS];
    bool  dirty_ = true;

    // Derived values (what Plateau::getParameters computes)
    float wet_ = 0.f, dry_ = 1.f;
    float size_ = 1.f, diffusion_ = 1.f, decay_ = 0.f;
    float preDelay_ = 0.f;
    float inputDampLow_ = 0.f, inputDampHigh_ = 10.f;
    float reverbDampLow_ = 0.f, reverbDampHigh_ = 10.f;
    float modSpeed_ = 1.f, modShape_ = 0.5f, modDepth_ = 0.f;
    int   diffuseInput_ = 1;

    // Hold / clear state machine (from Plateau::getParameters)
    bool freeze_ = false, frozen_ = false;
    bool clear_ = false, cleared_ = true, fadeOut_ = false, fadeIn_ = false;

    float guardSign_ = 1e-20f;   // see PLATEAU_ANTI_DENORMAL

    Dattorro reverb_;
    LinearEnvelope envelope_;

    void deriveParams();
    void applyToReverb();
    void tickState();
};
