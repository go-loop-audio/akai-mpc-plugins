// Runs PlateauFx on the same scenarios as ref_upstream.cpp and dumps its output.
// Usage: port_dump <outdir> [blocksize]
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "scenarios.hpp"
#include "../src/engine/PlateauFx.hpp"

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s outdir [blocksize]\n", argv[0]); return 2; }
    const int block = argc > 2 ? atoi(argv[2]) : 128;
    for (const Scenario& sc : makeScenarios()) {
        PlateauFx fx;
        for (int i = 0; i < N_PARAMS; ++i) fx.setParam(i, sc.params[i]);

        std::vector<float> inL(sc.length), inR(sc.length), outL(sc.length), outR(sc.length);
        uint32_t rng = 2463534242u;
        for (int n = 0; n < sc.length; ++n) makeInput(n, inL[n], inR[n], rng);

        size_t ev = 0;
        int n = 0;
        while (n < sc.length) {
            while (ev < sc.events.size() && sc.events[ev].at == n) {
                fx.setParam(sc.events[ev].param, sc.events[ev].value);
                ++ev;
            }
            int next = sc.length;
            if (ev < sc.events.size()) next = sc.events[ev].at;
            int count = std::min(block, next - n);
            fx.process(&inL[n], &inR[n], &outL[n], &outR[n], count);
            n += count;
        }
        std::string path = std::string(argv[1]) + "/" + sc.name + ".f32";
        FILE* f = fopen(path.c_str(), "wb");
        for (int i = 0; i < sc.length; ++i) { float o[2] = {outL[i], outR[i]}; fwrite(o, sizeof(float), 2, f); }
        fclose(f);
        printf("port %-24s %d frames\n", sc.name, sc.length);
    }
    return 0;
}
