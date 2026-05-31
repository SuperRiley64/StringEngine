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
    
    for (int i = 0; i < stringCount; ++i)
    {
        stringHeldNote[(size_t)i] = -1;
        stringIsHeld[(size_t)i] = false;
        stringHasHistory[(size_t)i] = false;
    }
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

void StringEngine::setSlideTimeMs(float newTimeMs)
{
    slideTimeMs = juce::jlimit(0.0f, 5000.0f, newTimeMs);
}

int StringEngine::findStringHoldingNote(int midiNoteNumber) const
{
    for (int i = 0; i < stringCount; ++i)
    {
        if (stringIsHeld[(size_t)i] &&
            stringHeldNote[(size_t)i] == midiNoteNumber)
        {
            return i;
        }
    }

    return -1;
}

int StringEngine::chooseStringForNote(int midiNoteNumber)
{
    // 1. If this exact note is currently held, reuse that string.
    if (const int existing = findStringHoldingNote(midiNoteNumber); existing >= 0)
        return existing;

    // 2. Prefer a string that is not currently held.
    // This allows normal chords to use multiple strings.
    int bestFree = -1;
    float lowestFreeEnergy = std::numeric_limits<float>::max();

    for (int i = 0; i < stringCount; ++i)
    {
        if (stringIsHeld[(size_t)i])
            continue;

        auto* string = getVoice(i);
        if (string == nullptr)
            continue;

        const float energy = string->getStringEnergy();

        if (energy < lowestFreeEnergy)
        {
            lowestFreeEnergy = energy;
            bestFree = i;
        }
    }

    if (bestFree >= 0)
        return bestFree;

    // 3. No free strings: steal quietest, tie-break oldest.
    int bestSteal = 0;
    float quietestEnergy = std::numeric_limits<float>::max();
    int oldestOrder = std::numeric_limits<int>::max();

    for (int i = 0; i < stringCount; ++i)
    {
        auto* string = getVoice(i);
        if (string == nullptr)
            continue;

        const float energy = string->getStringEnergy();
        const int order = stringStartOrder[(size_t)i];

        if (energy < quietestEnergy ||
            (std::abs(energy - quietestEnergy) < 1.0e-6f && order < oldestOrder))
        {
            quietestEnergy = energy;
            oldestOrder = order;
            bestSteal = i;
        }
    }

    return bestSteal;
}

void StringEngine::releaseNote(int midiNoteNumber)
{
    for (int i = 0; i < stringCount; ++i)
    {
        if (stringHeldNote[(size_t)i] == midiNoteNumber)
        {
            stringIsHeld[(size_t)i] = false;

            if (auto* string = getVoice(i))
                string->stopNote(0.0f, true);

            // Do NOT clear stringHeldNote.
            // Poly slide uses this as the previous picked pitch.
        }
    }
}

void StringEngine::handleMidiEvent(const juce::MidiMessage& message)
{
    if (message.isNoteOff())
        releaseNote(message.getNoteNumber());
}

// Mono/legato setters
void StringEngine::setMonoLegato(bool shouldUseMono)
{
    monoLegato = shouldUseMono;
}

void StringEngine::setLegatoTimeMs(float newTimeMs)
{
    legatoTimeMs = juce::jlimit(0.0f, 5000.0f, newTimeMs);
}

void StringEngine::handleMonoMidiEvent(const juce::MidiMessage& message)
{
    auto* string = getVoice(monoStringIndex);

    if (string == nullptr)
        return;

    if (message.isNoteOn())
    {
        const int midiNote = message.getNoteNumber();
        const float velocity = message.getFloatVelocity();

        const bool shouldLegato = !heldMonoNotes.empty();

        heldMonoNotes.erase(
            std::remove(heldMonoNotes.begin(), heldMonoNotes.end(), midiNote),
            heldMonoNotes.end()
        );

        heldMonoNotes.push_back(midiNote);

        if (shouldLegato)
        {
            string->setSlideTimeMs(legatoTimeMs);
            string->startLegatoNote(midiNote, velocity, nullptr, 0);
        }
        else
        {
            // First note: no legato slide.
            string->setSlideTimeMs(0.0f);
            string->startNote(midiNote, velocity, nullptr, 0);
        }

        stringStartOrder[(size_t)monoStringIndex] = nextStartOrder++;
        return;
    }

    if (message.isNoteOff())
    {
        const int midiNote = message.getNoteNumber();

        heldMonoNotes.erase(
            std::remove(heldMonoNotes.begin(), heldMonoNotes.end(), midiNote),
            heldMonoNotes.end()
        );

        if (!heldMonoNotes.empty())
        {
            const int previousNote = heldMonoNotes.back();

            string->setSlideTimeMs(legatoTimeMs);
            string->startLegatoNote(previousNote, 0.7f, nullptr, 0);
        }
        else
        {
            string->setSlideTimeMs(0.0f);
            string->stopNote(0.0f, true);
        }
    }
}

