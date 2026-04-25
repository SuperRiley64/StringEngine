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

    // Parameter Setters
    void setDecay(float newDecay);
    void setColor(float newColor);
    void setLetStringsRing(bool shouldRing);

private:
    static constexpr int maxPoints = 256;

    float getNextSample();
    void exciteString(float velocity);
    void updateString();

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

    // Parameters
    float decay = 0.8f;
    float color = 0.5f;
    // TODO: Make this a checkbox parameter.
    bool letStringsRing = true;
    
    // Counts how long the string has been quiet before clearing the voice.
    int quietSamples = 0;

    int pickupIndex = 48;
};
