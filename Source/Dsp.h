#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
namespace jm {
constexpr double pi = 3.14159265358979323846;
inline double db(double x) {
    return 20 * std::log10(std::max(1e-12, std::abs(x)));
}
inline double gain(double x) {
    return std::pow(10., x / 20.);
}
enum Type { Bell, LowShelf, HighShelf, HighPass, LowPass, Notch, BandPass };
struct Coeff {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    static Coeff make(int type, double fs, double hz, double g, double q) {
        hz = std::clamp(hz, 20., fs * .45);
        q = std::clamp(q, .1, 18.);
        g = std::clamp(g, -24., 24.);
        double w = 2 * pi * hz / fs, c = std::cos(w), s = std::sin(w), a = s / (2 * q),
               A = gain(g / 2), r = 2 * std::sqrt(A) * a;
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
        switch (type) {
        case Bell:
            b0 = 1 + a * A;
            b1 = -2 * c;
            b2 = 1 - a * A;
            a0 = 1 + a / A;
            a1 = -2 * c;
            a2 = 1 - a / A;
            break;
        case LowShelf:
            b0 = A * ((A + 1) - (A - 1) * c + r);
            b1 = 2 * A * ((A - 1) - (A + 1) * c);
            b2 = A * ((A + 1) - (A - 1) * c - r);
            a0 = (A + 1) + (A - 1) * c + r;
            a1 = -2 * ((A - 1) + (A + 1) * c);
            a2 = (A + 1) + (A - 1) * c - r;
            break;
        case HighShelf:
            b0 = A * ((A + 1) + (A - 1) * c + r);
            b1 = -2 * A * ((A - 1) + (A + 1) * c);
            b2 = A * ((A + 1) + (A - 1) * c - r);
            a0 = (A + 1) - (A - 1) * c + r;
            a1 = 2 * ((A - 1) - (A + 1) * c);
            a2 = (A + 1) - (A - 1) * c - r;
            break;
        case HighPass:
            b0 = (1 + c) / 2;
            b1 = -(1 + c);
            b2 = b0;
            a0 = 1 + a;
            a1 = -2 * c;
            a2 = 1 - a;
            break;
        case LowPass:
            b0 = (1 - c) / 2;
            b1 = 1 - c;
            b2 = b0;
            a0 = 1 + a;
            a1 = -2 * c;
            a2 = 1 - a;
            break;
        case Notch:
            b0 = 1;
            b1 = -2 * c;
            b2 = 1;
            a0 = 1 + a;
            a1 = -2 * c;
            a2 = 1 - a;
            break;
        case BandPass:
            b0 = a;
            b1 = 0;
            b2 = -a;
            a0 = 1 + a;
            a1 = -2 * c;
            a2 = 1 - a;
            break;
        }
        return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
    }
    double magnitude(double fs, double f) const {
        auto z = std::polar(1., -2 * pi * f / fs);
        return std::abs((b0 + b1 * z + b2 * z * z) / (1. + a1 * z + a2 * z * z));
    }
};
struct Filter {
    double z1 = 0, z2 = 0;
    double tick(double x, const Coeff &c) {
        double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        if (!std::isfinite(y)) {
            z1 = z2 = 0;
            return 0;
        }
        if (std::abs(z1) < 1e-25)
            z1 = 0;
        if (std::abs(z2) < 1e-25)
            z2 = 0;
        return y;
    }
};
struct Settings {
    bool enabled = false, dynamic = false;
    int type = Bell, route = 0, stages = 1;
    double hz = 1000, gainDb = 0, q = .707, threshold = -24, ratio = 2, attack = 10, release = 120,
           knee = 6, range = 4;
};
struct Band {
    std::array<std::array<Filter, 4>, 2> f{};
    std::array<Filter, 2> detector{};
    double env = 0, reduction = 0, smoothedGain = 0, smoothedHz = 1000, smoothedQ = .707;
    Coeff c{}, dc{};
    void update(const Settings &p, double fs, double level) {
        double a = std::exp(-32. / (fs * (level > env ? p.attack : p.release) * .001));
        env = a * env + (1 - a) * level;
        double over = db(env) - p.threshold, k = std::max(.01, p.knee),
               v = over <= -k / 2  ? 0
                   : over >= k / 2 ? over
                                   : (over + k / 2) * (over + k / 2) / (2 * k);
        reduction = p.dynamic ? std::min(p.range, v * (1 - 1 / std::max(1., p.ratio))) : 0;
        double s = 1 - std::exp(-32. / (fs * .02));
        smoothedGain += s * (p.gainDb - reduction - smoothedGain);
        smoothedHz += s * (p.hz - smoothedHz);
        smoothedQ += s * (p.q - smoothedQ);
        c = Coeff::make(p.type, fs, smoothedHz, smoothedGain, smoothedQ);
        dc = Coeff::make(BandPass, fs, smoothedHz, 0, smoothedQ);
    }
    std::array<double, 2> tick(double l, double r, const Settings &p) {
        if (!p.enabled)
            return {l, r};
        double x = l, y = r;
        if (p.route >= 3) {
            x = (l + r) * .5;
            y = (l - r) * .5;
        }
        auto process = [&](double v, int ch) {
            for (int j = 0; j < std::clamp(p.stages, 1, 4); ++j)
                v = f[ch][j].tick(v, c);
            return v;
        };
        if (p.route == 0) {
            x = process(x, 0);
            y = process(y, 1);
        } else if (p.route == 1 || p.route == 3)
            x = process(x, 0);
        else
            y = process(y, 1);
        if (p.route >= 3)
            return {x + y, x - y};
        return {x, y};
    }
};
} // namespace jm
