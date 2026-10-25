#include "Dsp.h"
#include "FactoryData.h"
#include "Group.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <mutex>
#include <string_view>
using namespace juce;
namespace {
String id(int i, const char *key) {
    return "band" + String(i + 1) + "_" + key;
}
AudioProcessorValueTreeState::ParameterLayout layout() {
    AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&](String name, float lo, float hi, float def, float skew = 1.f) {
        l.add(std::make_unique<AudioParameterFloat>(
            ParameterID{name, 1}, name, NormalisableRange<float>(lo, hi, 0, skew), def));
    };
    auto b = [&](String name, bool def) {
        l.add(std::make_unique<AudioParameterBool>(ParameterID{name, 1}, name, def));
    };
    auto choice = [&](String name, StringArray items, int def) {
        l.add(std::make_unique<AudioParameterChoice>(ParameterID{name, 1}, name, items, def));
    };
    b("bypass", false);
    b("auto_gain", true);
    f("output_db", -24, 12, 0);
    f("learn_amount", 0, 1, .35);
    f("learn_max_cut", 0, 12, 4);
    f("learn_max_boost", 0, 6, 1.5);
    f("learn_low", 40, 1000, 100, .4);
    f("learn_high", 1000, 20000, 12000, .4);
    f("learn_smoothing", .1, 2, 1);
    b("adaptive", false);
    b("learn_trigger", false);
    f("adaptation_seconds", 10, 20, 12);
    b("linear_phase", false);
    b("reference_target", false);
    choice("group_tier", {"Foreground", "Middle", "Background"}, 0);
    choice("correction_mode", {"Track only", "Group only", "Combined"}, 0);
    f("group_max_cut", 0, 6, 2);
    f("reference_start_seconds", 0, 600, 0);
    f("reference_duration_seconds", 1, 60, 15);
    choice("profile", {"Natural rap", "Deep voice", "Bright voice", "Doubles", "Beat"}, 0);
    const char *vocal[] = {"rumble",    "mud",       "nasal", "presence",
                           "harshness", "sibilance", "air",   "body"};
    for (auto k : vocal)
        f(k, 0, 1, 0);
    for (int i = 0; i < 24; ++i) {
        b(id(i, "enabled"), false);
        b(id(i, "dynamic"), false);
        b(id(i, "audition"), false);
        choice(id(i, "type"),
               {"Bell", "Low shelf", "High shelf", "High pass", "Low pass", "Notch", "Band pass"},
               0);
        choice(id(i, "route"), {"Linked", "Left", "Right", "Mid", "Side"}, 0);
        choice(id(i, "slope"), {"12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct"}, 0);
        f(id(i, "frequency"), 20, 20000, 1000, .3);
        f(id(i, "gain"), -18, 18, 0);
        f(id(i, "q"), .15, 12, .707, .4);
        f(id(i, "threshold"), -72, 0, -24);
        f(id(i, "ratio"), 1, 20, 2, .5);
        f(id(i, "attack"), .2, 200, 10, .4);
        f(id(i, "release"), 10, 1000, 120, .4);
        f(id(i, "knee"), 0, 18, 6);
        f(id(i, "range"), 0, 12, 4);
    }
    return l;
}
} // namespace
class VocalEQ final : public AudioProcessor, private Timer {
  public:
    std::mutex groupMutex;
    GroupLink group;
    std::atomic<bool> offline{false};
    UndoManager undo;
    AudioProcessorValueTreeState state{*this, &undo, "JMVEQ", layout()};
    std::map<std::string, std::atomic<float> *, std::less<>> globals;
    std::array<std::array<std::atomic<float> *, 16>, 24> bp{};
    std::array<std::atomic<double>, 513> publishedKernel{};
    std::array<double, 513> kernel{}, histL{}, histR{}, dryHistoryL{}, dryHistoryR{};
    int firPos = 0;
    std::atomic<bool> linearActive{false};
    std::atomic<unsigned> kernelVersion{0};
    std::array<jm::Band, 24> bands{};
    std::array<jm::Settings, 24> settings{};
    std::array<jm::Band, 8> vocals{};
    std::array<jm::Settings, 8> vs{};
    std::array<jm::Band, 32> correction{}, unmask{};
    std::array<jm::Settings, 32> gs{};
    std::array<jm::Settings, 32> cs{};
    std::array<jm::Filter, 2> auditionFilters{};
    std::array<std::atomic<float>, 24> dynamicActivity{};
    std::array<std::atomic<float>, 32> spectrum{}, postSpectrum{}, learned{}, reference{};
    std::atomic<float> progress{0}, peakIn{0}, peakOut{0};
    std::atomic<bool> learnRequest{false};
    VocalEQ()
        : AudioProcessor(BusesProperties()
                             .withInput("Input", AudioChannelSet::stereo(), true)
                             .withOutput("Output", AudioChannelSet::stereo(), true)) {
        for (auto &x : learned)
            x.store(0);
        for (auto &x : spectrum)
            x.store(-120);
        for (auto &x : postSpectrum)
            x.store(-120);
        publishedKernel[256].store(1);
        kernel[256] = 1;
        startTimerHz(4);
        for (auto *param : getParameters())
            if (auto *ranged = dynamic_cast<AudioProcessorParameterWithID *>(param))
                globals.emplace(ranged->paramID.toStdString(),
                                state.getRawParameterValue(ranged->paramID));
        const char *keys[] = {"enabled", "dynamic",   "audition", "type", "route",
                              "slope",   "frequency", "gain",     "q",    "threshold",
                              "ratio",   "attack",    "release",  "knee", "range"};
        for (int i = 0; i < 24; ++i)
            for (int j = 0; j < 15; ++j)
                bp[i][j] = state.getRawParameterValue(id(i, keys[j]));
    }
    const String getName() const override {
        return "J Money - Vocal Intelligence EQ";
    }
    ~VocalEQ() override {
        stopTimer();
    }
    void prepareToPlay(double rate, int) override {
        fs = rate;
        bands = {};
        vocals = {};
        correction = {};
        unmask = {};
        analysis = {};
        postAnalysis = {};
        postPower.fill(0);
        detected.fill(0);
        vd.fill(0);
        auditionFilters = {};
        power.fill(0);
        total.fill(0);
        samples = accepted = 0;
        count = 0;
        learning = false;
        outGain = wet = 1;
        inEnergy = outEnergy = 1e-12;
        histL.fill(0);
        histR.fill(0);
        dryHistoryL.fill(0);
        dryHistoryR.fill(0);
        firPos = 0;
        readSettings();
        for (int i = 0; i < 24; ++i) {
            bands[i].smoothedHz = settings[i].hz;
            bands[i].smoothedGain = settings[i].gainDb;
            bands[i].smoothedQ = settings[i].q;
        }
        setLatencySamples(linearActive.load() ? 256 : 0);
    }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &b) const override {
        return b.getMainInputChannelSet() == b.getMainOutputChannelSet() &&
               (b.getMainInputChannelSet() == AudioChannelSet::mono() ||
                b.getMainInputChannelSet() == AudioChannelSet::stereo());
    }
    bool supportsDoublePrecisionProcessing() const override {
        return true;
    }
    bool acceptsMidi() const override {
        return false;
    }
    bool producesMidi() const override {
        return false;
    }
    double getTailLengthSeconds() const override {
        double rate = std::max(44100., getSampleRate()), tail = 256. / rate;
        for (int i = 0; i < 24; ++i)
            if (bp[i][0]->load() > .5) {
                auto c = jm::Coeff::make((int)bp[i][3]->load(), rate, bp[i][6]->load(),
                                         bp[i][7]->load(), bp[i][8]->load());
                auto disc = std::sqrt(std::complex<double>(c.a1 * c.a1 - 4 * c.a2, 0));
                double r = std::max(std::abs((-c.a1 + disc) / 2.), std::abs((-c.a1 - disc) / 2.));
                if (r > 0 && r < 1)
                    tail += std::log(1e-9) / std::log(r) / rate * (1 + (int)bp[i][5]->load());
            }
        return tail + 4;
    }
    int getNumPrograms() override {
        return 1;
    }
    int getCurrentProgram() override {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override {
        return "User";
    }
    void changeProgramName(int, const String &) override {}
    AudioProcessorParameter *getBypassParameter() const override {
        return state.getParameter("bypass");
    }
    bool hasEditor() const override {
        return true;
    }
    AudioProcessorEditor *createEditor() override;
    void factory(int index) {
        auto data = JSON::parse(String::fromUTF8(factoryJson));
        auto *array = data.getArray();
        if (!array || index < 0 || index >= array->size())
            return;
        auto entry = (*array)[index];
        for (auto *param : getParameters())
            if (auto *ranged = dynamic_cast<RangedAudioParameter *>(param))
                set(ranged->paramID, ranged->convertFrom0to1(ranged->getDefaultValue()));
        if (auto *props = entry["parameters"].getDynamicObject())
            for (auto &item : props->getProperties())
                set(item.name.toString(), (float)item.value);
        auto curve = entry["target_db"];
        for (int i = 0; i < 32; ++i) {
            reference[i].store((float)curve[i]);
            learned[i].store(0);
        }
        set("reference_target", 1);
    }
    float value(std::string_view k) const {
        auto it = globals.find(k);
        return it == globals.end() ? 0 : it->second->load();
    }
    void set(const String &k, float v) {
        if (auto *p = state.getParameter(k)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(v));
            p->endChangeGesture();
        }
    }
    void service() {
        timerCallback();
    }
    bool loadReference(const File &file) {
        AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> reader(formats.createReaderFor(file));
        if (!reader)
            return false;
        int64 start = (int64)(value("reference_start_seconds") * reader->sampleRate);
        int64 end =
            std::min(reader->lengthInSamples,
                     start + (int64)(value("reference_duration_seconds") * reader->sampleRate));
        if (end <= start)
            return false;
        std::array<jm::Filter, 32> filters{};
        std::array<jm::Coeff, 32> coeff{};
        std::array<double, 32> energy{};
        for (int i = 0; i < 32; ++i)
            coeff[i] = jm::Coeff::make(jm::BandPass, reader->sampleRate, hz(i), 0, 4);
        AudioBuffer<float> buffer(2, 4096);
        int64 acceptedCount = 0;
        for (int64 pos = start; pos < end; pos += 4096) {
            int count = (int)std::min<int64>(4096, end - pos);
            if (!reader->read(&buffer, 0, count, pos, true, true))
                return false;
            for (int n = 0; n < count; ++n) {
                double x = (buffer.getSample(0, n) + buffer.getSample(1, n)) * .5;
                if (std::abs(x) > .0003)
                    ++acceptedCount;
                for (int i = 0; i < 32; ++i) {
                    double y = filters[i].tick(x, coeff[i]);
                    energy[i] += y * y;
                }
            }
        }
        if (acceptedCount < reader->sampleRate)
            return false;
        double mean = 0;
        for (auto e : energy)
            mean += jm::db(std::sqrt(e / acceptedCount)) / 32;
        for (int i = 0; i < 32; ++i)
            reference[i].store((float)(jm::db(std::sqrt(energy[i] / acceptedCount)) - mean));
        set("reference_target", 1);
        return true;
    }
    void getStateInformation(MemoryBlock &dest) override {
        std::lock_guard guard(groupMutex);
        auto tree = state.copyState();
        tree.setProperty("group", group.group, nullptr);
        tree.setProperty("instance_name", group.name, nullptr);
        for (int i = 0; i < 32; ++i) {
            tree.setProperty("curve" + String(i), learned[i].load(), nullptr);
            tree.setProperty("reference" + String(i), reference[i].load(), nullptr);
            tree.setProperty("groupcurve" + String(i), group.reduction[i].load(), nullptr);
        }
        if (auto xml = tree.createXml())
            copyXmlToBinary(*xml, dest);
    }
    void setStateInformation(const void *data, int size) override {
        if (size < 1 || size > 4 * 1024 * 1024)
            return;
        if (auto xml = getXmlFromBinary(data, size)) {
            auto tree = ValueTree::fromXml(*xml);
            if (tree.hasType("JMVEQ")) {
                for (auto child : tree) {
                    auto key = child.getProperty("id").toString();
                    if (auto *param = state.getParameter(key)) {
                        float val = (float)child.getProperty("value");
                        if (!std::isfinite(val))
                            return;
                        child.setProperty(
                            "value", param->convertFrom0to1(param->convertTo0to1(val)), nullptr);
                    }
                }
                for (int i = 0; i < 32; ++i)
                    if (!std::isfinite((float)tree.getProperty("curve" + String(i), 0.f)) ||
                        !std::isfinite((float)tree.getProperty("reference" + String(i), 0.f)))
                        return;
                std::lock_guard guard(groupMutex);
                state.replaceState(tree);
                group.group = tree.getProperty("group").toString().substring(0, 80);
                group.name = tree.getProperty("instance_name", "Vocal").toString().substring(0, 80);
                for (int i = 0; i < 32; ++i) {
                    learned[i].store(
                        jlimit(-12.f, 6.f, (float)tree.getProperty("curve" + String(i), 0.f)));
                    reference[i].store(
                        jlimit(-60.f, 60.f, (float)tree.getProperty("reference" + String(i), 0.f)));
                    float gc = (float)tree.getProperty("groupcurve" + String(i), 0.f);
                    group.reduction[i].store(std::isfinite(gc) ? jlimit(-6.f, 0.f, gc) : 0.f);
                }
            }
        }
    }
    void processBlock(AudioBuffer<float> &b, MidiBuffer &) override {
        process(b);
    }
    void processBlock(AudioBuffer<double> &b, MidiBuffer &) override {
        process(b);
    }
    void processBlockBypassed(AudioBuffer<float> &b, MidiBuffer &m) override {
        process(b, true);
    }
    void processBlockBypassed(AudioBuffer<double> &b, MidiBuffer &m) override {
        process(b, true);
    }

  private:
    void timerCallback() override {
        {
            std::lock_guard guard(groupMutex);
            group.tick(spectrum, (int)value("group_tier"), value("bypass") > .5,
                       value("group_max_cut"), offline.load());
            auto cmd = group.takeCommand();
            if (cmd.first == "learn")
                learnRequest = true;
            else if (cmd.first == "group_tier" || cmd.first == "group_max_cut" ||
                     cmd.first == "bypass")
                set(cmd.first, cmd.second);
        }
        bool active = value("linear_phase") > .5;
        if (active) {
            juce::dsp::FFT fft(10);
            std::array<juce::dsp::Complex<float>, 1024> input{}, output{};
            for (int k = 0; k <= 512; ++k) {
                double f = k * std::max(44100., getSampleRate()) / 1024., mag = 1;
                for (int i = 0; i < 24; ++i) {
                    auto v = [&](int j) { return bp[i][j]->load(); };
                    if (v(0) > .5 && v(1) < .5 && (int)v(4) == 0) {
                        auto c = jm::Coeff::make((int)v(3), std::max(44100., getSampleRate()), v(6),
                                                 v(7), v(8));
                        int stages = ((int)v(3) == jm::HighPass || (int)v(3) == jm::LowPass)
                                         ? 1 + (int)v(5)
                                         : 1;
                        mag *= std::pow(c.magnitude(std::max(44100., getSampleRate()), f), stages);
                    }
                }
                input[k] = {(float)std::clamp(mag, 0., 64.), 0};
                if (k > 0 && k < 512)
                    input[1024 - k] = input[k];
            }
            fft.perform(input.data(), output.data(), true);
            kernelVersion.fetch_add(1);
            for (int n = 0; n < 513; ++n) {
                int k = (n - 256 + 1024) % 1024;
                double window = .42 - .5 * std::cos(2 * jm::pi * n / 512) +
                                .08 * std::cos(4 * jm::pi * n / 512);
                publishedKernel[n].store(output[k].real() * window);
            }
            kernelVersion.fetch_add(1);
        }
        if (active != linearActive.load()) {
            linearActive.store(active);
            setLatencySamples(active ? 256 : 0);
        }
    }
    double fs = 44100;
    uint64_t samples = 0, accepted = 0;
    int count = 0;
    bool learning = false, previousLearn = false;
    std::array<jm::Filter, 32> analysis{}, postAnalysis{};
    std::array<double, 32> postPower{};
    std::array<jm::Coeff, 32> analysisC{};
    std::array<double, 32> power{}, total{};
    std::array<double, 24> detected{};
    std::array<double, 8> vd{};
    double inEnergy = 1e-12, outEnergy = 1e-12, outGain = 1, wet = 1;
    static double hz(int i) {
        return 60 * std::pow(16000. / 60, i / 31.);
    }
    void readSettings() {
        for (int i = 0; i < 24; ++i) {
            auto &p = settings[i];
            auto v = [&](int k) { return bp[i][k]->load(); };
            p.enabled = v(0) > .5;
            p.dynamic = v(1) > .5;
            p.type = (int)v(3);
            p.route = (int)v(4);
            p.stages = (p.type == jm::HighPass || p.type == jm::LowPass) ? 1 + (int)v(5) : 1;
            p.hz = v(6);
            p.gainDb = v(7);
            p.q = v(8);
            p.threshold = v(9);
            p.ratio = v(10);
            p.attack = v(11);
            p.release = v(12);
            p.knee = v(13);
            p.range = v(14);
        }
        const char *keys[] = {"rumble",    "mud",       "nasal", "presence",
                              "harshness", "sibilance", "air",   "body"};
        double freq[] = {65, 320, 1000, 2600, 4200, 7200, 12000, 160};
        double gains[] = {0, -5, -4, 2, -4, -5, 2, 2};
        for (int i = 0; i < 8; ++i) {
            auto &p = vs[i];
            double a = value(keys[i]);
            p.enabled = a > .001;
            p.hz = i == 0 ? 20 + 70 * a : freq[i];
            p.type = i == 0 ? jm::HighPass : i == 6 ? jm::HighShelf : jm::Bell;
            p.q = i == 5 ? 2 : .8;
            p.gainDb = gains[i] * a;
            p.dynamic = i == 4 || i == 5;
            p.range = 5 * a;
            p.gainDb = p.dynamic ? 0 : p.gainDb;
            p.threshold = -30;
            p.attack = i == 5 ? 2 : 8;
            p.release = 100;
        }
        for (int i = 0; i < 32; ++i) {
            analysisC[i] = jm::Coeff::make(jm::BandPass, fs, hz(i), 0, 4);
            auto &p = cs[i];
            p.enabled = hz(i) >= value("learn_low") && hz(i) <= value("learn_high");
            p.hz = hz(i);
            p.q = 4;
            p.gainDb = (value("correction_mode") == 1
                            ? 0
                            : jlimit(-value("learn_max_cut"), value("learn_max_boost"),
                                     learned[i].load()) *
                                  value("learn_amount"));
            gs[i] = p;
            gs[i].gainDb = value("correction_mode") == 0 ? 0 : group.reduction[i].load();
            p.enabled = p.enabled &&
                        (std::abs(p.gainDb) > 1e-5 || std::abs(correction[i].smoothedGain) > 1e-5);
            gs[i].enabled = gs[i].enabled && (std::abs(gs[i].gainDb) > 1e-5 ||
                                              std::abs(unmask[i].smoothedGain) > 1e-5);
        }
    }
    void finishLearn() {
        std::array<double, 32> raw{};
        double mean = 0;
        for (int i = 0; i < 32; ++i) {
            raw[i] = jm::db(std::sqrt(total[i] / std::max<uint64_t>(1, accepted)));
            mean += raw[i];
        }
        mean /= 32;
        if (*std::max_element(raw.begin(), raw.end()) - *std::min_element(raw.begin(), raw.end()) <
            6) {
            progress.store(.99f);
            return;
        }
        int profile = (int)value("profile");
        for (int i = 0; i < 32; ++i) {
            double target = -3 * std::log2(hz(i) / 500.);
            if (profile == 1 && hz(i) < 300)
                target += 2;
            if (profile == 2 && hz(i) > 3000)
                target += 1;
            if (profile == 3 && hz(i) > 1500)
                target -= 2;
            double sum = 0, weight = 0;
            for (int j = 0; j < 32; ++j) {
                double w = std::exp(-.5 * std::pow((i - j) / (2 * value("learn_smoothing")), 2));
                double tj = value("reference_target") > .5 ? reference[j].load()
                                                           : -3 * std::log2(hz(j) / 500.);
                if (value("reference_target") < .5) {
                    if (profile == 1 && hz(j) < 300)
                        tj += 2;
                    if (profile == 2 && hz(j) > 3000)
                        tj += 1;
                    if (profile == 3 && hz(j) > 1500)
                        tj -= 2;
                }
                sum += w * (mean + tj - raw[j]);
                weight += w;
            }
            double c = sum / weight;
            if (raw[i] < mean - 24)
                c = std::min(0., c);
            learned[i].store((float)std::clamp(c, -(double)value("learn_max_cut"),
                                               (double)value("learn_max_boost")));
        }
        if (!value("adaptive"))
            learning = false;
        else {
            total.fill(0);
            accepted = 0;
        }
    }
    template <class T> void process(AudioBuffer<T> &b, bool forceBypass = false) {
        ScopedNoDenormals noDenormals;
        if (b.getNumChannels() < 1)
            return;
        offline.store(isNonRealtime());
        readSettings();
        bool learnFlag = value("learn_trigger") > .5;
        if (learnFlag != previousLearn) {
            learnRequest = true;
            previousLearn = learnFlag;
        }
        const bool linear = linearActive.load();
        if (linear) {
            unsigned v = kernelVersion.load();
            std::array<double, 513> next{};
            if (!(v & 1)) {
                for (int i = 0; i < 513; ++i)
                    next[i] = publishedKernel[i].load();
                if (v == kernelVersion.load())
                    for (int i = 0; i < 513; ++i)
                        kernel[i] += .1 * (next[i] - kernel[i]);
            }
        }
        if (learnRequest.exchange(false)) {
            learning = true;
            total.fill(0);
            accepted = 0;
            progress = 0;
        }
        const bool autoGain = value("auto_gain") > .5,
                   bypassed = forceBypass || value("bypass") > .5;
        const double trim = value("output_db");
        double inputPeak = 0, outputPeak = 0;
        int audition = -1;
        for (int i = 0; i < 24; ++i)
            if (bp[i][2]->load() > .5)
                audition = i;
        for (int n = 0; n < b.getNumSamples(); ++n) {
            double l = (double)b.getSample(0, n),
                   r = b.getNumChannels() > 1 ? (double)b.getSample(1, n) : l;
            if (!std::isfinite(l) || std::abs(l) > (double)std::numeric_limits<T>::max())
                l = 0;
            if (!std::isfinite(r) || std::abs(r) > (double)std::numeric_limits<T>::max())
                r = 0;
            double dryL = l, dryR = r;
            inputPeak = std::max(inputPeak, std::max(std::abs(l), std::abs(r)));
            double mono = (l + r) * .5;
            for (int i = 0; i < 32; ++i) {
                double v = analysis[i].tick(mono, analysisC[i]);
                power[i] += .001 * (v * v - power[i]);
                if (learning && inEnergy > 1e-7)
                    total[i] += v * v;
            }
            if (learning && inEnergy > 1e-7)
                ++accepted;
            for (int i = 0; i < 24; ++i)
                if (settings[i].enabled)
                    detected[i] = std::max(
                        detected[i], std::max(std::abs(bands[i].detector[0].tick(l, bands[i].dc)),
                                              std::abs(bands[i].detector[1].tick(r, bands[i].dc))));
            for (int i = 0; i < 8; ++i)
                if (vs[i].enabled)
                    vd[i] =
                        std::max(vd[i], std::abs(vocals[i].detector[0].tick(mono, vocals[i].dc)));
            if ((count++ % 32) == 0) {
                for (int i = 0; i < 24; ++i) {
                    if (settings[i].enabled || bp[i][2]->load() > .5)
                        bands[i].update(settings[i], fs, detected[i]);
                    dynamicActivity[i].store((float)bands[i].reduction);
                    detected[i] = 0;
                }
                for (int i = 0; i < 8; ++i) {
                    if (vs[i].enabled)
                        vocals[i].update(vs[i], fs, vd[i]);
                    vd[i] = 0;
                }
                for (int i = 0; i < 32; ++i) {
                    if (cs[i].enabled)
                        correction[i].update(cs[i], fs, 0);
                    if (gs[i].enabled)
                        unmask[i].update(gs[i], fs, 0);
                    spectrum[i].store((float)jm::db(std::sqrt(power[i])));
                }
            }

            if (linear) {
                histL[firPos] = l;
                histR[firPos] = r;
                l = r = 0;
                int k = firPos;
                for (int j = 0; j < 513; ++j) {
                    l += kernel[j] * histL[k];
                    r += kernel[j] * histR[k];
                    if (--k < 0)
                        k = 512;
                }
            }
            for (int i = 0; i < 24; ++i) {
                if (linear && !settings[i].dynamic && settings[i].route == 0)
                    continue;
                auto y = bands[i].tick(l, r, settings[i]);
                l = y[0];
                r = y[1];
            }
            for (int i = 0; i < 8; ++i) {
                auto y = vocals[i].tick(l, r, vs[i]);
                l = y[0];
                r = y[1];
            }
            for (int i = 0; i < 32; ++i) {
                auto y = correction[i].tick(l, r, cs[i]);
                l = y[0];
                r = y[1];
                auto z = unmask[i].tick(l, r, gs[i]);
                l = z[0];
                r = z[1];
            }
            if (audition >= 0) {
                l = auditionFilters[0].tick(dryL, bands[audition].dc);
                r = auditionFilters[1].tick(dryR, bands[audition].dc);
            }
            dryHistoryL[firPos] = dryL;
            dryHistoryR[firPos] = dryR;
            if (linear) {
                int k = (firPos + 513 - 256) % 513;
                dryL = dryHistoryL[k];
                dryR = dryHistoryR[k];
            }
            firPos = (firPos + 1) % 513;
            double smooth = 1 - std::exp(-1 / (fs * .4));
            inEnergy += smooth * ((dryL * dryL + dryR * dryR) * .5 - inEnergy);
            outEnergy += smooth * ((l * l + r * r) * .5 - outEnergy);
            double compensation =
                autoGain && inEnergy > 1e-8
                    ? std::clamp(10 * std::log10(inEnergy / std::max(1e-12, outEnergy)), -6., 6.)
                    : 0;
            outGain += (1 - std::exp(-1 / (fs * .03))) * (jm::gain(trim + compensation) - outGain);
            wet += (1 - std::exp(-1 / (fs * .01))) * ((bypassed ? 0. : 1.) - wet);
            l = dryL + wet * (l * outGain - dryL);
            r = dryR + wet * (r * outGain - dryR);
            if (!std::isfinite(l) || std::abs(l) > (double)std::numeric_limits<T>::max())
                l = 0;
            if (!std::isfinite(r) || std::abs(r) > (double)std::numeric_limits<T>::max())
                r = 0;
            for (int i = 0; i < 32; ++i) {
                double y = postAnalysis[i].tick((l + r) * .5, analysisC[i]);
                postPower[i] += .001 * (y * y - postPower[i]);
                if ((count % 32) == 0)
                    postSpectrum[i].store((float)jm::db(std::sqrt(postPower[i])));
            }
            b.setSample(0, n, (T)l);
            if (b.getNumChannels() > 1)
                b.setSample(1, n, (T)r);
            outputPeak = std::max(outputPeak, std::max(std::abs(l), std::abs(r)));
            if (learning && ++samples % 4096 == 0) {
                progress.store((float)std::min(1., accepted / (fs * value("adaptation_seconds"))));
                if (accepted >= fs * value("adaptation_seconds"))
                    finishLearn();
            }
        }
        peakIn.store((float)inputPeak);
        peakOut.store((float)outputPeak);
    }
};
class Editor final : public AudioProcessorEditor, private Timer {
    VocalEQ &p;
    TooltipWindow tooltips{this, 600};
    std::unique_ptr<GenericAudioProcessorEditor> controls;
    TextButton advanced{"Advanced parameters"};
    Label quickGuide;
    bool advancedVisible = false;
    ComboBox profile;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> profileAttachment;
    std::array<Slider, 8> macros;
    std::array<Label, 8> macroLabels;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 8> macroAttachments;
    MemoryBlock aState, bState;
    bool slotB = false;
    int dragging = -1;
    bool groupView = false;
    String draggedPeer;
    Slider sharedCut;
    Label sharedLabel;
    TextButton groupToggle{"Group view"}, learnAll{"Learn All"};
    TextButton ab{"A / B"}, reset{"Reset"};
    TextButton learn{"Learn 12 seconds"}, undo{"Undo"}, redo{"Redo"}, save{"Save preset"},
        load{"Load preset"}, referenceButton{"Load reference"};
    Label info;
    TextEditor groupName, trackName;
    ComboBox presets;
    std::unique_ptr<FileChooser> chooser;

