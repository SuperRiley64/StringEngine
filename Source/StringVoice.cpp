/*
  ==============================================================================

    StringVoice.cpp
    Created: 25 Apr 2026 9:14:26am
    Author:  Riley Knybel

  ==============================================================================
*/

#include "StringVoice.h"

namespace
{
    int positionToPlayableIndex(float normalizedPosition, int numPoints)
    {
        // 0.0 = first playable point after silent nut
        // 1.0 = last playable point before silent bridge
        const float p = juce::jlimit(0.0f, 1.0f, normalizedPosition);

        return juce::jlimit(
            1,
            numPoints - 2,
            1 + (int)std::round(p * float(numPoints - 3))
        );
    }
}

StringVoice::StringVoice()
{
}

void StringVoice::prepare(double sampleRate, int)
{
    sr = sampleRate;

    pos.fill(0.0f);
    vel.fill(0.0f);
}
// Visualizer
void StringVoice::copyStringState(std::vector<float>& destination) const
{
    destination.clear();
    destination.reserve(numPoints);

    for (int i = 0; i < numPoints; ++i)
        destination.push_back(pos[i]);
}

int StringVoice::getCurrentNumPoints() const
{
    return numPoints;
}

// Parameters

void StringVoice::setDecay(float newDecay)
{
    decay = juce::jlimit(0.0f, 1.0f, newDecay);
}

void StringVoice::setColor(float newColor)
{
    color = juce::jlimit(0.0f, 1.0f, newColor);
}

void StringVoice::setPickupPosition(float newPickupPosition)
{
    pickupPosition = juce::jlimit(0.0f, 1.0f, newPickupPosition);
}

void StringVoice::setLetStringsRing(bool shouldRing)
{
    letStringsRing = shouldRing;
}

void StringVoice::setPickPosition(float newPickPosition)
{
    pickPosition = juce::jlimit(0.0f, 1.0f, newPickPosition);
}

void StringVoice::setPickWidth(float newPickWidth)
{
    pickWidth = juce::jlimit(0.0f, 1.0f, newPickWidth);
}

void StringVoice::setPickStrength(float newPickStrength)
{
    pickStrength = juce::jlimit(0.0f, 1.0f, newPickStrength);
}

void StringVoice::startNote(int midiNoteNumber, float velocityValue,
                            juce::SynthesiserSound*, int)
{
    // Only do the anti-click deferred replacement if this voice is being stolen
    // while it is still actively ringing.
    //
    // If the voice is already in release mode, start the new note immediately.
    // Otherwise rapid polyphonic playing can feel like notes randomly disappear.
    if (isVoiceActive() && !isReleasing)
    {
        float energy = 0.0f;

        for (int i = 1; i < numPoints - 1; ++i)
            energy += std::abs(pos[i]) + std::abs(vel[i]);

        energy /= (float)juce::jmax(1, numPoints - 2);

        if (energy > 1.0e-5f)
        {
            pendingNewNote = true;
            pendingMidiNote = midiNoteNumber;
            pendingVelocity = velocityValue;

            stealFadeSamples = juce::jmax(1, (int)(0.003 * sr)); // 3 ms
            stealFadeCounter = 0;
            return;
        }
    }

    startNewStringNote(midiNoteNumber, velocityValue);
}

void StringVoice::startNewStringNote(int midiNoteNumber, float velocityValue)
{
    pendingNewNote = false;
    stealFadeCounter = 0;
    
    fadeInSamples = juce::jmax(1, (int)(0.003 * sr)); // 3 ms fade-in to reduce note-on clicks
    fadeInCounter = 0;
    quietSamples = 0;
    colorFilterState = 0.0f;

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

    // Pickup position is normalized 0-1.
    // Keep it away from the exact endpoints because fixed endpoints are silent.
    pickupIndex = positionToPlayableIndex(pickupPosition, numPoints);

    // Corrected pitch rate.
    // This is the important fix.
    rate = (2.0f * freq * float(numPoints - 1))
           / (float(sr) * float(subSteps));

    rate = juce::jlimit(0.01f, maxStableRate, rate);

    // Decay controls broad sustain only.

    // Color is handled as a tone filter in getNextSample().

    const float sustainDamping = juce::jmap(decay, 0.0f, 1.0f, 0.999f, 0.999999f);

    // String-dependent damping:
    // lower notes / thicker strings ring longer,
    // higher notes / thinner strings decay faster.
    // MIDI 40 = low E on guitar.
    // MIDI 76 = high E around the 12th fret.
    const float noteNorm = juce::jlimit(
        0.0f,
        1.0f,
        (float(midiNoteNumber) - 40.0f) / (76.0f - 40.0f)
    );

    // At high notes, subtract a little more damping.

    // Keep this tiny because damping values near 1.0 are extremely sensitive.

    const float stringLoss = juce::jmap(noteNorm, 0.0f, 1.0f, 0.0f, 0.00045f);

    damping = sustainDamping - stringLoss;

    damping = juce::jlimit(0.90f, 0.99999f, damping);

    exciteString(velocityGain);
}

