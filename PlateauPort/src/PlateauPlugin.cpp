// PlateauPlugin.cpp - Plateau plate reverb as an MPC insert effect.
//
// GLA Plateau is based on Plateau, the plate reverb from ValleyRackFree (Dale Johnson, Valley Audio
// Soft; after Dattorro 1997). See NOTICE.md. GPL-3.0-or-later.
//
// The MPC SDK's build.py compiles only the .cpp files that sit directly in
// src/, so this file is the single translation unit: it defines the plugin and
// #includes the engine and the (unmodified) Valley DSP sources at the bottom.
// The tests in test/ compile those same files separately, without the SDK.

// The DSP code uses assert() for debug checks. The MPC has no crash protection,
// so compile them out (the PC tests run with them on).
#ifndef NDEBUG
#define NDEBUG 1
#endif

// The SDK framework flushes denormals around every process() call, so the
// engine's own anti-denormal guard is not needed here (see PlateauFx.hpp).
#define PLATEAU_ANTI_DENORMAL 0

#include <cmath>
#ifndef M_PI                       // some cross toolchains hide M_PI in strict C++17 mode
#define M_PI 3.14159265358979323846
#endif

#include "mpc_plugin.h"
#include "engine/PlateauFx.hpp"

class PlateauPlugin : public mpc::Plugin
{
public:
    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo i;
        i.name = "GLA Plateau";
        i.vendor = "Go Loop Audio";                       // == "manufacturer" in plugin.json
        i.uniqueId = MPC_FOURCC('G', 'L', 'P', 't');      // == "uid" in plugin.json
        i.version = 1000;
        i.isInstrument = false;
        i.numInputs = 2;
        i.numOutputs = 2;
        i.tailSamples = 0;                                // unknown: depends on Decay / Hold
        return i;
    }

    // The order of these parameters IS PlateauFx::Param (see PARAMS.md).
    // Once the plugin has shipped, only ever append. Plain values shown to the
    // user are friendlier than the engine's native Rack units; toEngine()
    // converts. Defaults equal the Rack module's defaults.
    PlateauPlugin() : mpc::Plugin(makeInfo())
    {
        using namespace mpc;
        addParam(floatParam ("dry",           "Dry",          0.f, 100.f, 100.f,   "%",  0));
        addParam(floatParam ("wet",           "Wet",          0.f, 100.f,  50.f,   "%",  0));
        addParam(floatParam ("predelay",      "Pre-delay",    0.f, 500.f,   0.f,   "ms", 0));
        addParam(floatParam ("in_lowcut",     "In Low Cut",   0.f,  10.f,  10.f,   "",   1));
        addParam(floatParam ("in_highcut",    "In High Cut",  0.f,  10.f,  10.f,   "",   1));
        addParam(floatParam ("size",          "Size",         0.f, 100.f,  50.f,   "%",  0));
        addParam(floatParam ("diffusion",     "Diffusion",    0.f, 100.f, 100.f,   "%",  0));
        addParam(floatParam ("decay",         "Decay",        0.f, 100.f,  50.f,   "%",  0));
        addParam(floatParam ("rev_lowcut",    "Rev Low Cut",  0.f,  10.f,  10.f,   "",   1));
        addParam(floatParam ("rev_highcut",   "Rev High Cut", 0.f,  10.f,  10.f,   "",   1));
        addParam(floatParam ("mod_speed",     "Mod Rate",     0.f, 100.f,   0.f,   "%",  0));
        addParam(floatParam ("mod_shape",     "Mod Shape",  -100.f, 100.f,  0.f,   "%",  0));
        addParam(floatParam ("mod_depth",     "Mod Depth",    0.f, 100.f,   3.125f,"%",  0));
        addParam(boolParam  ("tuned",         "Tuned",        false));
        addParam(boolParam  ("diffuse_input", "In Diffuse",   true));
        addParam(boolParam  ("hold",          "Hold",         false));
        addParam(boolParam  ("clear",         "Clear",        false));
        if (numParams() != PlateauFx::NUM_PARAMS) mpc::log("Plateau: parameter count mismatch");
    }

    void prepare(double sampleRate, int /*maxBlockSize*/) override
    {
        // The MPC always runs at 44.1 kHz and the Dattorro tank is fixed to it.
        if (sampleRate != PlateauFx::kSampleRate)
            mpc::log("Plateau: running at %.0f Hz, designed for 44100 Hz (reverb will sound pitched)", sampleRate);
    }

    void reset() override { fx_.reset(); }

    void process(mpc::ProcessContext& ctx) override
    {
        pushParams();

        const float* inL = ctx.inputs[0];
        const float* inR = ctx.numInputs > 1 ? ctx.inputs[1] : ctx.inputs[0];
        float* outL = ctx.outputs[0];
        float* outR = ctx.numOutputs > 1 ? ctx.outputs[1] : ctx.outputs[0];
        fx_.process(inL, inR, outL, outR, ctx.numFrames);   // in place is fine
    }

    // plain value shown to the user -> engine native value
    static float toEngine(int id, float v)
    {
        switch (id)
        {
            case PlateauFx::P_DRY: case PlateauFx::P_WET: case PlateauFx::P_SIZE:
            case PlateauFx::P_MOD_SPEED:  return v * 0.01f;                 // % -> 0..1
            case PlateauFx::P_PREDELAY:   return v * 0.001f;                // ms -> s
            case PlateauFx::P_DIFFUSION:  return v * 0.1f;                  // % -> 0..10
            case PlateauFx::P_DECAY:      return 0.1f + v * 0.01f * (0.9999f - 0.1f);
            case PlateauFx::P_MOD_SHAPE:  return (v + 100.f) * 0.005f;      // -100..100 % -> 0..1
            case PlateauFx::P_MOD_DEPTH:  return v * 0.16f;                 // % -> 0..16
            default:                      return v;                         // knob units and switches
        }
    }

    bool formatValue(int index, float v, std::string& out) override
    {
        char buf[32];
        switch (index)
        {
            case PlateauFx::P_DRY: case PlateauFx::P_WET: case PlateauFx::P_SIZE:
            case PlateauFx::P_DIFFUSION: case PlateauFx::P_DECAY: case PlateauFx::P_MOD_SPEED:
            case PlateauFx::P_MOD_DEPTH:
                std::snprintf(buf, sizeof(buf), "%.0f%%", (double)v); break;
            case PlateauFx::P_MOD_SHAPE:
                if (std::fabs(v) < 0.5f) std::snprintf(buf, sizeof(buf), "0%%");
                else                     std::snprintf(buf, sizeof(buf), "%+.0f%%", (double)v);
                break;
            case PlateauFx::P_PREDELAY:
                std::snprintf(buf, sizeof(buf), "%.0f ms", (double)v); break;
            case PlateauFx::P_IN_LOWCUT: case PlateauFx::P_REV_LOWCUT:
            case PlateauFx::P_IN_HIGHCUT: case PlateauFx::P_REV_HIGHCUT:
            {
                const float pitch = PlateauFx::dampPitch(index, v);
                if (PlateauFx::isLowCut(index) && pitch < 0.001f)       std::snprintf(buf, sizeof(buf), "Off");
                else if (!PlateauFx::isLowCut(index) && pitch > 9.999f) std::snprintf(buf, sizeof(buf), "Open");
                else
                {
                    const float hz = 440.f * std::pow(2.f, pitch - 5.f);
                    if (hz < 1000.f) std::snprintf(buf, sizeof(buf), "%.0f Hz", (double)hz);
                    else             std::snprintf(buf, sizeof(buf), "%.1f kHz", (double)(hz * 0.001f));
                }
                break;
            }
            default: return false;   // On/Off for the switches
        }
        out = buf;
        return true;
    }

private:
    void pushParams()
    {
        for (int i = 0; i < PlateauFx::NUM_PARAMS; ++i) fx_.setParam(i, toEngine(i, get(i)));
    }

    PlateauFx fx_;
};

// Pull in the engine and the Valley DSP sources (one translation unit, see top).
#include "engine/PlateauFx.cpp"
#include "Plateau/Dattorro.cpp"
#include "dsp/filters/OnePoleFilters.cpp"
#include "dsp/modulation/LinearEnvelope.cpp"

MPC_PLUGIN_ENTRY(PlateauPlugin)
