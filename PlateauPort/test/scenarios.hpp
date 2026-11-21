// Shared by ref_upstream.cpp (original Valley code) and port_dump.cpp (PlateauFx)
// so both are driven by exactly the same parameters, events and input.
#pragma once
#include <cstdint>
#include <vector>

// Parameter ids match PlateauFx::Param order.
enum { N_PARAMS = 17 };

struct Event { int at; int param; float value; };   // 'at' must be a multiple of 128

struct Scenario {
    const char* name;
    float params[N_PARAMS];
    std::vector<Event> events;
    int length;
};

// dry wet pre inLo inHi size diff decay revLo revHi mSpd mShp mDep tuned dIn hold clear
static const int kLen = 44100 * 4;

inline std::vector<Scenario> makeScenarios() {
    std::vector<Scenario> s;
    s.push_back({"defaults",
        {1, .5f, 0, 10, 10, .5f, 10, .54995f, 10, 10, 0, .5f, .5f, 0, 1, 0, 0}, {}, kLen});
    s.push_back({"long_big_modulated",
        {.3f, 1, .3f, 10, 10, 1, 10, .9f, 10, 10, 1, .2f, 16, 0, 1, 0, 0}, {}, kLen});
    s.push_back({"tuned_small",
        {0, 1, 0, 10, 10, .6f, 10, .8f, 10, 10, 0, .5f, 0, 1, 1, 0, 0}, {}, kLen});
    s.push_back({"dark_no_diffusion",
        {.5f, .8f, .05f, 3, 2, .4f, 0, .5f, 4, 6, .3f, .9f, 4, 0, 0, 0, 0}, {}, kLen});
    s.push_back({"hold",
        {1, .6f, 0, 10, 10, .5f, 10, .6f, 10, 10, .1f, .5f, 1, 0, 1, 0, 0},
        {{44032, 15, 1.f}, {110592, 15, 0.f}}, kLen});
    s.push_back({"clear_and_param_moves",
        {1, .5f, 0, 10, 10, .5f, 10, .85f, 10, 10, 0, .5f, .5f, 0, 1, 0, 0},
        {{52992, 16, 1.f}, {88192, 16, 0.f}, {96256, 1, 1.f}, {96256, 5, .9f},
         {128000, 7, .3f}, {150016, 13, 1.f}}, kLen});
    return s;
}

// Deterministic stereo test input: noise bursts and an impulse on the right.
inline void makeInput(int n, float& l, float& r, uint32_t& rng) {
    auto next = [&]() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float)((int32_t)rng) / 2147483648.f;
    };
    float nl = next(), nr = next();           // always advance the generator
    bool burst = n < 6000 || (n >= 88200 && n < 90000);
    l = burst ? 0.5f * nl : 0.f;
    r = burst ? 0.5f * nr : 0.f;
    if (n == 30000) r += 0.8f;
}
