// Behaviour, robustness and cost checks for PlateauFx (built WITHOUT -DNDEBUG so
// the DSP's own asserts are live).
#include <malloc.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "../src/engine/PlateauFx.hpp"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { ++g_pass; } else { ++g_fail; \
    printf("  FAIL  %s:%d  %s  -- ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

static double energy(const std::vector<float>& v, size_t a, size_t b) {
    double e = 0; for (size_t i = a; i < b && i < v.size(); ++i) e += (double)v[i] * v[i]; return e;
}
static bool allFinite(const std::vector<float>& v, float bound = 1e3f) {
    for (float x : v) { if (!std::isfinite(x) || std::fabs(x) > bound) return false; }
    return true;
}
static uint32_t g_rng = 12345u;
static float frand() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return (float)(g_rng >> 8) / 16777216.f; }

struct Run { std::vector<float> l, r; };
// Run n frames, input = first 'burst' frames of noise then silence, 128-frame blocks.
static Run run(PlateauFx& fx, int n, int burst, float amp = 0.5f) {
    Run o; o.l.assign(n, 0); o.r.assign(n, 0);
    std::vector<float> il(n, 0), ir(n, 0);
    for (int i = 0; i < burst && i < n; ++i) { il[i] = amp * (2 * frand() - 1); ir[i] = amp * (2 * frand() - 1); }
    for (int i = 0; i < n; i += 128) {
        int c = std::min(128, n - i);
        fx.process(&il[i], &ir[i], &o.l[i], &o.r[i], c);
    }
    return o;
}

