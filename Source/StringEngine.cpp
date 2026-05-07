/*
  ==============================================================================

    StringEngine.cpp
    Created: 7 May 2026 3:36:06pm
    Author:  Riley Knybel

  ==============================================================================
*/

#include "StringEngine.h"

StringEngine::StringEngine()
{
    for (int i = 0; i < stringCount; ++i)
        strings[(size_t)i] = std::make_unique<StringVoice>(openMidiNotes[(size_t)i]);
}

void StringEngine::prepare(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    blockSize = samplesPerBlock;

    for (auto& string : strings)
        string->prepare(sampleRate, samplesPerBlock);
}

void StringEngine::setCurrentPlaybackSampleRate(double sampleRate)
{
    sr = sampleRate;
}

int StringEngine::getNumVoices() const
{
    return stringCount;
}

StringVoice* StringEngine::getVoice(int index)
{
    if (index < 0 || index >= stringCount)
        return nullptr;

    return strings[(size_t)index].get();
}

const StringVoice* StringEngine::getVoice(int index) const
{
    if (index < 0 || index >= stringCount)
        return nullptr;

    return strings[(size_t)index].get();
}

void StringEngine::setSympatheticAmount(float newAmount)
{
    sympatheticAmount = juce::jlimit(0.0f, 1.0f, newAmount);
}

int StringEngine::chooseStringForNote(int midiNoteNumber)
{
    // ===== steal the oldest active string ==================
    int oldestString = 0;
    int oldestOrder = std::numeric_limits<int>::max();

    for (int i = 0; i < stringCount; ++i)
    {
        const int order = stringStartOrder[(size_t)i];

        if (order < oldestOrder)
        {
            oldestOrder = order;
            oldestString = i;
        }
    }

    return oldestString;
}

void StringEngine::handleMidiEvent(const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        const int midiNote = message.getNoteNumber();
        const float velocity = message.getFloatVelocity();

        const int stringIndex = chooseStringForNote(midiNote);
        
        // Set note age
        if (auto* string = getVoice(stringIndex))
        {
            string->startNote(midiNote, velocity, nullptr, 0);
            stringStartOrder[(size_t)stringIndex] = nextStartOrder++;
        }

        if (auto* string = getVoice(stringIndex))
            string->startNote(midiNote, velocity, nullptr, 0);

        return;
    }

    if (message.isNoteOff())
    {
        const int midiNote = message.getNoteNumber();

        for (auto& string : strings)
        {
            if (string->getCurrentMidiNote() == midiNote)
                string->stopNote(0.0f, true);
        }
    }
}

void StringEngine::applySympatheticResonance()
{
    if (sympatheticAmount <= 0.001f)
        return;

    const float amount =
        juce::jmap(std::pow(sympatheticAmount, 1.5f),
                   0.0f, 1.0f,
                   0.0f, 1.0f);

    for (int source = 0; source < stringCount; ++source)
    {
        const auto* sourceVoice = strings[(size_t)source].get();

        if (sourceVoice == nullptr)
            continue;

        if (sourceVoice->getStringEnergy() < 1.0e-6f)
            continue;

        for (int target = 0; target < stringCount; ++target)
        {
            if (source == target)
                continue;

            auto* targetVoice = strings[(size_t)target].get();

            if (targetVoice == nullptr)
                continue;

            targetVoice->injectSympatheticEnergy(*sourceVoice, amount);
        }
    }
}

void StringEngine::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                   juce::MidiBuffer& midiMessages,
                                   int startSample,
                                   int numSamples)
{
    for (const auto metadata : midiMessages)
        handleMidiEvent(metadata.getMessage());

    applySympatheticResonance();

    for (auto& string : strings)
        string->renderNextBlock(outputBuffer, startSample, numSamples);
}
