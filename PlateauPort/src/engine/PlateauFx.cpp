// PlateauFx - see PlateauFx.hpp.
//
// The arithmetic in deriveParams(), tickState() and process() is deliberately
// a line-for-line transcription of Plateau::getParameters()/process() from
// ValleyRackFree (CV inputs at 0 V, so every CV term drops out). Types
// (float vs double) are kept the same so the output stays bit-identical to the
// Rack module; test/test_vs_upstream.cpp checks exactly that.
#include "PlateauFx.hpp"
#include <algorithm>
#include <cmath>

namespace {

constexpr float sizeMin = 0.0025f;
constexpr float sizeMax = 4.0f;
constexpr float decayMax = 0.9999f;
constexpr float modShapeMin = 0.001f;
constexpr float modShapeMax = 0.999f;
constexpr float modDepthMin = 0.f;
constexpr float modDepthMax = 16.f;
constexpr float minus20dBGain = 0.1f;
constexpr float zerodBGain = 1.f;   // Rack's default "input sensitivity"

inline float clampf(float x, float lo, float hi) {
    return std::fmin(std::fmax(x, lo), hi);
}
inline float rescalef(float x, float xMin, float xMax, float yMin, float yMax) {
    return yMin + (x - xMin) / (xMax - xMin) * (yMax - yMin);
}

}  // namespace

const PlateauFx::ParamInfo PlateauFx::kParams[PlateauFx::NUM_PARAMS] = {
    // key              name              min   max      default   switch
    {"dry",           "Dry",            0.f,  1.f,     1.f,       false},
    {"wet",           "Wet",            0.f,  1.f,     0.5f,      false},
    {"predelay",      "Pre-delay",      0.f,  0.5f,    0.f,       false},
    {"in_lowcut",     "Input low cut",  0.f,  10.f,    10.f,      false},
    {"in_highcut",    "Input high cut", 0.f,  10.f,    10.f,      false},
    {"size",          "Size",           0.f,  1.f,     0.5f,      false},
    {"diffusion",     "Diffusion",      0.f,  10.f,    10.f,      false},
    {"decay",         "Decay",          0.1f, 0.9999f, 0.54995f,  false},
    {"rev_lowcut",    "Reverb low cut", 0.f,  10.f,    10.f,      false},
    {"rev_highcut",   "Reverb high cut",0.f,  10.f,    10.f,      false},
    {"mod_speed",     "Mod rate",       0.f,  1.f,     0.f,       false},
    {"mod_shape",     "Mod shape",      0.f,  1.f,     0.5f,      false},
    {"mod_depth",     "Mod depth",      0.f,  16.f,    0.5f,      false},
    {"tuned",         "Tuned mode",     0.f,  1.f,     0.f,       true},
    {"diffuse_input", "Diffuse input",  0.f,  1.f,     1.f,       true},
    {"hold",          "Hold",           0.f,  1.f,     0.f,       true},
    {"clear",         "Clear",          0.f,  1.f,     0.f,       true},
};

PlateauFx::PlateauFx() : reverb_(kSampleRate, 16.0, (double)sizeMax) {
    for (int i = 0; i < NUM_PARAMS; ++i) params_[i] = kParams[i].def;

    reverb_.setSampleRate(kSampleRate);
    envelope_.setSampleRate((float)kSampleRate);
    envelope_.setTime(0.004f);
    envelope_._value = 1.f;

    // Every Dattorro setter must run once before audio: its input high-pass
    // starts with all-zero coefficients and is silent until its cutoff is set.
    deriveParams();
    applyToReverb();
    dirty_ = false;
}

float PlateauFx::toNative(int id, float norm) {
    const ParamInfo& p = kParams[id];
    if (p.isSwitch) return norm > 0.5f ? 1.f : 0.f;
    return p.min + clampf(norm, 0.f, 1.f) * (p.max - p.min);
}

float PlateauFx::toNorm(int id, float native) {
    const ParamInfo& p = kParams[id];
    if (p.isSwitch) return native > 0.5f ? 1.f : 0.f;
    return clampf((native - p.min) / (p.max - p.min), 0.f, 1.f);
}

void PlateauFx::setParam(int id, float v) {
    if (id < 0 || id >= NUM_PARAMS) return;
    const ParamInfo& p = kParams[id];
    v = clampf(v, p.min, p.max);
    if (v != params_[id]) {
        params_[id] = v;
        dirty_ = true;
    }
}

float PlateauFx::getParam(int id) const {
    return (id >= 0 && id < NUM_PARAMS) ? params_[id] : 0.f;
}

void PlateauFx::reset() {
    reverb_.clear();
    envelope_ = LinearEnvelope();
    envelope_.setSampleRate((float)kSampleRate);
    envelope_.setTime(0.004f);
    envelope_._value = 1.f;
    freeze_ = clear_ = fadeOut_ = fadeIn_ = false;
    cleared_ = true;
    if (frozen_) {
        frozen_ = false;
        reverb_.freeze(false);
    }
}

// Plateau::getParameters() computes, per damping knob p (CV at 0 V):
//   low cut : 10 - clamp(p + 5, 0, 10)       high cut : clamp(p + 5, 0, 10)
float PlateauFx::dampPitch(int id, float p) {
#if PLATEAU_FULL_TRAVEL_DAMP
    return isLowCut(id) ? 5.f - p * 0.5f : 5.f + p * 0.5f;
#else
    float v = 0.f;
    v += (p + 5.f);
    v = clampf(v, 0.f, 10.f);
    return isLowCut(id) ? 10.f - v : v;
#endif
}

