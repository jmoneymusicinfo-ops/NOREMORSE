#include "Dsp.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
void check(bool yes, const char *msg) {
    if (!yes) {
        std::cerr << "FAIL " << msg << '\n';
        std::exit(1);
    }
}
int main() {
    int checks = 0;
    auto expect = [&](bool b, const char *s) {
        check(b, s);
        ++checks;
    };
    for (double fs : {44100., 48000., 88200., 96000., 176400., 192000.}) {
        auto bell = jm::Coeff::make(jm::Bell, fs, 1000, 6, 1);
        expect(std::abs(jm::db(bell.magnitude(fs, 1000)) - 6) < .001, "bell center");
        auto hp = jm::Coeff::make(jm::HighPass, fs, 100, 0, .707);
        expect(jm::db(hp.magnitude(fs, 20)) < -25, "highpass rejects rumble");
        auto notch = jm::Coeff::make(jm::Notch, fs, 1000, 0, 2);
        expect(notch.magnitude(fs, 1000) < 1e-8, "notch center");
        for (int type = 0; type < 7; ++type) {
            auto c = jm::Coeff::make(type, fs, 19000, 18, 12);
            jm::Filter f;
            for (int i = 0; i < 20000; ++i) {
                double y = f.tick(i == 0 ? 1 : 0, c);
                expect(std::isfinite(y) && std::abs(y) < 100, "finite impulse");
            }
        }
        jm::Band band;
        jm::Settings p;
        p.enabled = true;
        p.dynamic = true;
        p.threshold = -30;
        p.range = 3;
        p.ratio = 4;
        p.attack = 10;
        p.release = 100;
        for (int i = 0; i < (int)(fs / 32); ++i)
            band.update(p, fs, 1);
        expect(std::abs(band.reduction - 3) < .01, "dynamic range bound");
        for (int i = 0; i < (int)(fs / 32 * 2); ++i)
            band.update(p, fs, 0);
        expect(band.reduction < .01, "release returns");
        p.dynamic = false;
        p.gainDb = 6;
        p.route = 3;
        for (int i = 0; i < 1000; ++i)
            band.update(p, fs, 0);
        auto y = band.tick(1, -1, p);
        expect(std::abs(y[0] - 1) < 1e-10 && std::abs(y[1] + 1) < 1e-10, "mid leaves pure side");
        p.route = 1;
        y = band.tick(.1, .2, p);
        expect(y[1] == .2, "left leaves right");
        p.route = 2;
        y = band.tick(.1, .2, p);
        expect(y[0] == .1, "right leaves left");
        jm::Band attack;
        p.attack = 10;
        p.release = 100;
        p.dynamic = true;
        for (int n = 0; n < (int)(fs * .01 / 32); ++n)
            attack.update(p, fs, 1);
        expect(attack.env > .59 && attack.env < .65, "attack envelope time constant");
        double previous = attack.env;
        for (int n = 0; n < (int)(fs * .1 / 32); ++n)
            attack.update(p, fs, 0);
        expect(attack.env / previous > .35 && attack.env / previous < .39,
               "release envelope time constant");
        jm::Band smooth;
        p.dynamic = false;
        p.gainDb = 18;
        smooth.update(p, fs, 0);
        expect(smooth.smoothedGain > 0 && smooth.smoothedGain < 1,
               "gain smoothing bounded first step");
        p.enabled = false;
        y = band.tick(.1, -.2, p);
        expect(y[0] == .1 && y[1] == -.2, "disabled exact identity");
    }
    auto start = std::chrono::steady_clock::now();
    std::array<jm::Band, 24> b;
    jm::Settings p;
    p.enabled = true;
    for (auto &x : b)
        for (int i = 0; i < 1000; ++i)
            x.update(p, 48000, 0);
    double sample = .1;
    for (int n = 0; n < 480000; ++n)
        for (auto &x : b)
            sample = x.tick(sample, sample, p)[0];
    std::cout << checks << " checks passed. 24 static bands, stereo, 48000 Hz, 480000 samples: "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
              << " seconds; checksum " << sample << '\n';
}
