#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
void jmFactory(juce::AudioProcessor &, int);
void jmService(juce::AudioProcessor &);
bool jmReference(juce::AudioProcessor &, const juce::File &);
void jmGroup(juce::AudioProcessor &, const juce::String &, const juce::String &);
float jmGroupCut(juce::AudioProcessor &, int);