int main() {
    const int SR = 44100;

    printf("parameter table\n");
    for (int i = 0; i < PlateauFx::NUM_PARAMS; ++i) {
        const auto& p = PlateauFx::kParams[i];
        CHECK(p.def >= p.min && p.def <= p.max, "default of %s out of range", p.key);
        float rt = PlateauFx::toNative(i, PlateauFx::toNorm(i, p.def));
        CHECK(std::fabs(rt - p.def) < 1e-5f || p.isSwitch, "round trip of %s: %g vs %g", p.key, rt, p.def);
    }
    {
        PlateauFx fx;
        fx.setParam(PlateauFx::P_WET, 5.f);   CHECK(fx.getParam(PlateauFx::P_WET) == 1.f, "clamp high");
        fx.setParam(PlateauFx::P_WET, -5.f);  CHECK(fx.getParam(PlateauFx::P_WET) == 0.f, "clamp low");
        fx.setParam(99, 1.f); fx.setParam(-1, 1.f);   // out-of-range ids must be ignored, not crash
    }

    printf("silence in -> silence out\n");
    {
        PlateauFx fx; Run o = run(fx, SR, 0);
#if PLATEAU_ANTI_DENORMAL
        CHECK(energy(o.l, 0, o.l.size()) < 1e-30 && energy(o.r, 0, o.r.size()) < 1e-30, "output not silent");
#else
        CHECK(energy(o.l, 0, o.l.size()) == 0.0 && energy(o.r, 0, o.r.size()) == 0.0, "output not silent");
#endif
    }

    printf("wet path: stereo, decays, dry/wet split\n");
    {
        PlateauFx fx; fx.setParam(PlateauFx::P_DRY, 0.f); fx.setParam(PlateauFx::P_WET, 1.f);
        Run o = run(fx, SR * 8, 2000);
        CHECK(allFinite(o.l) && allFinite(o.r), "non-finite/huge output");
        double e0 = energy(o.l, SR / 4, SR), e1 = energy(o.l, SR * 3, SR * 4), e2 = energy(o.l, SR * 6, SR * 7);
        CHECK(e0 > 1e-6, "no reverb tail at all (%g)", e0);
        CHECK(e1 < e0 && e2 < e1, "tail not decaying: %g %g %g", e0, e1, e2);
        double d = 0; for (size_t i = 0; i < o.l.size(); ++i) d += std::fabs(o.l[i] - o.r[i]);
        CHECK(d > 1e-3, "left and right identical (no stereo)");
    }
    {   // dry only passes the input straight through
        PlateauFx fx; fx.setParam(PlateauFx::P_DRY, 1.f); fx.setParam(PlateauFx::P_WET, 0.f);
        std::vector<float> in(1024), ol(1024), orr(1024);
        for (auto& x : in) x = 2 * frand() - 1;
        fx.process(in.data(), in.data(), ol.data(), orr.data(), 1024);
        bool same = true; for (int i = 0; i < 1024; ++i) same &= (ol[i] == in[i] && orr[i] == in[i]);
        CHECK(same, "dry-only output differs from input");
    }

    printf("pre-delay\n");
    {
        PlateauFx fx; fx.setParam(PlateauFx::P_DRY, 0.f); fx.setParam(PlateauFx::P_WET, 1.f);
        fx.setParam(PlateauFx::P_PREDELAY, 0.5f);
        Run o = run(fx, SR, 64);
        size_t first = 0; while (first < o.l.size() && o.l[first] == 0.f && o.r[first] == 0.f) ++first;
        CHECK(first >= 22000 && first < 22500, "first wet sample at %zu, expected near 22050", first);
        printf("  (first wet sample at %zu of 22050 expected)\n", first);
    }

    printf("hold\n");
    {
        PlateauFx a, b;
        for (PlateauFx* f : {&a, &b}) { f->setParam(PlateauFx::P_DRY, 0.f); f->setParam(PlateauFx::P_WET, 1.f);
            f->setParam(PlateauFx::P_DECAY, 0.3f); f->setParam(PlateauFx::P_MOD_DEPTH, 0.f); }
        Run pre = run(a, SR / 2, SR / 4);  Run pre2 = run(b, SR / 2, SR / 4);
        (void)pre; (void)pre2;
        a.setParam(PlateauFx::P_HOLD, 1.f);                 // b keeps decaying normally
        // hold has a ~1 s crossfade upstream; look well after it
        Run ha = run(a, SR * 8, 0), hb = run(b, SR * 8, 0);
        double a1 = energy(ha.l, SR * 2, SR * 3), a2 = energy(ha.l, SR * 6, SR * 7);
        double b1 = energy(hb.l, SR * 2, SR * 3), b2 = energy(hb.l, SR * 6, SR * 7);
        printf("  held: %.3g -> %.3g   free: %.3g -> %.3g\n", a1, a2, b1, b2);
        CHECK(a2 > 0.2 * a1 && a1 > 0, "held tail not sustained");
        CHECK(b2 < 0.01 * std::max(b1, 1e-30) || b1 < 1e-25, "free tail should have collapsed");
        CHECK(allFinite(ha.l), "held tail blew up");
    }

    printf("clear\n");
    {
        PlateauFx fx; fx.setParam(PlateauFx::P_DRY, 0.f); fx.setParam(PlateauFx::P_WET, 1.f);
        fx.setParam(PlateauFx::P_DECAY, 0.9f);
        run(fx, SR, SR / 2);
        fx.setParam(PlateauFx::P_CLEAR, 1.f);
        Run c = run(fx, SR, 0);                 // fade out 4 ms, clear, fade in 4 ms
        CHECK(energy(c.l, SR / 10, SR) < 1e-12, "tail survived clear: %g", energy(c.l, SR / 10, SR));
        fx.setParam(PlateauFx::P_CLEAR, 0.f);
        Run again = run(fx, SR, 4000);          // reverb works again afterwards
        CHECK(energy(again.l, 4000, SR) > 1e-8, "no reverb after clear");
    }

    printf("reset()\n");
    {
        PlateauFx fx; fx.setParam(PlateauFx::P_DRY, 0.f); fx.setParam(PlateauFx::P_WET, 1.f);
        run(fx, SR, SR / 2); fx.reset();
        Run o = run(fx, SR, 0);
        CHECK(energy(o.l, 0, o.l.size()) < 1e-30, "tail survived reset");
    }

    printf("in-place processing equals out-of-place\n");
    {
        PlateauFx a, b;
        std::vector<float> l(4096), r(4096), l2, r2, ol(4096), orr(4096);
        for (int i = 0; i < 4096; ++i) { l[i] = 2 * frand() - 1; r[i] = 2 * frand() - 1; }
        l2 = l; r2 = r;
        for (int i = 0; i < 4096; i += 128) {
            a.process(&l[i], &r[i], &ol[i], &orr[i], 128);
            b.process(&l2[i], &r2[i], &l2[i], &r2[i], 128);
        }
        CHECK(l2 == ol && r2 == orr, "aliasing changes the result");
    }

    printf("fuzz: random parameters every block, 30 s\n");
    {
        PlateauFx fx;
        const int blocks = SR * 30 / 128;
        std::vector<float> il(128), ir(128), ol(128), orr(128);
        bool ok = true; float peak = 0;
        for (int b = 0; b < blocks && ok; ++b) {
            if (b % 8 == 0)
                for (int p = 0; p < PlateauFx::NUM_PARAMS; ++p) {
                    const auto& pi = PlateauFx::kParams[p];
                    if (p == PlateauFx::P_CLEAR) { fx.setParam(p, frand() < 0.1f); continue; }
                    // sometimes exactly at an extreme
                    float u = frand(); u = u < .1f ? 0.f : (u > .9f ? 1.f : u);
                    fx.setParam(p, pi.isSwitch ? (frand() < .3f ? 1.f : 0.f) : pi.min + u * (pi.max - pi.min));
                }
            for (int i = 0; i < 128; ++i) { il[i] = 2 * frand() - 1; ir[i] = 2 * frand() - 1; }
            fx.process(il.data(), ir.data(), ol.data(), orr.data(), 128);
            for (int i = 0; i < 128; ++i) {
                if (!std::isfinite(ol[i]) || !std::isfinite(orr[i])) ok = false;
                peak = std::fmax(peak, std::fmax(std::fabs(ol[i]), std::fabs(orr[i])));
            }
        }
        CHECK(ok, "NaN/Inf from random parameters");
        printf("  peak output over fuzz run: %.2f\n", peak);
        CHECK(peak < 1000.f, "output exploded (%g)", peak);
    }

    printf("cost\n");
    {
        struct mallinfo2 m0 = mallinfo2();
        PlateauFx* fx = new PlateauFx();
        struct mallinfo2 m1 = mallinfo2();
        printf("  heap per instance: %.2f MB (sizeof object %zu bytes)\n",
               (double)(m1.uordblks - m0.uordblks) / 1048576.0, sizeof(PlateauFx));

        struct Case { const char* name; float size, depth, speed; };
        for (Case c : {Case{"defaults", .5f, .5f, 0.f}, Case{"max size, max mod", 1.f, 16.f, 1.f}}) {
            fx->setParam(PlateauFx::P_SIZE, c.size); fx->setParam(PlateauFx::P_MOD_DEPTH, c.depth);
            fx->setParam(PlateauFx::P_MOD_SPEED, c.speed);
            std::vector<float> il(128), ir(128), ol(128), orr(128);
            for (int i = 0; i < 128; ++i) { il[i] = 0.5f * (2 * frand() - 1); ir[i] = il[i]; }
            for (int i = 0; i < 2000; ++i) fx->process(il.data(), ir.data(), ol.data(), orr.data(), 128);  // warm up
            const int N = 20000;
            auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < N; ++i) fx->process(il.data(), ir.data(), ol.data(), orr.data(), 128);
            double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / N;
            printf("  %-18s %.1f us per 128-frame block = %.2f%% of the 2902 us block budget (this PC, -O2)\n",
                   c.name, us, 100.0 * us / 2902.0);
        }
        delete fx;
    }

    printf("denormals: sustained cost while a tail dies out (median block time vs steady-input baseline)\n");
    {
        auto median = [](std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; };
        auto timeBlocks = [&](PlateauFx& fx, bool noise, int blocks) {
            std::vector<float> in(128, 0), ol(128), orr(128);
            std::vector<double> t;
            for (int i = 0; i < blocks; ++i) {
                if (noise) for (auto& x : in) x = 0.5f * (2 * frand() - 1);
                auto t0 = std::chrono::steady_clock::now();
                fx.process(in.data(), in.data(), ol.data(), orr.data(), 128);
                t.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
            }
            return t;
        };
        struct Cfg { const char* name; float size, decay, highcut; };
        for (Cfg c : {Cfg{"small size, min decay", 0.f, 0.1f, 10.f},
                      Cfg{"small size, min decay, dark", 0.f, 0.1f, 0.f},
                      Cfg{"default size, min decay", 0.5f, 0.1f, 10.f}}) {
            PlateauFx fx; fx.setParam(PlateauFx::P_DRY, 0.f); fx.setParam(PlateauFx::P_WET, 1.f);
            fx.setParam(PlateauFx::P_SIZE, c.size); fx.setParam(PlateauFx::P_DECAY, c.decay);
            fx.setParam(PlateauFx::P_REV_HIGHCUT, c.highcut); fx.setParam(PlateauFx::P_MOD_DEPTH, 0.f);
            double base = median(timeBlocks(fx, true, 3000));            // steady noise: no denormals possible
            const int blocks = c.size > 0.1f ? 60000 : 20000;            // ~174 s / ~58 s of silence
            std::vector<double> t = timeBlocks(fx, false, blocks);
            double tail = median(t);
            std::vector<double> late(t.end() - blocks / 4, t.end());
            printf("  %-30s steady-input median %.1f us, dying-tail median %.1f us (%.1fx), last quarter %.1f us\n",
                   c.name, base, tail, tail / base, median(late));
#if PLATEAU_ANTI_DENORMAL
            CHECK(tail < 3.0 * base, "sustained denormal slowdown (%s)", c.name);
#endif
        }
    }

    printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