  public:
    Editor(VocalEQ &x) : AudioProcessorEditor(x), p(x) {
        setOpaque(true);
        setResizable(true, true);
        setResizeLimits(920, 920, 1400, 1400);
        getConstrainer()->setFixedAspectRatio(1.0);
        setSize(920, 920);
        profile.addItemList({"Natural rap", "Deep voice", "Bright voice", "Doubles", "Beat"}, 1);
        addAndMakeVisible(profile);
        profile.setTooltip("Original source target; disable reference_target to use this profile");
        profileAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
            p.state, "profile", profile);
        const char *macroKeys[] = {"rumble",    "mud",       "nasal", "presence",
                                   "harshness", "sibilance", "air",   "body"};
        for (int i = 0; i < 8; ++i) {
            macros[i].setSliderStyle(Slider::RotaryVerticalDrag);
            macros[i].setTextBoxStyle(Slider::TextBoxBelow, false, 65, 18);
            macros[i].setTooltip(String(macroKeys[i]) + " EQ amount, 0 to 1");
            macroLabels[i].setText(macroKeys[i], dontSendNotification);
            macroLabels[i].setJustificationType(Justification::centred);
            addAndMakeVisible(macros[i]);
            addAndMakeVisible(macroLabels[i]);
            macroAttachments[i] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
                p.state, macroKeys[i], macros[i]);
        }
        auto data = JSON::parse(String::fromUTF8(factoryJson));
        if (auto *array = data.getArray())
            for (int i = 0; i < array->size(); ++i)
                presets.addItem((*array)[i]["name"].toString(), i + 1);
        presets.setTextWhenNothingSelected("Factory presets");
        presets.onChange = [this] { p.factory(presets.getSelectedId() - 1); };
        {
            std::lock_guard guard(p.groupMutex);
            groupName.setText(p.group.group);
            trackName.setText(p.group.name);
        }
        groupName.setTextToShowWhenEmpty("Group name (blank = off)", Colours::grey);
        groupName.onTextChange = [this] {
            std::lock_guard guard(p.groupMutex);
            p.group.group = groupName.getText().substring(0, 80);
        };
        trackName.onTextChange = [this] {
            std::lock_guard guard(p.groupMutex);
            p.group.name = trackName.getText().substring(0, 80);
        };
        for (auto *c : {(Component *)&advanced, (Component *)&quickGuide, (Component *)&learn,
                        (Component *)&undo, (Component *)&redo, (Component *)&save,
                        (Component *)&load, (Component *)&info, (Component *)&referenceButton,
                        (Component *)&groupName, (Component *)&trackName, (Component *)&presets,
                        (Component *)&ab, (Component *)&reset, (Component *)&groupToggle,
                        (Component *)&learnAll, (Component *)&sharedCut, (Component *)&sharedLabel})
            addAndMakeVisible(c);
        quickGuide.setText(
            "Start with a factory preset and the vocal controls above.\n"
            "Use Advanced parameters for manual bands, learning limits, and dynamics.\n"
            "Group view shows tracks that have a named J Money instance.",
            dontSendNotification);
        quickGuide.setJustificationType(Justification::centred);
        advanced.onClick = [this] {
            if (!controls) {
                controls = std::make_unique<GenericAudioProcessorEditor>(p);
                addChildComponent(*controls);
            }
            advancedVisible = !advancedVisible;
            groupView = false;
            controls->setVisible(advancedVisible);
            quickGuide.setVisible(!advancedVisible);
            for (int i = 0; i < 8; ++i) {
                macros[i].setVisible(true);
                macroLabels[i].setVisible(true);
            }
            resized();
            repaint();
        };
        sharedCut.setRange(0, 6, .1);
        sharedCut.setValue(2);
        sharedCut.setTextBoxStyle(Slider::TextBoxRight, false, 55, 24);
        sharedCut.setTooltip("Send maximum group attenuation to every current member (dB)");
        sharedLabel.setText("Shared group cut dB", dontSendNotification);
        sharedCut.onValueChange = [this] {
            std::lock_guard guard(p.groupMutex);
            for (auto &peer : p.group.peers)
                p.group.command(peer.id, "group_max_cut", (float)sharedCut.getValue());
            p.set("group_max_cut", (float)sharedCut.getValue());
        };
        groupToggle.onClick = [this] {
            groupView = !groupView;
            if (controls)
                controls->setVisible(!groupView && advancedVisible);
            quickGuide.setVisible(!groupView && !advancedVisible);
            for (int i = 0; i < 8; ++i) {
                macros[i].setVisible(!groupView);
                macroLabels[i].setVisible(!groupView);
            }
            repaint();
        };
        learnAll.onClick = [this] {
            std::lock_guard guard(p.groupMutex);
            for (auto &peer : p.group.peers)
                p.group.command(peer.id, "learn", 1);
            p.learnRequest = true;
        };
        ab.onClick = [this] {
            MemoryBlock current;
            p.getStateInformation(current);
            auto &dest = slotB ? aState : bState;
            if (dest.getSize() == 0)
                dest = current;
            if (slotB)
                bState = current;
            else
                aState = current;
            p.setStateInformation(dest.getData(), (int)dest.getSize());
            slotB = !slotB;
            ab.setButtonText(slotB ? "Slot B" : "Slot A");
        };
        reset.onClick = [this] {
            for (auto *param : p.getParameters())
                if (auto *r = dynamic_cast<RangedAudioParameter *>(param))
                    p.set(r->paramID, r->convertFrom0to1(r->getDefaultValue()));
            for (auto &c : p.learned)
                c.store(0);
        };
        learn.onClick = [this] { p.set("learn_trigger", p.value("learn_trigger") > .5 ? 0 : 1); };
        undo.onClick = [this] { p.undo.undo(); };
        redo.onClick = [this] { p.undo.redo(); };
        save.onClick = [this] { file(true); };
        load.onClick = [this] { file(false); };
        referenceButton.setTooltip("Analyze an isolated vocal reference. Matching a vocal to a "
                                   "complete mastered song can give misleading results. Set "
                                   "start/duration in the advanced panel, then Learn your source.");
        learn.setTooltip("Analyze 10–20 seconds of representative nonsilent source audio; DSP "
                         "spectral matching, not trained AI.");
        groupToggle.setTooltip(
            "Group cards: drag to change tier; double-click to toggle a peer bypass.");
        referenceButton.onClick = [this] {
            chooser = std::make_unique<FileChooser>(
                "Reference: isolated vocal recommended; full mixes can mislead", File{},
                "*.wav;*.aiff;*.flac");
            chooser->launchAsync(
                FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                [this](const FileChooser &c) {
                    if (c.getResult() != File() && !p.loadReference(c.getResult()))
                        info.setText("Reference rejected: unreadable, silent, or too short",
                                     dontSendNotification);
                });
        };
        startTimerHz(15);
    }
    void selectSoftwareRenderer() {
#if JUCE_WINDOWS
        if (auto *peer = getPeer()) {
            auto engines = peer->getAvailableRenderingEngines();
            for (int i = 0; i < engines.size(); ++i)
                if (engines[i].containsIgnoreCase("software")) {
                    if (peer->getCurrentRenderingEngine() != i)
                        peer->setCurrentRenderingEngine(i);
                    break;
                }
        }
#endif
    }
    void parentHierarchyChanged() override {
        selectSoftwareRenderer();
    }
    void visibilityChanged() override {
        selectSoftwareRenderer();
    }
    ComponentPeer *createNewPeer(int styleFlags, void *parentWindow) override {
        auto *peer = Component::createNewPeer(styleFlags, parentWindow);
#if JUCE_WINDOWS
        const auto engines = peer->getAvailableRenderingEngines();
        for (int i = 0; i < engines.size(); ++i)
            if (engines[i].containsIgnoreCase("software")) {
                peer->setCurrentRenderingEngine(i);
                break;
            }
#endif
        return peer;
    }
    void file(bool writing) {
        chooser = std::make_unique<FileChooser>(
            writing ? "Save J Money preset" : "Load J Money preset",
            File::getSpecialLocation(File::userDocumentsDirectory), "*.jmveq");
        chooser->launchAsync(
            writing ? FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                    : FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
            [this, writing](const FileChooser &c) {
                auto f = c.getResult();
                if (f == File())
                    return;
                if (writing) {
                    MemoryBlock b;
                    p.getStateInformation(b);
                    f.withFileExtension("jmveq").replaceWithData(b.getData(), b.getSize());
                } else {
                    MemoryBlock b;
                    if (f.loadFileAsData(b))
                        p.setStateInformation(b.getData(), (int)b.getSize());
                }
            });
    }
    void resized() override {
        profile.setBounds(getWidth() - 245, 8, 220, 28);
        for (int i = 0; i < 8; ++i) {
            int w = (getWidth() - 30) / 8;
            macroLabels[i].setBounds(15 + i * w, 365, w, 20);
            macros[i].setBounds(15 + i * w, 385, w, 65);
        }
        learn.setBounds(20, 45, 170, 30);
        undo.setBounds(200, 45, 65, 30);
        redo.setBounds(270, 45, 65, 30);
        save.setBounds(345, 45, 110, 30);
        load.setBounds(465, 45, 110, 30);
        referenceButton.setBounds(585, 45, 140, 30);
        ab.setBounds(735, 45, 70, 30);
        reset.setBounds(815, 45, 65, 30);
        groupToggle.setBounds(20, 330, 110, 26);
        learnAll.setBounds(140, 330, 110, 26);
        sharedLabel.setBounds(270, 330, 160, 26);
        sharedCut.setBounds(435, 330, 220, 26);
        groupName.setBounds(20, 300, 220, 25);
        trackName.setBounds(250, 300, 180, 25);
        presets.setBounds(445, 300, getWidth() - 465, 25);
        info.setBounds(20, 278, getWidth() - 40, 22);
        if (controls)
            controls->setBounds(15, 460, getWidth() - 30, getHeight() - 475);
        quickGuide.setBounds(35, 480, getWidth() - 70, getHeight() - 510);
        advanced.setBounds(670, 330, getWidth() - 690, 26);
    }
    void paint(Graphics &g) override {
        g.fillAll(Colour(0xff101722));
        g.setColour(Colours::white);
        g.setFont(23);
        g.drawText("J MONEY / VOCAL INTELLIGENCE EQ", 20, 8, getWidth() - 280, 30,
                   Justification::centredLeft);
        Rectangle<float> graph(25, 95, (float)getWidth() - 50, 175);
        g.setColour(Colour(0xff1d2a38));
        g.fillRoundedRectangle(graph, 6);
        for (int k = 0; k < 32; ++k) {
            float x = graph.getX() + graph.getWidth() *
                                         std::log((60 * std::pow(16000. / 60, k / 31.)) / 20) /
                                         std::log(1000.);
            float y = graph.getBottom() -
                      jlimit(0.f, 1.f, (p.spectrum[k].load() + 90) / 90) * graph.getHeight();
            g.setColour(Colours::turquoise);
            g.drawLine(x, graph.getBottom(), x, y, 2);
            g.setColour(Colours::violet);
            float py = graph.getBottom() -
                       jlimit(0.f, 1.f, (p.postSpectrum[k].load() + 90) / 90) * graph.getHeight();
            g.fillEllipse(x - 1, py - 1, 3, 3);
            float cy = graph.getCentreY() - p.learned[k].load() * 7;
            g.setColour(Colours::orange);
            g.fillEllipse(x - 2, cy - 2, 4, 4);
        }
        Path response;
        for (int n = 0; n < 240; ++n) {
            double f = 20 * std::pow(1000., n / 239.);
            double mag = 1;
            for (int i = 0; i < 24; ++i)
                if (p.value(id(i, "enabled").toStdString()) > .5) {
                    auto c = jm::Coeff::make((int)p.value(id(i, "type").toStdString()),
                                             std::max(44100., p.getSampleRate()),
                                             p.value(id(i, "frequency").toStdString()),
                                             p.value(id(i, "gain").toStdString()),
                                             p.value(id(i, "q").toStdString()));
                    mag *= c.magnitude(std::max(44100., p.getSampleRate()), f);
                }
            float x = graph.getX() + graph.getWidth() * n / 239.f,
                  y = graph.getCentreY() - (float)jm::db(mag) * graph.getHeight() / 48;
            if (n == 0)
                response.startNewSubPath(x, y);
            else
                response.lineTo(x, y);
        }
        g.setColour(Colours::white.withAlpha(.6f));
        g.strokePath(response, PathStrokeType(1.5));
        for (int i = 0; i < 24; ++i)
            if (p.value(id(i, "enabled").toStdString()) > .5) {
                float f = p.value(id(i, "frequency").toStdString()),
                      v = p.value(id(i, "gain").toStdString());
                float x = graph.getX() + graph.getWidth() * std::log(f / 20) / std::log(1000.f),
                      y = graph.getCentreY() - v * graph.getHeight() / 48;
                g.setColour(Colour::fromHSV(i / 24.f, .65f, .95f, 1));
                g.fillEllipse(x - 6, y - 6, 12, 12);
                float reduction = p.dynamicActivity[i].load();
                if (reduction > .05f) {
                    g.setColour(Colours::orange);
                    g.drawLine(x, y, x, y + reduction * graph.getHeight() / 48, 3);
                }
                g.setFont(11);
                g.drawText(String(i + 1), (int)x - 8, (int)y - 22, 20, 15, Justification::centred);
            }
        if (groupView) {
            int rows[3] = {0, 0, 0};
            const char *titles[] = {"FOREGROUND", "MIDDLE", "BACKGROUND"};
            for (int t = 0; t < 3; ++t) {
                g.setColour(Colours::white);
                g.setFont(16);
                g.drawText(titles[t], 20 + t * (getWidth() - 40) / 3, 370,
                           (getWidth() - 40) / 3 - 10, 25, Justification::centredLeft);
            }
            for (auto &peer : p.group.peers) {
                int tier = jlimit(0, 2, peer.tier), row = rows[tier]++;
                float x = 20.f + tier * (getWidth() - 40) / 3.f, y = 400.f + row * 80,
                      w = (getWidth() - 40) / 3.f - 12;
                g.setColour(Colour(0xff243345));
                g.fillRoundedRectangle(x, y, w, 70, 5);
                g.setColour(peer.bypass ? Colours::grey : Colours::turquoise);
                g.setFont(14);
                g.drawText(peer.name + (peer.bypass ? " [bypassed]" : ""), (int)x + 8, (int)y + 4,
                           (int)w - 12, 20, Justification::centredLeft);
                Path line;
                for (int k = 0; k < 32; ++k) {
                    float px = x + 8 + (w - 16) * k / 31,
                          py = y + 65 - jlimit(0.f, 1.f, (peer.spectrum[k] + 80) / 80) * 35;
                    if (k == 0)
                        line.startNewSubPath(px, py);
                    else
                        line.lineTo(px, py);
                }
                g.strokePath(line, PathStrokeType(1));
            }
        }
        g.setFont(12);
        g.setColour(Colours::white);
        g.drawText("20 Hz     Pre: cyan / Post: violet / Learned: orange                  20 kHz",
                   30, 270, getWidth() - 60, 16, Justification::centredLeft);
    }
    void mouseDoubleClick(const MouseEvent &e) override {
        if (!groupView || e.y < 400)
            return;
        for (int i = 0; i < (int)p.group.peers.size(); ++i) {
            auto &peer = p.group.peers[(size_t)i];
            int row = 0;
            for (int j = 0; j < i; ++j)
                if (p.group.peers[(size_t)j].tier == peer.tier)
                    ++row;
            int col = peer.tier;
            if (e.y >= 400 + row * 80 && e.y < 470 + row * 80 &&
                e.x >= 20 + col * (getWidth() - 40) / 3 &&
                e.x < 20 + (col + 1) * (getWidth() - 40) / 3) {
                p.group.command(peer.id, "bypass", peer.bypass ? 0 : 1);
                break;
            }
        }
    }
    void mouseDown(const MouseEvent &e) override {
        if (groupView && e.y >= 400) {
            for (int i = 0; i < (int)p.group.peers.size(); ++i) {
                auto &peer = p.group.peers[(size_t)i];
                int col = peer.tier, row = 0;
                for (int j = 0; j < i; ++j)
                    if (p.group.peers[(size_t)j].tier == peer.tier)
                        ++row;
                if (e.y >= 400 + row * 80 && e.y < 470 + row * 80 &&
                    e.x >= 20 + col * (getWidth() - 40) / 3 &&
                    e.x < 20 + (col + 1) * (getWidth() - 40) / 3) {
                    draggedPeer = peer.id;
                    return;
                }
            }
        }
        if (e.y < 95 || e.y > 270)
            return;
        float best = 18;
        for (int i = 0; i < 24; ++i)
            if (p.value(id(i, "enabled").toStdString()) > .5) {
                float x = 25 + (getWidth() - 50) *
                                   std::log(p.value(id(i, "frequency").toStdString()) / 20) /
                                   std::log(1000.f),
                      y = 182.5f - p.value(id(i, "gain").toStdString()) * 175 / 48;
                float d = std::hypot(e.x - x, e.y - y);
                if (d < best) {
                    best = d;
                    dragging = i;
                }
            }
        if (dragging >= 0) {
            p.state.getParameter(id(dragging, "frequency"))->beginChangeGesture();
            p.state.getParameter(id(dragging, "gain"))->beginChangeGesture();
        }
    }
    void mouseDrag(const MouseEvent &e) override {
        if (dragging < 0)
            return;
        auto *f = p.state.getParameter(id(dragging, "frequency"));
        auto *g = p.state.getParameter(id(dragging, "gain"));
        float freq = 20 * std::pow(1000.f, jlimit(0.f, 1.f, (e.x - 25.f) / (getWidth() - 50.f)));
        float gain = jlimit(-18.f, 18.f, (182.5f - e.y) * 48 / 175);
        f->setValueNotifyingHost(f->convertTo0to1(freq));
        g->setValueNotifyingHost(g->convertTo0to1(gain));
    }
    void mouseUp(const MouseEvent &e) override {
        if (draggedPeer.isNotEmpty()) {
            int tier = jlimit(0, 2, (e.x - 20) * 3 / std::max(1, getWidth() - 40));
            p.group.command(draggedPeer, "group_tier", (float)tier);
            draggedPeer.clear();
        }
        if (dragging >= 0) {
            p.state.getParameter(id(dragging, "frequency"))->endChangeGesture();
            p.state.getParameter(id(dragging, "gain"))->endChangeGesture();
            dragging = -1;
        }
    }
    void timerCallback() override {
        info.setText("Learn: " + String(p.progress.load() * 100, 0) +
                         "% accepted audio (needs accepted audio above -70 dBFS).  In " +
                         String(jm::db(p.peakIn.load()), 1) + " / Out " +
                         String(jm::db(p.peakOut.load()), 1) + " dBFS | " + p.group.overview +
                         (p.peakOut.load() > 1 ? "  OVERLOAD" : ""),
                     dontSendNotification);
        repaint();
    }
};
AudioProcessorEditor *VocalEQ::createEditor() {
    return new Editor(*this);
}
AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new VocalEQ;
}
// Integration entry points operate outside the real-time callback.
void jmFactory(AudioProcessor &p, int n) {
    dynamic_cast<VocalEQ &>(p).factory(n);
}
bool jmReference(AudioProcessor &p, const File &f) {
    return dynamic_cast<VocalEQ &>(p).loadReference(f);
}
void jmGroup(AudioProcessor &p, const String &group, const String &name) {
    auto &v = dynamic_cast<VocalEQ &>(p);
    std::lock_guard guard(v.groupMutex);
    v.group.group = group;
    v.group.name = name;
}
float jmGroupCut(AudioProcessor &p, int bin) {
    return dynamic_cast<VocalEQ &>(p).group.reduction[(size_t)bin].load();
}

void jmService(AudioProcessor &p) {
    dynamic_cast<VocalEQ &>(p).service();
}
