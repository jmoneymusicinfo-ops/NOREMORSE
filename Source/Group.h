#pragma once
#include <array>
#include <juce_audio_utils/juce_audio_utils.h>
// Called only on the message thread. Disk snapshots also work across host processes.
class GroupLink {
    juce::String uuid = juce::Uuid().toString();
    juce::File own;

  public:
    struct Peer {
        juce::String id, name;
        int tier = 0;
        bool bypass = false;
        std::array<float, 32> spectrum{};
    };
    std::vector<Peer> peers;
    juce::String group, name = "Vocal", overview = "Group disabled";
    std::array<std::atomic<float>, 32> reduction{};
    void command(const juce::String &target, const juce::String &key, float value) {
        if (own == juce::File())
            return;
        juce::XmlElement xml("Command");
        xml.setAttribute("parameter", key);
        xml.setAttribute("value", value);
        xml.setAttribute("time", juce::String(juce::Time::currentTimeMillis()));
        auto path = own.getSiblingFile(target + ".cmd");
        juce::TemporaryFile temp(path);
        if (xml.writeTo(temp.getFile()))
            temp.overwriteTargetFileWithTemporary();
    }
    std::pair<juce::String, float> takeCommand() {
        if (own == juce::File())
            return {};
        auto path = own.withFileExtension("cmd");
        if (!path.existsAsFile())
            return {};
        auto xml = path.getSize() < 2048 ? juce::XmlDocument::parse(path) : nullptr;
        path.deleteFile();
        if (!xml || !xml->hasTagName("Command") ||
            std::abs(juce::Time::currentTimeMillis() -
                     xml->getStringAttribute("time").getLargeIntValue()) > 3000)
            return {};
        return {xml->getStringAttribute("parameter"), (float)xml->getDoubleAttribute("value")};
    }
    ~GroupLink() {
        if (own != juce::File())
            own.deleteFile();
    }
    void tick(const std::array<std::atomic<float>, 32> &spectrum, int tier, bool bypass,
              float amount, bool offline) {
        peers.clear();
        if (group.isEmpty()) {
            if (own != juce::File())
                own.deleteFile();
            for (auto &x : reduction)
                x.store(0);
            overview = "Group disabled";
            return;
        }
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("JMoneyVEQ")
                       .getChildFile(juce::String::toHexString(group.hashCode64()));
        dir.createDirectory();
        auto next = dir.getChildFile(uuid + ".xml");
        if (own != juce::File() && own != next)
            own.deleteFile();
        own = next;
        juce::XmlElement xml("Peer");
        xml.setAttribute("id", uuid);
        xml.setAttribute("name", name.substring(0, 80));
        xml.setAttribute("tier", tier);
        xml.setAttribute("bypass", bypass);
        xml.setAttribute("time", juce::String(juce::Time::currentTimeMillis()));
        for (int i = 0; i < 32; ++i)
            xml.setAttribute("s" + juce::String(i), spectrum[i].load());
        // Replace atomically; no reader observes a partially-written snapshot.
        juce::TemporaryFile temp(own);
        if (xml.writeTo(temp.getFile()))
            temp.overwriteTargetFileWithTemporary();
        if (offline) {
            overview = "Offline: last group curve frozen; print group decisions before export";
            return;
        }
        std::array<float, 32> requested{};
        int count = 0;
        overview = "";
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.xml");
        files.sort();
        for (auto &f : files) {
            if (f.getSize() > 16384)
                continue;
            auto peer = juce::XmlDocument::parse(f);
            if (!peer || !peer->hasTagName("Peer"))
                continue;
            auto age = juce::Time::currentTimeMillis() -
                       peer->getStringAttribute("time").getLargeIntValue();
            if (age > 3000 || age < 0)
                continue;
            if (++count > 10)
                break;
            Peer row;
            row.id = peer->getStringAttribute("id");
            row.name = peer->getStringAttribute("name");
            row.tier = peer->getIntAttribute("tier");
            row.bypass = peer->getBoolAttribute("bypass");
            for (int i = 0; i < 32; ++i)
                row.spectrum[i] = (float)peer->getDoubleAttribute("s" + juce::String(i), -120);
            peers.push_back(row);
            overview +=
                peer->getStringAttribute("name") + " [" + peer->getStringAttribute("tier") + "] ";
            if (f == own || peer->getBoolAttribute("bypass") ||
                peer->getIntAttribute("tier") >= tier)
                continue;
            for (int i = 0; i < 32; ++i) {
                float other = (float)peer->getDoubleAttribute("s" + juce::String(i), -120),
                      self = spectrum[i].load();
                if (other > -55 && self > -65) {
                    float overlap = juce::jlimit(0.f, 1.f, (std::min(self, other) + 65) / 35);
                    requested[i] = std::max(requested[i], amount * overlap);
                }
            }
        }
        for (int i = 0; i < 32; ++i) {
            float sum = 0;
            int n = 0;
            for (int j = std::max(0, i - 2); j <= std::min(31, i + 2); ++j) {
                sum += requested[j];
                ++n;
            }
            float prev = reduction[i].load();
            reduction[i].store(prev + .15f * (-sum / n - prev));
        }
    }
};
