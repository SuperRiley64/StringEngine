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

void StringEngine::setStrumAmount(float newAmount)
{
    strumAmount = juce::jlimit(-1.0f, 1.0f, newAmount);
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

void StringEngine::scheduleChordNotes(std::vector<juce::MidiMessage>& noteOns,
                                      int samplePosition)
{
    if (noteOns.empty())
        return;

    struct AssignedNote
    {
        int midiNote = 0;
        float velocity = 0.0f;
        int stringIndex = 0;
    };

    std::vector<AssignedNote> assigned;
    assigned.reserve(noteOns.size());

    for (const auto& note : noteOns)
    {
        const int midiNote = note.getNoteNumber();
        const float velocity = note.getFloatVelocity();

        const int stringIndex = chooseStringForNote(midiNote);

        // Reserve immediately so chord notes do not grab the same string.
        stringStartOrder[(size_t)stringIndex] = nextStartOrder++;

        assigned.push_back({ midiNote, velocity, stringIndex });
    }

    // -1 = down strum, +1 = up strum.
    // If 0 = low string and 5 = high string:
    // down strum = low -> high = ascending stringIndex
    // up strum   = high -> low = descending stringIndex
    if (strumAmount < 0.0f)
    {
        std::sort(assigned.begin(), assigned.end(),
                  [] (const AssignedNote& a, const AssignedNote& b)
                  {
                      return a.stringIndex < b.stringIndex;
                  });
    }
    else if (strumAmount > 0.0f)
    {
        std::sort(assigned.begin(), assigned.end(),
                  [] (const AssignedNote& a, const AssignedNote& b)
                  {
                      return a.stringIndex > b.stringIndex;
                  });
    }

    const float strumDepth = std::abs(strumAmount);
    const int count = (int)assigned.size();

    const int maxStrumSamples =
        (int)std::round(0.070f * (float)sr * strumDepth);

    const int stepSamples =
        count > 1
            ? juce::jmax(1, maxStrumSamples / (count - 1))
            : 0;

    for (int i = 0; i < count; ++i)
    {
        ScheduledNote scheduled;
        scheduled.midiNote = assigned[(size_t)i].midiNote;
        scheduled.velocity = assigned[(size_t)i].velocity;
        scheduled.stringIndex = assigned[(size_t)i].stringIndex;
        scheduled.samplesUntilStart = samplePosition + (i * stepSamples);

        scheduledNotes.push_back(scheduled);
    }

    std::sort(scheduledNotes.begin(), scheduledNotes.end(),
              [] (const ScheduledNote& a, const ScheduledNote& b)
              {
                  return a.samplesUntilStart < b.samplesUntilStart;
              });
}

void StringEngine::startScheduledNotesForSample()
{
    for (auto it = scheduledNotes.begin(); it != scheduledNotes.end(); )
    {
        if (it->samplesUntilStart <= 0)
        {
            if (auto* string = getVoice(it->stringIndex))
            {
                string->startNote(it->midiNote, it->velocity, nullptr, 0);
                stringStartOrder[(size_t)it->stringIndex] = nextStartOrder++;
            }

            it = scheduledNotes.erase(it);
        }
        else
        {
            --(it->samplesUntilStart);
            ++it;
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
    std::vector<juce::MidiMessage> blockNoteOns;

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
            blockNoteOns.push_back(message);
        else
            handleMidiEvent(message);
    }

    if (!blockNoteOns.empty())
        scheduleChordNotes(blockNoteOns, 0);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        startScheduledNotesForSample();

        if (++sympatheticCounter >= 256)
        {
            sympatheticCounter = 0;
            applySympatheticResonance();
        }

        for (auto& string : strings)
            string->renderNextBlock(outputBuffer, startSample + sample, 1);
    }
}