void StringVoice::stopNote(float, bool allowTailOff)
{
    if (letStringsRing)
    {
        isReleasing = false;
        return;
    }
    // Avoid instant hard clear when possible.
    isReleasing = true;
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
    // TODO: Make this a Pick Position parameter.
    // 0.0 = near nut
    // 1.0 = near bridge
    // Lower values are rounder/hollower, higher values are brighter/twangier.

    // TODO: Make this a Pick Width parameter.
    // 0.0 = very narrow/hard pick
    // 1.0 = wider/softer finger-like excitation

    // TODO: Make this a Pick Volume / Pick Strength parameter.
    const float amp = pickStrength * velocityValue;

    const int pluckIndex = positionToPlayableIndex(pickPosition, numPoints);

    // Convert normalized pick width to a radius in string points.
    // Keep at least 1 so the smoothing math always has an actual region.
    const int widthRadius = juce::jlimit(
        1,
        juce::jmax(1, numPoints / 3),
        (int)std::round(1.0f + pickWidth * float(numPoints) * 0.15f)
    );

    for (int i = 1; i < numPoints - 1; ++i)
    {
        // Base triangular displacement, same as before.
        float triangle = 0.0f;

        if (i <= pluckIndex)
            triangle = float(i) / float(pluckIndex);
        else
            triangle = float(numPoints - 1 - i) / float(numPoints - 1 - pluckIndex);

        // Width envelope around the pick point.
        const int distanceFromPick = std::abs(i - pluckIndex);
        const float widthEnvelope = juce::jlimit(
            0.0f,
            1.0f,
            1.0f - (float(distanceFromPick) / float(widthRadius))
        );

        const float shaped = triangle * juce::jmap(pickWidth, 0.0f, 1.0f, 1.0f, 0.35f)
                           + widthEnvelope * juce::jmap(pickWidth, 0.0f, 1.0f, 0.0f, 0.65f);

        // Filtered pick noise transient.
        // TODO: Make this a Pick Noise parameter.
        // Small amount only — this is the scrape/click of the pick, not the body of the note.
        const float rawNoise = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;

        // Simple spatial filtering: strongest near the pick, softened by pick width.
        const float noiseAmount = 0.006f * velocityValue;
        const float pickNoise = rawNoise * noiseAmount * widthEnvelope;

        pos[i] = amp * shaped + pickNoise;
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
    
    // Subtle dispersion / phase smear.
    // TODO: Make this a Dispersion parameter.
    // Keep this extremely restrained. Too much turns metallic / drum-like.
    // This gently shares a tiny amount of displacement with neighboring points
    // after the main spring update, which adds a little string complexity without
    // the aggressive steel-drum behavior from the stiffness term.
    const float dispersion = 0.0015f;

    for (int i = 2; i < numPoints - 2; ++i)
    {
        const float neighborAverage = 0.5f * (pos[i - 1] + pos[i + 1]);
        pos[i] += dispersion * (neighborAverage - pos[i]);
    }
    
    // Bridge damping / termination losses.
    // TODO: Make this a Bridge Damping parameter.
    // Real guitar endpoints are not perfectly lossless. The bridge/nut leak energy,
    // especially from the points closest to the ends of the string.
    const float bridgeDamping = 0.9975f;

    if (numPoints > 6)
    {
        // Lightly damp the playable points closest to the fixed endpoints.
        // This keeps the string from feeling too mathematically perfect.
        vel[1] *= bridgeDamping;
        vel[numPoints - 2] *= bridgeDamping;

        // Slightly gentler loss one point farther in.
        vel[2] *= 0.999f;
        vel[numPoints - 3] *= 0.999f;
    }
}

float StringVoice::getNextSample()
{
    // During release, slowly increase damping.
    // This is okay for now, though later a separate releaseGain would be cleaner.
    if (isReleasing)
        damping *= 0.99995f;
    
    if (pendingNewNote)
    {
        const float fade = 1.0f - ((float)stealFadeCounter / (float)stealFadeSamples);

        for (int i = 0; i < numPoints; ++i)
        {
            pos[i] *= fade;
            vel[i] *= fade;
        }

        stealFadeCounter++;

        if (stealFadeCounter >= stealFadeSamples)
        {
            pendingNewNote = false;
            pos.fill(0.0f);
            vel.fill(0.0f);

            startNewStringNote(pendingMidiNote, pendingVelocity);
        }
    }

    // High notes may run the string simulation multiple times per output sample.
    // This improves tuning/stability without needing unusably tiny strings.
    for (int i = 0; i < subSteps; ++i)
        updateString();

    // Simple temporary pickup: read the center point and a little of its neighbors.
    // Map normalized pickup position to a string point every sample.
    // This lets the pickup position parameter move while the note is ringing.
    pickupIndex = positionToPlayableIndex(pickupPosition, numPoints);

    // Read the pickup point plus neighbors.
    float out = pos[pickupIndex];

    out += 0.5f * pos[pickupIndex - 1];
    out += 0.5f * pos[pickupIndex + 1];

    // Neighbor sum can be up to ~2x, so tame it.
    out *= outputGain;
    
    // Color is a simple one-pole low-pass tone control.
    // 0.0 = darker / more filtered
    // 1.0 = brighter / more direct
    const float toneAmount = juce::jmap(color, 0.0f, 1.0f, 0.04f, 0.95f);

    colorFilterState += toneAmount * (out - colorFilterState);
    out = colorFilterState;

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
