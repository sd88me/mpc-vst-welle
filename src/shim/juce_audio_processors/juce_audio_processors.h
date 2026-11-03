// MPC port shim: juce_audio_processors without its GUI half (juce_gui_extra). The headless processor module plus
// a copy of AudioProcessorValueTreeState with the Slider/ComboBox/Button attachments removed (apvts.h/.cpp).
#pragma once
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_data_structures/juce_data_structures.h>
namespace juce { class AudioProcessorEditor; }
#include "apvts.h"