// Plateau::getParameters(), CV terms removed.
void PlateauFx::deriveParams() {
    const bool tuned = params_[P_TUNED] > 0.5f;
    diffuseInput_ = params_[P_DIFFUSE_INPUT] > 0.5f ? 1 : 0;

    preDelay_ = params_[P_PREDELAY];

    size_ = params_[P_SIZE];
    if (tuned) {
        size_ = sizeMin * powf(2.f, size_ * 5.f);
        size_ = clampf(size_, sizeMin, 2.5f);
    } else {
        size_ *= size_;
        size_ = rescalef(size_, 0.f, 1.f, 0.01f, sizeMax);
        size_ = clampf(size_, 0.01f, sizeMax);
    }

    diffusion_ = clampf(params_[P_DIFFUSION], 0.f, 10.f);

    decay_ = rescalef(0.f, 0.f, 10.f, 0.1f, 0.999f);
    decay_ += params_[P_DECAY];
    decay_ = clampf(decay_, 0.1f, decayMax);
    decay_ = 1.f - decay_;
    decay_ = 1.f - decay_ * decay_;

    inputDampLow_   = dampPitch(P_IN_LOWCUT,   params_[P_IN_LOWCUT]);
    inputDampHigh_  = dampPitch(P_IN_HIGHCUT,  params_[P_IN_HIGHCUT]);
    reverbDampLow_  = dampPitch(P_REV_LOWCUT,  params_[P_REV_LOWCUT]);
    reverbDampHigh_ = dampPitch(P_REV_HIGHCUT, params_[P_REV_HIGHCUT]);

    modSpeed_ = params_[P_MOD_SPEED];
    modSpeed_ = clampf(modSpeed_, 0.f, 1.f);
    modSpeed_ *= modSpeed_;
    modSpeed_ = modSpeed_ * 99.f + 1.f;

    modShape_ = rescalef(params_[P_MOD_SHAPE], 0.f, 1.f, modShapeMin, modShapeMax);
    modShape_ = clampf(modShape_, modShapeMin, modShapeMax);

    modDepth_ = rescalef(0.f, 0.f, 10.f, modDepthMin, modDepthMax);
    modDepth_ += params_[P_MOD_DEPTH];
    modDepth_ = clampf(modDepth_, modDepthMin, modDepthMax);

    dry_ = clampf(params_[P_DRY], 0.f, 1.f);
    wet_ = clampf(params_[P_WET], 0.f, 1.f) * 10.f;
}

// The setter block from Plateau::process().
void PlateauFx::applyToReverb() {
    reverb_.setTimeScale(size_);
    reverb_.setPreDelay(clampf(preDelay_, 0.f, 1.f));
    reverb_.setInputFilterLowCutoffPitch(inputDampLow_);
    reverb_.setInputFilterHighCutoffPitch(inputDampHigh_);
    reverb_.enableInputDiffusion(diffuseInput_ > 0.5f);
    reverb_.setDecay(decay_);
    reverb_.setTankDiffusion(diffusion_);
    reverb_.setTankFilterLowCutFrequency(reverbDampLow_);
    reverb_.setTankFilterHighCutFrequency(reverbDampHigh_);
    reverb_.setTankModSpeed(modSpeed_);
    reverb_.setTankModDepth(modDepth_);
    reverb_.setTankModShape(modShape_);
}

// Hold and Clear handling from Plateau::getParameters(), run once per sample.
void PlateauFx::tickState() {
    freeze_ = params_[P_HOLD] > 0.5f;
    if (freeze_ && !frozen_) {
        frozen_ = true;
        reverb_.freeze(true);
    } else if (!freeze_ && frozen_) {
        frozen_ = false;
        reverb_.freeze(false);
    }

    const bool clearBtn = params_[P_CLEAR] > 0.5f;
    if (clearBtn && !clear_ && cleared_) {
        cleared_ = false;
        clear_ = true;
    } else if (!clearBtn && cleared_) {
        clear_ = false;
    }

    if (clear_) {
        if (!cleared_ && !fadeOut_ && !fadeIn_) {
            fadeOut_ = true;
            envelope_.setStartEndPoints(1.f, 0.f);
            envelope_.trigger();
        }
        if (fadeOut_ && envelope_._justFinished) {
            reverb_.clear();
            fadeOut_ = false;
            fadeIn_ = true;
            envelope_.setStartEndPoints(0.f, 1.f);
            envelope_.trigger();
        }
        if (fadeIn_ && envelope_._justFinished) {
            fadeIn_ = false;
            cleared_ = true;
            envelope_._value = 1.f;
        }
    }
    envelope_.process();
}

void PlateauFx::process(const float* inL, const float* inR,
                        float* outL, float* outR, int n) {
    if (dirty_) {
        deriveParams();
        applyToReverb();
        dirty_ = false;
    }

    for (int i = 0; i < n; ++i) {
        tickState();

        const float leftInput = inL[i];
        const float rightInput = inR[i];
        const float env = envelope_._value;

#if PLATEAU_ANTI_DENORMAL
        const float guard = guardSign_;
        guardSign_ = -guardSign_;
#else
        const float guard = 0.f;
#endif
        reverb_.process(leftInput * minus20dBGain * zerodBGain * env + guard,
                        rightInput * minus20dBGain * zerodBGain * env + guard);

        const float leftOutput = leftInput * dry_ +
                                 reverb_.getLeftOutput() * wet_ * env;
        const float rightOutput = rightInput * dry_ +
                                  reverb_.getRightOutput() * wet_ * env;
        outL[i] = leftOutput;
        outR[i] = rightOutput;
    }
}
