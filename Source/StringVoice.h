/*
  ==============================================================================

    StringVoice.h
    Created: 25 Apr 2026 9:14:26am
    Author:  Riley Knybel

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>

class StringVoice : public juce::SynthesiserVoice
{
public:
    StringVoice();

    bool canPlaySound(juce::SynthesiserSound*) override { return true; }

    void prepare(double sampleRate, int samplesPerBlock);

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int currentPitchWheelPosition) override;

    void stopNote(float velocity, bool allowTailOff) override;

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample,
                         int numSamples) override;
    
    // Visualizer
    void copyStringState(std::vector<float>& destination) const;
    int getCurrentNumPoints() const;

    // Parameter Setters
    void setDecay(float newDecay);
    void setColor(float newColor);
    void setPickupPosition(float newPickupPosition);
    void setLetStringsRing(bool shouldRing);
    
    void setPickPosition(float newPickPosition);
    void setPickWidth(float newPickWidth);
    void setPickStrength(float newPickStrength);
    
    void setPalmPosition(float newPalmPosition);
    void setPalmDamping(float newPalmDamping);
    void setBridgeDamping(float newBridgeDamping);
    void setDispersion(float newDispersion);
    void setHarmonics(float newHarmonics);
    void setPickNoiseAmount(float newPickNoiseAmount);

private:
    static constexpr int maxPoints = 256;

    float getNextSample();
    void exciteString(float velocity);
    void updateString();
    void startNewStringNote(int midiNoteNumber, float velocityValue);

    double sr = 44100.0;
    int fadeInSamples = 0;
    int fadeInCounter = 0;

    std::array<float, maxPoints> pos {};
    std::array<float, maxPoints> vel {};

    int numPoints = 96;
    int subSteps = 1; // For really high (tiny string) note oversampling

    float rate = 0.5f;
    float damping = 0.9995f;

    float velocityGain = 0.0f;
    float outputGain = 1.0f;

    bool isReleasing = false;
    float releaseGain = 1.0f;
    
    float colorFilterState = 0.0f;
    
    // State variables to fix popping on voice steals
    bool pendingNewNote = false;
    int pendingMidiNote = 0;
    float pendingVelocity = 0.0f;
    int stealFadeSamples = 0;
    int stealFadeCounter = 0;

    // String Parameters
    float decay = 0.8f;
    float color = 0.5f;
    float pickupPosition = 0.85f; // 0 = nut side, 1 = bridge side
    bool letStringsRing = true;
    
    // Pick Parameters
    float pickPosition = 0.25f;
    float pickWidth = 0.25f;
    float pickStrength = 0.1f;
    float harmonics = 0.18f;
    float pickNoiseAmount = 0.25f;
    
    // Damping Parameters
    float palmPosition = 0.85f; // 0=nut, 1=bridge (usually near bridge)
    float palmDamping = 0.0f;   // 0=no mute, 1=heavy mute
    float bridgeDamping = 0.05f;
    
    // String Parameters
    float dispersion = 0.05f;
    
    // Counts how long the string has been quiet before clearing the voice.
    int quietSamples = 0;

    int pickupIndex = 48;
};
