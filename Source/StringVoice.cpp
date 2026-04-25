/*
  ==============================================================================

    StringVoice.cpp
    Created: 25 Apr 2026 9:14:26am
    Author:  Riley Knybel

  ==============================================================================
*/

#include "StringVoice.h"

StringVoice::StringVoice()
{
}

void StringVoice::prepare(double sampleRate, int)
{
    sr = sampleRate;

    pos.fill(0.0f);
    vel.fill(0.0f);
}

void StringVoice::setDecay(float newDecay)
{
    decay = juce::jlimit(0.0f, 1.0f, newDecay);
}

void StringVoice::setColor(float newColor)
{
    color = juce::jlimit(0.0f, 1.0f, newColor);
}

void StringVoice::setLetStringsRing(bool shouldRing)
{
    letStringsRing = shouldRing;
}

void StringVoice::startNote(int midiNoteNumber, float velocityValue,
                            juce::SynthesiserSound*, int)
{
    fadeInSamples = juce::jmax(1, (int)(0.003 * sr)); // 3 ms fade-in to reduce note-on clicks
    fadeInCounter = 0;
    quietSamples = 0;

    pos.fill(0.0f);
    vel.fill(0.0f);

    velocityGain = velocityValue;
    isReleasing = false;

    const float freq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

    // Stability limit for this simple finite-difference model.
    // TODO: Make this a Stability / Quality parameter if you add quality modes.
    // Lower = safer/smoother, but can require more substeps for high notes.
    const float maxStableRate = 0.70f;

    // Keep a minimum spatial resolution so high notes do not collapse into
    // tiny 3-5 point strings. Tiny strings tune badly and sound buzzy.
    // TODO: Make this a Quality parameter.
    // Higher minimum = smoother high notes, but more CPU.
    const int minUsefulPoints = 16;

    // Pick enough substeps so even minUsefulPoints can hit the requested pitch
    // without rate exceeding maxStableRate.
    //
    // Fixed string approximation:
    // fundamentalFreq ~= rate * effectiveSampleRate / (2 * (numPoints - 1))
    //
    // Therefore:
    // rate = 2 * freq * (numPoints - 1) / effectiveSampleRate
    subSteps = 1;

    while ((2.0f * freq * float(minUsefulPoints - 1))
           / (float(sr) * float(subSteps)) > maxStableRate)
    {
        ++subSteps;
    }

    // Now choose as many points as possible for this note/substep count
    // without violating the stability limit.
    const int maxAllowedPoints =
        1 + (int)std::floor((maxStableRate * float(sr) * float(subSteps))
                            / (2.0f * freq));

    numPoints = juce::jlimit(minUsefulPoints, maxPoints, maxAllowedPoints);

    // TODO: Later this becomes Pickup Position parameter.
    float pickupPos = 0.85f; // near bridge
    pickupIndex = int(pickupPos * (numPoints - 1));

    // Corrected pitch rate.
    // This is the important fix.
    rate = (2.0f * freq * float(numPoints - 1))
           / (float(sr) * float(subSteps));

    rate = juce::jlimit(0.01f, maxStableRate, rate);

    // Decay controls broad sustain.
    // Color currently makes the string slightly livelier/brighter.
    const float sustainDamping = juce::jmap(decay, 0.0f, 1.0f, 0.992f, 0.9999f);
    const float colorDamping   = juce::jmap(color, 0.0f, 1.0f, 0.996f, 1.0f);

    damping = sustainDamping * colorDamping;
    damping = juce::jlimit(0.90f, 0.99995f, damping);

    exciteString(velocityGain);
}

void StringVoice::stopNote(float, bool allowTailOff)
{
    if (letStringsRing)
    {
        // Guitar-style behavior:
        // ignore note-off and let the simulated string decay naturally.
        // Do NOT call clearCurrentNote() here.
        isReleasing = false;
        return;
    }

    if (allowTailOff)
    {
        isReleasing = true;
    }
    else
    {
        isReleasing = false;
        clearCurrentNote();
    }
}

void StringVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                  int startSample,
                                  int numSamples)
{
    if (! isVoiceActive())
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float out = getNextSample();

        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            outputBuffer.addSample(ch, startSample + sample, out);
    }
}

void StringVoice::exciteString(float velocityValue)
{
    // Temporary excitation, since we are not doing Revitar's pick model yet.
    // Shape the string into a small triangle-ish displacement.
    // TODO: Make this a Pick Position parameter.
    const int pluckIndex = juce::jlimit(1, numPoints - 2, numPoints / 4);
    // TODO: Make this a Pick Volume / Pick Strength parameter.
    const float amp = 0.1f * velocityValue; // Small amp value to prevent clicking

    for (int i = 1; i < numPoints - 1; ++i)
    {
        if (i <= pluckIndex)
            pos[i] = amp * (float(i) / float(pluckIndex));
        else
            pos[i] = amp * (float(numPoints - 1 - i) / float(numPoints - 1 - pluckIndex));
    }

    pos[0] = 0.0f;
    pos[numPoints - 1] = 0.0f;
}

void StringVoice::updateString()
{
    float acc = 0.0f;

    // This is the Revitar-ish spring-mass update.
    // Revitar unrolls this loop 4x for speed; keeping it simple here.
    for (int i = 0; i < numPoints - 1; ++i)
    {
        const float disX = pos[i] - pos[i + 1];

        vel[i] = (vel[i] + rate * (acc - disX)) * damping;
        pos[i] += rate * vel[i];

        acc = disX;
    }

    // Fixed string endpoints, same idea as Revitar.
    pos[0] = 0.0f;
    vel[0] = 0.0f;

    pos[numPoints - 1] = 0.0f;
    vel[numPoints - 1] = 0.0f;
}

float StringVoice::getNextSample()
{
    // During release, slowly increase damping.
    // This is okay for now, though later a separate releaseGain would be cleaner.
    if (isReleasing)
        damping *= 0.99995f;

    // High notes may run the string simulation multiple times per output sample.
    // This improves tuning/stability without needing unusably tiny strings.
    for (int i = 0; i < subSteps; ++i)
        updateString();

    // Simple temporary pickup: read the center point and a little of its neighbors.
    float out = pos[pickupIndex];

    if (pickupIndex > 0)
        out += 0.5f * pos[pickupIndex - 1];

    if (pickupIndex < numPoints - 1)
        out += 0.5f * pos[pickupIndex + 1];

    // Neighbor sum can be up to ~2x, so tame it.
    out *= outputGain;

    // Fade in to avoid a hard discontinuity when the pluck shape appears.
        if (fadeInCounter < fadeInSamples)
        {
            const float fade = (float)fadeInCounter / (float)juce::jmax(1, fadeInSamples);
            out *= fade;
            fadeInCounter++;
        }

        // Traditional release mode.
        if (!letStringsRing && isReleasing && damping < 0.95f)
            clearCurrentNote();

        // Natural ring-out mode.
        // Do NOT clear on one quiet sample because the waveform crosses zero constantly.
        // Instead, only clear after the string has stayed very quiet for a while.
        if (letStringsRing)
        {
            float energy = 0.0f;

            for (int i = 1; i < numPoints - 1; ++i)
                energy += std::abs(pos[i]) + std::abs(vel[i]);

            energy /= (float)juce::jmax(1, numPoints - 2);

            if (energy < 1.0e-5f)
                quietSamples++;
            else
                quietSamples = 0;

            if (quietSamples > (int)(0.25 * sr)) // quiet for 250 ms
                clearCurrentNote();
        }

        return out;
    }

