// Reference: runs the ORIGINAL ValleyRackFree code and dumps its output.
//
// upstream-ref/ holds an untouched copy of Dattorro and its helpers.
// upstream_plateau_struct.inc / upstream_plateau_body.inc are the unmodified
// text of struct Plateau and of Plateau::process()/getParameters()/setLights()
// cut from the Valley repo. Only the VCV Rack base types are replaced by the
// tiny stand-ins below. So this program is the real module's maths and DSP.
//
// Usage: ref_upstream <outdir>      (writes <outdir>/<scenario>.f32, L/R interleaved)
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <algorithm>

#include "scenarios.hpp"

// ---- stand-ins for VCV Rack ------------------------------------------------
struct json_t;
struct ResetEvent {};
struct Param { float v = 0.f; float getValue() const { return v; } void setValue(float x) { v = x; } };
struct Port {
    float volts = 0.f; bool connected = false;
    float getVoltage() const { return volts; }
    float getVoltageSum() const { return volts; }
    bool isConnected() const { return connected; }
    void setVoltage(float x) { volts = x; }
};
struct Light { float value = 0.f; };
struct Module {
    struct ProcessArgs {};
    Param params[64]; Port inputs[32]; Port outputs[4]; Light lights[8];
    virtual ~Module() {}
    virtual void process(const ProcessArgs&) {}
    virtual json_t* dataToJson() { return nullptr; }
    virtual void dataFromJson(json_t*) {}
    virtual void onSampleRateChange() {}
    virtual void onReset(const ResetEvent&) {}
};
template <typename T> T clamp(T x, T a, T b) { return std::fmin(std::fmax(x, a), b); }
inline float rescale(float x, float xMin, float xMax, float yMin, float yMax) {
    return yMin + (x - xMin) / (xMax - xMin) * (yMax - yMin);
}

// ---- original Valley code --------------------------------------------------
#include "../upstream-ref/Plateau/Dattorro.hpp"
#include "../upstream-ref/dsp/modulation/LinearEnvelope.hpp"
#include "../upstream-ref/NonLinear.hpp"

// struct Plateau, verbatim (its first line is "struct Plateau : Module {")
#include "upstream_plateau_struct.inc"

Plateau::Plateau() : reverb(192000, 16, sizeMax) {      // as in Plateau.cpp
    const float def[] = {1.f, .5f, 0.f, 10.f, 10.f, .5f, 10.f, .54995f, 10.f, 10.f,
                         0.f, .5f, .5f};
    (void)def;
    reverb.setSampleRate(44100.0);
    envelope.setSampleRate(44100.0);
    envelope.setTime(0.004f);
    envelope._value = 1.f;
}

// Rack-only overrides (patch save/load, reset). Not part of the audio path.
void Plateau::onSampleRateChange() {}
json_t* Plateau::dataToJson() { return nullptr; }
void Plateau::dataFromJson(json_t*) {}
void Plateau::onReset(const ResetEvent&) {}

// Plateau::process / getParameters / setLights, verbatim
#include "upstream_plateau_body.inc"

// scenario param index -> Rack ParamId
static const int kRackParam[N_PARAMS] = {
    Plateau::DRY_PARAM, Plateau::WET_PARAM, Plateau::PRE_DELAY_PARAM,
    Plateau::INPUT_LOW_DAMP_PARAM, Plateau::INPUT_HIGH_DAMP_PARAM,
    Plateau::SIZE_PARAM, Plateau::DIFFUSION_PARAM, Plateau::DECAY_PARAM,
    Plateau::REVERB_LOW_DAMP_PARAM, Plateau::REVERB_HIGH_DAMP_PARAM,
    Plateau::MOD_SPEED_PARAM, Plateau::MOD_SHAPE_PARAM, Plateau::MOD_DEPTH_PARAM,
    Plateau::TUNED_MODE_PARAM, Plateau::DIFFUSE_INPUT_PARAM,
    Plateau::FREEZE_PARAM, Plateau::CLEAR_PARAM};

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s outdir\n", argv[0]); return 2; }
    for (const Scenario& sc : makeScenarios()) {
        Plateau p;
        for (int i = 0; i < N_PARAMS; ++i) p.params[kRackParam[i]].setValue(sc.params[i]);
        p.inputs[Plateau::LEFT_INPUT].connected = true;
        p.inputs[Plateau::RIGHT_INPUT].connected = true;

        std::string path = std::string(argv[1]) + "/" + sc.name + ".f32";
        FILE* f = fopen(path.c_str(), "wb");
        uint32_t rng = 2463534242u;
        size_t ev = 0;
        Module::ProcessArgs args;
        for (int n = 0; n < sc.length; ++n) {
            while (ev < sc.events.size() && sc.events[ev].at == n) {
                p.params[kRackParam[sc.events[ev].param]].setValue(sc.events[ev].value);
                ++ev;
            }
            float l, r;
            makeInput(n, l, r, rng);
            p.inputs[Plateau::LEFT_INPUT].volts = l;
            p.inputs[Plateau::RIGHT_INPUT].volts = r;
            p.process(args);
            float o[2] = {p.outputs[Plateau::LEFT_OUTPUT].volts,
                          p.outputs[Plateau::RIGHT_OUTPUT].volts};
            fwrite(o, sizeof(float), 2, f);
        }
        fclose(f);
        printf("ref  %-24s %d frames\n", sc.name, sc.length);
    }
    return 0;
}
