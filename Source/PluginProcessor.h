#pragma once

#include <JuceHeader.h>
#include <array>
#include "StringVoice.h"

class StringSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override      { return true; }
    bool appliesToChannel (int) override   { return true; }
};

class GuitarSynthAudioProcessor  : public juce::AudioProcessor
{
public:
    GuitarSynthAudioProcessor();
    ~GuitarSynthAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    // ===== Visualizer =========================================================
    void copyActiveStringStates(std::array<std::vector<float>, 6>& destinations) const;
    
    // ===== Body resonator =====================================================
    void processBodyResonator(juce::AudioBuffer<float>& buffer);
    void updateBodyResonatorParameters(float bodyMix, float bodySize, float bodyDamping);

    static constexpr int bodyModeCount = 6;

    std::array<juce::dsp::IIR::Filter<float>, bodyModeCount> bodyFiltersL;
    std::array<juce::dsp::IIR::Filter<float>, bodyModeCount> bodyFiltersR;

    std::array<float, bodyModeCount> bodyBaseFrequencies
    {
        90.0f,
        140.0f,
        220.0f,
        315.0f,
        480.0f,
        720.0f
    };

    std::array<float, bodyModeCount> bodyModeGains
    {
        0.70f,
        0.55f,
        0.45f,
        0.35f,
        0.25f,
        0.18f
    };

    float currentBodyMix = 0.0f;
    float currentBodySize = 0.5f;
    float currentBodyDamping = 0.5f;
    
    // ===== Parameters ===========================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::Synthesiser synth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarSynthAudioProcessor)
};