// Strum functions

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

    struct HeldString
    {
        int midiNote = 0;
        int stringIndex = 0;
    };

    std::vector<AssignedNote> assigned;
    assigned.reserve(noteOns.size());

    std::vector<juce::MidiMessage> sortedNotes = noteOns;

    std::sort(sortedNotes.begin(), sortedNotes.end(),
              [] (const juce::MidiMessage& a, const juce::MidiMessage& b)
              {
                  return a.getNoteNumber() < b.getNoteNumber();
              });

    std::vector<HeldString> heldStrings;

    // Chord slide mapping
    for (int i = 0; i < stringCount; ++i)
    {
        if (stringHeldNote[(size_t)i] >= 0)
            heldStrings.push_back({ stringHeldNote[(size_t)i], i });
    }

    std::sort(heldStrings.begin(), heldStrings.end(),
              [] (const HeldString& a, const HeldString& b)
              {
                  return a.midiNote < b.midiNote;
              });

    std::array<bool, stringCount> reservedThisChord {};
    reservedThisChord.fill(false);

    // ===== Slide mode: preserve chord voice order =============================
    if (slideTimeMs > 0.0f && !heldStrings.empty())
    {
        const int pairedCount =
            juce::jmin((int)sortedNotes.size(), (int)heldStrings.size());

        for (int i = 0; i < pairedCount; ++i)
        {
            const auto& note = sortedNotes[(size_t)i];
            const int stringIndex = heldStrings[(size_t)i].stringIndex;

            reservedThisChord[(size_t)stringIndex] = true;

            assigned.push_back({
                note.getNoteNumber(),
                note.getFloatVelocity(),
                stringIndex
            });
        }

        // Extra new notes get free strings.
        for (int i = pairedCount; i < (int)sortedNotes.size(); ++i)
        {
            const auto& note = sortedNotes[(size_t)i];

            int stringIndex = -1;

            for (int s = 0; s < stringCount; ++s)
            {
                if (!reservedThisChord[(size_t)s] && !stringIsHeld[(size_t)s])
                {
                    stringIndex = s;
                    break;
                }
            }

            if (stringIndex < 0)
            {
                for (int s = 0; s < stringCount; ++s)
                {
                    if (!reservedThisChord[(size_t)s])
                    {
                        stringIndex = s;
                        break;
                    }
                }
            }

            if (stringIndex >= 0)
            {
                reservedThisChord[(size_t)stringIndex] = true;

                assigned.push_back({
                    note.getNoteNumber(),
                    note.getFloatVelocity(),
                    stringIndex
                });
            }
        }
    }
    else
    {
        // ===== Normal mode: allocate separate strings for chord notes ==========
        for (const auto& note : sortedNotes)
        {
            const int midiNote = note.getNoteNumber();
            const float velocity = note.getFloatVelocity();

            int stringIndex = chooseStringForNote(midiNote);

            if (reservedThisChord[(size_t)stringIndex])
            {
                for (int i = 0; i < stringCount; ++i)
                {
                    if (!reservedThisChord[(size_t)i] && !stringIsHeld[(size_t)i])
                    {
                        stringIndex = i;
                        break;
                    }
                }
            }

            reservedThisChord[(size_t)stringIndex] = true;

            assigned.push_back({ midiNote, velocity, stringIndex });
        }
    }

    if (strumAmount < 0.0f)
    {
        std::sort(assigned.begin(), assigned.end(),
                  [] (const AssignedNote& a, const AssignedNote& b)
                  {
                      return a.midiNote < b.midiNote;
                  });
    }
    else if (strumAmount > 0.0f)
    {
        std::sort(assigned.begin(), assigned.end(),
                  [] (const AssignedNote& a, const AssignedNote& b)
                  {
                      return a.midiNote > b.midiNote;
                  });
    }

    const float strumDepth = std::abs(strumAmount);
    const int count = (int)assigned.size();

    const int maxStrumSamples =
        (int)std::round(0.070f * (float)sr * strumDepth);

    const int stepSamples =
        count > 1 ? juce::jmax(1, maxStrumSamples / (count - 1)) : 0;

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
                const size_t index = (size_t)it->stringIndex;

                const int oldNote = stringHeldNote[index];

                const bool shouldSlide =
                    slideTimeMs > 0.0f &&
                    stringHasHistory[index] &&
                    oldNote >= 0 &&
                    oldNote != it->midiNote;

                if (shouldSlide)
                {
                    string->setSlideTimeMs(slideTimeMs);
                    string->startPickedSlideNote(oldNote, it->midiNote, it->velocity);
                }
                else
                {
                    string->setSlideTimeMs(0.0f);
                    string->startNote(it->midiNote, it->velocity, nullptr, 0);
                }

                stringHeldNote[index] = it->midiNote;
                stringIsHeld[index] = true;
                stringHasHistory[index] = true;
                stringStartOrder[index] = nextStartOrder++;
            }

            it = scheduledNotes.erase(it);
        }
        else
        {
            --it->samplesUntilStart;
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
    if (monoLegato)
        {
            for (const auto metadata : midiMessages)
                handleMonoMidiEvent(metadata.getMessage());
            
            for (int sample = 0; sample < numSamples; ++sample)
            {
                if (++sympatheticCounter >= 256)
                {
                    sympatheticCounter = 0;
                    applySympatheticResonance();
                }
                
                for (auto& string : strings)
                    string->renderNextBlock(outputBuffer, startSample + sample, 1);
            }

            return;
        }
    
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
