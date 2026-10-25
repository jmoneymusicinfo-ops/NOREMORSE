#include "TestAccess.h"
#include <chrono>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <juce_audio_utils/juce_audio_utils.h>
#include <limits>
#include <new>
thread_local bool audioScope = false;
thread_local size_t audioAllocations = 0;
void *operator new(std::size_t n) {
    if (audioScope)
        ++audioAllocations;
    if (auto *p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n) {
    return ::operator new(n);
}
void operator delete(void *p) noexcept {
    std::free(p);
}
void operator delete[](void *p) noexcept {
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept {
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept {
    std::free(p);
}
extern juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter();
void require(bool x, const char *s) {
    if (!x) {
        std::cerr << s << '\n';
        std::exit(1);
    }
}
void set(juce::AudioProcessor &p, const juce::String &id, float value) {
    for (auto *x : p.getParameters())
        if (auto *r = dynamic_cast<juce::RangedAudioParameter *>(x); r && r->paramID == id) {
            r->setValueNotifyingHost(r->convertTo0to1(value));
            return;
        }
    require(false, "missing parameter");
}
int main(int argc, char **argv) {
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
    juce::MidiBuffer midi;
    if (argc == 2 && juce::String(argv[1]) == "--window-probe") {
        juce::Component window;
        window.setOpaque(true);
        window.setSize(200, 200);
        std::cerr << "Creating empty JUCE window\n" << std::flush;
        window.addToDesktop(juce::ComponentPeer::windowHasTitleBar);
        std::cerr << "Empty JUCE window created\n" << std::flush;
        window.setVisible(true);
        window.removeFromDesktop();
        return 0;
    }
    if (argc >= 2 && juce::String(argv[1]) == "--editor-smoke") {
        for (int cycle = 0; cycle < 5; ++cycle) {
            std::cerr << "Creating editor " << cycle + 1 << "\n" << std::flush;
            std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditorIfNeeded());
            require(editor != nullptr, "editor creation");
            std::cerr << "Editor constructed\n" << std::flush;
            require(editor->getWidth() == editor->getHeight(), "square initial editor");
            require(editor->getConstrainer()->getFixedAspectRatio() == 1.0,
                    "square aspect constraint");
            juce::Component hostWindow;
            hostWindow.setOpaque(true);
            if (cycle % 2 == 0) {
                hostWindow.setBounds(editor->getBounds());
                hostWindow.addAndMakeVisible(*editor);
                hostWindow.addToDesktop(juce::ComponentPeer::windowHasTitleBar);
                hostWindow.setVisible(true);
            } else
                editor->addToDesktop(juce::ComponentPeer::windowHasTitleBar);
            std::cerr << "Native peer created\n" << std::flush;
            require(editor->getPeer() != nullptr, "native editor peer");
#if JUCE_WINDOWS
            auto *peer = editor->getPeer();
            auto engine = peer->getAvailableRenderingEngines()[peer->getCurrentRenderingEngine()];
            require(engine.containsIgnoreCase("software"), "Windows software renderer");
#endif
            editor->setVisible(true);
            std::cerr << "Editor visible\n" << std::flush;
            if (cycle == 0) {
                auto start = std::chrono::steady_clock::now();
                for (int n = 0; n < editor->getNumChildComponents(); ++n)
                    if (auto *b = dynamic_cast<juce::Button *>(editor->getChildComponent(n));
                        b && b->getButtonText() == "Advanced parameters") {
                        require((bool)b->onClick, "advanced action");
                        b->onClick();
                        break;
                    }
                std::cerr << "Advanced panel opened in "
                          << std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
                                 .count()
                          << " seconds\n"
                          << std::flush;
            }
            juce::Image image(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true,
                              juce::SoftwareImageType{});
            juce::Graphics graphics(image);
            editor->paintEntireComponent(graphics, false);
            require(image.isValid(), "editor snapshot");
            if (cycle == 0 && argc == 3) {
                auto stream = juce::File(juce::String(argv[2])).createOutputStream();
                require(stream != nullptr, "snapshot file");
                require(juce::PNGImageFormat{}.writeImageToStream(image, *stream), "snapshot PNG");
            }
            editor->setBoundsConstrained({0, 0, 1100, 950});
            require(editor->getWidth() == editor->getHeight(), "square constrained resize");
            editor->setVisible(false);
            hostWindow.removeChildComponent(editor.get());
            editor->removeFromDesktop();
            hostWindow.removeFromDesktop();
            p->editorBeingDeleted(editor.get());
            editor.reset();
            std::cerr << "Editor painted, resized and closed\n" << std::flush;
        }
        return 0;
    }

    if (argc == 3 && juce::String(argv[1]) == "--peer") {
        jmGroup(*p, juce::String::fromUTF8(argv[2]), "separate-process lead");
        p->setRateAndBufferSizeDetails(48000, 256);
        p->prepareToPlay(48000, 256);
        juce::AudioBuffer<float> block(2, 256);
        for (int n = 0; n < 100; ++n) {
            for (int i = 0; i < 256; ++i) {
                float x = .2f * std::sin(2 * 3.141592653589793 * 1000 * (n * 256 + i) / 48000);
                block.setSample(0, i, x);
                block.setSample(1, i, x);
            }
            p->processBlock(block, midi);
        }
        jmService(*p);
        std::cout << "READY" << std::flush;
        for (int n = 0; n < 100; ++n) {
            juce::Thread::sleep(100);
            jmService(*p);
        }
        return 0;
    }

    for (int channels : {1, 2})
        for (double fs : {44100., 48000., 88200., 96000., 176400., 192000.}) {
            auto layout = p->getBusesLayout();
            layout.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono()
                                                   : juce::AudioChannelSet::stereo());
            layout.outputBuses = layout.inputBuses;
            require(p->setBusesLayout(layout), "layout rejected");
            p->setRateAndBufferSizeDetails(fs, 1024);
            p->prepareToPlay(fs, 1024);
            set(*p, "auto_gain", 0);
            set(*p, "learn_amount", 0);
            for (int size : {1, 17, 64, 127, 512, 1024}) {
                juce::AudioBuffer<float> b(channels, size);
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < size; ++i)
                        b.setSample(c, i, .1f);
                audioScope = true;
                p->processBlock(b, midi);
                audioScope = false;
                require(audioAllocations == 0, "callback C++ allocation");
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < size; ++i)
                        require(std::abs(b.getSample(c, i) - .1f) < 1e-6, "zero EQ identity");
            }
            p->releaseResources();
            p->setProcessingPrecision(juce::AudioProcessor::doublePrecision);
            p->prepareToPlay(fs, 512);
            juce::AudioBuffer<double> d(channels, 512);
            d.clear();
            p->processBlock(d, midi);
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < 512; ++i)
                    require(d.getSample(c, i) == 0, "double silence");
            p->releaseResources();
            p->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
        }
    set(*p, "band1_enabled", 1);
    set(*p, "band1_frequency", 320);
    set(*p, "band1_gain", -4);
    juce::MemoryBlock a, b;
    p->getStateInformation(a);
    std::unique_ptr<juce::AudioProcessor> q(createPluginFilter());
    q->setStateInformation(a.getData(), (int)a.getSize());
    q->getStateInformation(b);
    require(a == b, "state roundtrip");

    for (int preset = 0; preset < 14; ++preset) {
        jmFactory(*q, preset);
        juce::MemoryBlock s1, s2;
        q->getStateInformation(s1);
        p->setStateInformation(s1.getData(), (int)s1.getSize());
        p->getStateInformation(s2);
        require(s1 == s2, "factory preset roundtrip");
    }
    p.reset(createPluginFilter());
    p->setRateAndBufferSizeDetails(48000, 512);
    set(*p, "auto_gain", 0);
    set(*p, "linear_phase", 1);
    jmService(*p);
    p->prepareToPlay(48000, 512);
    jmService(*p);
    require(p->getLatencySamples() == 256, "linear latency");
    juce::AudioBuffer<float> impulse(2, 512);
    impulse.clear();
    impulse.setSample(0, 0, 1);
    impulse.setSample(1, 0, 1);
    p->processBlock(impulse, midi);
    for (int i = 0; i < 512; ++i)
        require(std::abs(impulse.getSample(0, i) - (i == 256 ? 1.f : 0.f)) < 1e-5,
                "FIR identity delay");
    p->prepareToPlay(48000, 512);
    impulse.clear();
    impulse.setSample(0, 0, 1);
    p->processBlockBypassed(impulse, midi);
    for (int i = 0; i < 512; ++i)
        require(std::abs(impulse.getSample(0, i) - (i == 256 ? 1.f : 0.f)) < 1e-5,
                "bypass delay alignment");
    set(*p, "band1_enabled", 1);
    set(*p, "band1_gain", 6);
    set(*p, "band1_frequency", 1000);
    jmService(*p);
    p->prepareToPlay(48000, 1024);
    juce::AudioBuffer<float> fir(2, 1024);
    for (int n = 0; n < 100; ++n) {
        fir.clear();
        p->processBlock(fir, midi);
    }
    fir.clear();
    fir.setSample(0, 0, 1);
    p->processBlock(fir, midi);
    std::complex<double> response{};
    for (int n = 0; n < 513; ++n) {
        response +=
            (double)fir.getSample(0, n) * std::polar(1., -2 * 3.141592653589793 * 1000 * n / 48000);
        require(std::abs(fir.getSample(0, n) - fir.getSample(0, 512 - n)) < 1e-5,
                "FIR symmetric impulse");
    }
    require(std::abs(20 * std::log10(std::abs(response)) - 6) < .3,
            "FIR implements bell EQ magnitude");
    p.reset(createPluginFilter());
    q.reset(createPluginFilter());
    for (auto *proc : {p.get(), q.get()}) {
        proc->setRateAndBufferSizeDetails(48000, 256);
        proc->prepareToPlay(48000, 256);
        set(*proc, "auto_gain", 0);
    }
    jmGroup(*p, "integration-" + juce::String(juce::Time::currentTimeMillis()), "lead");
    juce::MemoryBlock groupState;
    p->getStateInformation(groupState);
    q->setStateInformation(groupState.getData(), (int)groupState.getSize());
    set(*q, "group_tier", 2);
    set(*q, "correction_mode", 2);
    auto feed = [&](juce::AudioProcessor &proc, int blocks) {
        juce::AudioBuffer<float> b(2, 256);
        for (int block = 0; block < blocks; ++block) {
            for (int n = 0; n < 256; ++n) {
                float x = .2f * std::sin(2 * 3.141592653589793 * 1000 * (block * 256 + n) / 48000.);
                b.setSample(0, n, x);
                b.setSample(1, n, x);
            }
            audioScope = true;
            proc.processBlock(b, midi);
            audioScope = false;
            require(audioAllocations == 0, "callback C++ allocation");
        }
    };
    feed(*p, 100);
    feed(*q, 100);
    for (int i = 0; i < 25; ++i) {
        jmService(*p);
        jmService(*q);
    }
    float cut = jmGroupCut(*q, 16);
    require(cut < -.01, "group masks lower tier");
    q->setNonRealtime(true);
    feed(*q, 1);
    p.reset();
    for (int i = 0; i < 10; ++i)
        jmService(*q);
    require(jmGroupCut(*q, 16) == cut, "offline freezes group curve");
    q->setNonRealtime(false);
    feed(*q, 1);
    for (int i = 0; i < 50; ++i)
        jmService(*q);
    require(std::abs(jmGroupCut(*q, 16)) < .01, "peer loss releases group cut");

    {
        auto groupName = "cross-process-" + juce::Uuid().toString();
        juce::ChildProcess child;
        require(
            child.start(juce::StringArray{
                juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),
                "--peer", groupName}),
            "cross-process peer launch");
        char ready[5]{};
        require(child.readProcessOutput(ready, 5) == 5 && std::string_view(ready, 5) == "READY",
                "cross-process ready");
        std::unique_ptr<juce::AudioProcessor> other(createPluginFilter());
        other->setRateAndBufferSizeDetails(48000, 256);
        other->prepareToPlay(48000, 256);
        jmGroup(*other, groupName, "background");
        set(*other, "group_tier", 2);
        set(*other, "correction_mode", 2);
        feed(*other, 100);
        for (int i = 0; i < 20; ++i)
            jmService(*other);
        require(jmGroupCut(*other, 16) < -.01, "cross-process spectral unmasking");
        child.kill();
        juce::Thread::sleep(3200);
        for (int i = 0; i < 50; ++i)
            jmService(*other);
        require(std::abs(jmGroupCut(*other, 16)) < .01, "stale crashed process releases group cut");
    }
    {
        auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("jmoney-reference", ".wav");
        juce::WavAudioFormat wav;
        auto stream = file.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wav.createWriterFor(stream.release(), 48000, 1, 24, {}, 0));
        require(writer != nullptr, "reference writer");
        juce::AudioBuffer<float> source(1, 96000);
        for (int n = 0; n < 96000; ++n)
            source.setSample(0, n, .2f * std::sin(2 * 3.141592653589793 * 500 * n / 48000));
        require(writer->writeFromAudioSampleBuffer(source, 0, 96000), "reference write");
        writer.reset();
        require(jmReference(*q, file), "reference analysis");
        file.deleteFile();
    }
    q.reset(createPluginFilter());
    q->setRateAndBufferSizeDetails(48000, 256);
    q->prepareToPlay(48000, 256);
    set(*q, "learn_trigger", 1);
    feed(*q, 2300);
    juce::MemoryBlock learned;
    q->getStateInformation(learned);
    auto xml = juce::AudioProcessor::getXmlFromBinary(learned.getData(), (int)learned.getSize());
    require(xml != nullptr, "learn state valid");
    bool corrected = false;
    for (int i = 0; i < 32; ++i)
        corrected |= std::abs(xml->getDoubleAttribute("curve" + juce::String(i))) > .01;
    require(corrected, "learning stores nonzero bounded correction");
    juce::AudioBuffer<float> invalid(2, 256);
    invalid.clear();
    invalid.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    invalid.setSample(1, 1, std::numeric_limits<float>::infinity());
    q->processBlock(invalid, midi);
    for (int c = 0; c < 2; ++c)
        for (int n = 0; n < 256; ++n)
            require(std::isfinite(invalid.getSample(c, n)), "invalid audio sanitized");
    auto start = std::chrono::steady_clock::now();
    feed(*q, 1875);
    std::cout << "Full processor timing: one stereo instance, 48000 Hz, 256 samples, learned curve "
                 "enabled, 10 seconds audio in "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
              << " seconds\n";

    for (int i = 0; i < 24; ++i) {
        set(*q, "band" + juce::String(i + 1) + "_enabled", 1);
        set(*q, "band" + juce::String(i + 1) + "_dynamic", 1);
        set(*q, "band" + juce::String(i + 1) + "_frequency", (float)(40 * std::pow(400., i / 23.)));
        set(*q, "band" + juce::String(i + 1) + "_gain", -.5f);
    }
    set(*q, "linear_phase", 1);
    jmService(*q);
    auto fullStart = std::chrono::steady_clock::now();
    feed(*q, 1875);
    std::cout << "Full processor timing: one stereo instance, 48000 Hz, 256 samples, 24 dynamic "
                 "bands + learned curve + linear FIR, 10 seconds audio in "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - fullStart).count()
              << " seconds\n";
    {
        std::unique_ptr<juce::AudioProcessor> quiet(createPluginFilter());
        quiet->setRateAndBufferSizeDetails(48000, 256);
        quiet->prepareToPlay(48000, 256);
        set(*quiet, "learn_trigger", 1);
        juce::AudioBuffer<float> block(2, 256);
        uint32_t seed = 123;
        for (int n = 0; n < 2300; ++n) {
            for (int i = 0; i < 256; ++i) {
                seed = seed * 1664525u + 1013904223u;
                float v = ((float)(seed & 65535) / 32768.f - 1) * 1e-6f;
                block.setSample(0, i, v);
                block.setSample(1, i, v);
            }
            audioScope = true;
            quiet->processBlock(block, midi);
            audioScope = false;
        }
        juce::MemoryBlock saved;
        quiet->getStateInformation(saved);
        auto data = juce::AudioProcessor::getXmlFromBinary(saved.getData(), (int)saved.getSize());
        require(data != nullptr, "noise state");
        for (int i = 0; i < 32; ++i)
            require(data->getDoubleAttribute("curve" + juce::String(i)) == 0,
                    "low noise does not learn correction");
        require(audioAllocations == 0, "noise callback allocation");
    }
    {
        std::vector<std::unique_ptr<juce::AudioProcessor>> ten;
        auto name = "ten-peer-" + juce::Uuid().toString();
        for (int i = 0; i < 10; ++i) {
            ten.emplace_back(createPluginFilter());
            auto &v = *ten.back();
            v.setRateAndBufferSizeDetails(48000, 256);
            v.prepareToPlay(48000, 256);
            jmGroup(v, name, "instance " + juce::String(i));
            set(v, "group_tier", i == 0 ? 0 : 2);
            set(v, "correction_mode", 2);
            feed(v, 100);
            jmService(v);
        }
        for (int n = 0; n < 20; ++n)
            for (auto &v : ten)
                jmService(*v);
        require(jmGroupCut(*ten[0], 16) == 0, "ten peers keep lead unchanged");
        for (int i = 1; i < 10; ++i)
            require(jmGroupCut(*ten[(size_t)i], 16) < -.01, "ten peers reduce backgrounds");
        auto begin = std::chrono::steady_clock::now();
        for (auto &v : ten)
            feed(*v, 1875);
        std::cout << "Ten-instance timing: stereo 48000 Hz, 256 samples, group unmasking, minimum "
                     "phase, 10 seconds per instance in "
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count()
                  << " seconds\n";
    }
    std::cout << "Integration passed: mono/stereo, six rates, six block sizes, identity, double "
                 "silence, state roundtrip.\n";
}
