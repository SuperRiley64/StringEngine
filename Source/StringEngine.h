/*
  ==============================================================================

    StringEngine.h
    Created: 7 May 2026 3:36:06pm
    Author:  Riley Knybel

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "StringVoice.h"

class StringEngine
{
public:
    StringEngine();

    void prepare(double sampleRate, int samplesPerBlock);
    void setCurrentPlaybackSampleRate(double sampleRate);

    int getNumVoices() const;
    StringVoice* getVoice(int index);
    const StringVoice* getVoice(int index) const;

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         juce::MidiBuffer& midiMessages,
                         int startSample,
                         int numSamples);

    void setSympatheticAmount(float newAmount);
    void setStrumAmount(float newAmount);
    void setMonoLegato(bool shouldUseMono);
    void setLegatoTimeMs(float newTimeMs);

private:
    static constexpr int stringCount = 6;

    // Standard guitar tuning for now:
    // E2 A2 D3 G3 B3 E4
    std::array<int, stringCount> openMidiNotes
    {
        40, 45, 50, 55, 59, 64
    };

    std::array<std::unique_ptr<StringVoice>, stringCount> strings;
    
    std::array<int, stringCount> stringStartOrder {};
    int nextStartOrder = 1;

    double sr = 44100.0;
    int blockSize = 512;

    float sympatheticAmount = 0.0f;
    int sympatheticCounter = 0;
    float strumAmount = 0.0f;
    
    // Mono/legato state variables
    void handleMonoMidiEvent(const juce::MidiMessage& message);
    bool monoLegato = false;
    float legatoTimeMs = 0.0f;

    int monoStringIndex = 0;
    std::vector<int> heldMonoNotes;
    
    // Strumming state variables
    struct ScheduledNote
    {
        int midiNote = 0;
        float velocity = 0.0f;
        int stringIndex = 0;
        int samplesUntilStart = 0;
    };
    
    std::vector<ScheduledNote> scheduledNotes;
    
    // Strumming functions
    void startScheduledNotesForSample();
    void scheduleChordNotes(std::vector<juce::MidiMessage>& noteOns, int samplePosition);

    // Other functions
    int chooseStringForNote(int midiNoteNumber);
    void handleMidiEvent(const juce::MidiMessage& message);
    void applySympatheticResonance();
};
