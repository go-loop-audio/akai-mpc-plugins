// Checks the plugin layer (src/PlateauPlugin.cpp): the friendly values the MPC
// shows (%, ms, Hz) must map onto exactly the engine's native ranges and
// defaults, and the value text must read sensibly. Needs the SDK framework
// headers: make sdk-test SDK=/path/to/mpc-plugin-builder-sdk-main
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../src/PlateauPlugin.cpp"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main() {
    PlateauPlugin pl;
    CHECK(pl.numParams() == PlateauFx::NUM_PARAMS, "param count %d", pl.numParams());

    for (int i = 0; i < pl.numParams(); ++i) {
        const mpc::Param& p = pl.param(i);
        const PlateauFx::ParamInfo& e = PlateauFx::kParams[i];
        CHECK(std::strcmp(p.id.c_str(), e.key) == 0, "param %d id '%s' vs engine key '%s'", i, p.id.c_str(), e.key);
        CHECK(p.name.size() <= 12, "name '%s' longer than 12 characters", p.name.c_str());
        // default maps to the engine default (the Rack module's default)
        float d = PlateauPlugin::toEngine(i, p.def);
        CHECK(std::fabs(d - e.def) < 1e-5f, "%s default %g -> %g, engine default %g", e.key, p.def, d, e.def);
        // ends of the range map to the ends of the engine range
        float lo = PlateauPlugin::toEngine(i, p.min), hi = PlateauPlugin::toEngine(i, p.max);
        CHECK(std::fabs(lo - e.min) < 1e-5f, "%s min %g -> %g, engine min %g", e.key, p.min, lo, e.min);
        CHECK(std::fabs(hi - e.max) < 1e-5f, "%s max %g -> %g, engine max %g", e.key, p.max, hi, e.max);
        // and the mapping is monotonic
        float prev = -1e9f;
        for (int k = 0; k <= 20; ++k) {
            float v = PlateauPlugin::toEngine(i, p.min + (p.max - p.min) * k / 20.f);
            CHECK(v >= prev - 1e-6f, "%s not monotonic", e.key);
            prev = v;
        }
    }

    auto text = [&](int idx, float plain) { std::string s; if (!pl.formatValue(idx, plain, s)) s = "(default)"; return s; };
    struct T { int idx; float v; const char* want; };
    const T cases[] = {
        {PlateauFx::P_DRY, 100.f, "100%"},      {PlateauFx::P_WET, 50.f, "50%"},
        {PlateauFx::P_PREDELAY, 250.f, "250 ms"}, {PlateauFx::P_DECAY, 50.f, "50%"},
        {PlateauFx::P_MOD_SHAPE, 0.f, "0%"},    {PlateauFx::P_MOD_SHAPE, 50.f, "+50%"}, {PlateauFx::P_MOD_SHAPE, -100.f, "-100%"},
        {PlateauFx::P_IN_LOWCUT, 0.f, "440 Hz"}, {PlateauFx::P_IN_LOWCUT, 5.f, "Off"}, {PlateauFx::P_IN_LOWCUT, 10.f, "Off"},
        {PlateauFx::P_IN_HIGHCUT, 0.f, "440 Hz"}, {PlateauFx::P_IN_HIGHCUT, 2.5f, "2.5 kHz"}, {PlateauFx::P_IN_HIGHCUT, 5.f, "Open"},
        {PlateauFx::P_REV_LOWCUT, 2.5f, "78 Hz"}, {PlateauFx::P_REV_HIGHCUT, 10.f, "Open"},
    };
    for (const T& c : cases) {
        std::string got = text(c.idx, c.v);
#if PLATEAU_FULL_TRAVEL_DAMP
        if (c.idx == PlateauFx::P_IN_LOWCUT || c.idx == PlateauFx::P_IN_HIGHCUT ||
            c.idx == PlateauFx::P_REV_LOWCUT || c.idx == PlateauFx::P_REV_HIGHCUT) continue;
#endif
        CHECK(got == c.want, "%s at %g shows '%s', expected '%s'", PlateauFx::kParams[c.idx].key, c.v, got.c_str(), c.want);
    }

    printf(fails ? "plugin mapping: %d FAILED\n" : "plugin mapping: all checks passed\n", fails);
    return fails ? 1 : 0;
}
