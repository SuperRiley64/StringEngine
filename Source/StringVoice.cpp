#include "StringVoice.h"

namespace
{
    int positionToPlayableIndex(float normalizedPosition, int numPoints)
    {
        const float p = juce::jlimit(0.0f, 1.0f, normalizedPosition);

        return juce::jlimit(
            1,
            numPoints - 2,
            1 + (int)std::round(p * float(numPoints - 3))
        );
    }
}

//==============================================================================
StringVoice::StringVoice() {}

void StringVoice::prepare(double sampleRate, int)
{
    sr = sampleRate;
    pos.fill(0.0f);
    vel.fill(0.0f);
}

//==============================================================================
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

//==============================================================================
// Parameters

void StringVoice::setColor(float newColor)                     { color = juce::jlimit(0.0f, 1.0f, newColor); }
void StringVoice::setPickupPosition(float newPosition)         { pickupPosition = juce::jlimit(0.0f, 1.0f, newPosition); }
void StringVoice::setPickPosition(float newPosition)           { pickPosition = juce::jlimit(0.0f, 1.0f, newPosition); }
void StringVoice::setPickWidth(float newWidth)                 { pickWidth = juce::jlimit(0.0f, 1.0f, newWidth); }
void StringVoice::setPickStrength(float newStrength)           { pickStrength = juce::jlimit(0.0f, 1.0f, newStrength); }
void StringVoice::setPalmPosition(float newPalmPosition)       { palmPosition = juce::jlimit(0.0f, 1.0f, newPalmPosition); }
void StringVoice::setPalmDamping(float newPalmDamping)         { palmDamping = juce::jlimit(0.0f, 1.0f, newPalmDamping); }
void StringVoice::setBridgeDamping(float newBridgeDamping)     { bridgeDamping = juce::jlimit(0.0f, 1.0f, newBridgeDamping); }
void StringVoice::setDispersion(float newDispersion)           { dispersion = juce::jlimit(0.0f, 1.0f, newDispersion); }
void StringVoice::setHarmonics(float newHarmonics)             { harmonics = juce::jlimit(0.0f, 1.0f, newHarmonics); }
void StringVoice::setPickNoiseAmount(float newPickNoiseAmount) { pickNoiseAmount = juce::jlimit(0.0f, 1.0f, newPickNoiseAmount); }
void StringVoice::setLetStringsRing(bool shouldRing)
{
    letStringsRing = shouldRing;
    if (letStringsRing)
    {
        isReleasing = false;
        releaseGain = 1.0f;
    }
}

//==============================================================================
// Note lifecycle

void StringVoice::startNote(int midiNoteNumber, float velocityValue,
                            juce::SynthesiserSound*, int)
{
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
            stealFadeSamples = juce::jmax(1, (int)(0.003 * sr));
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
    releaseGain = 1.0f;

    fadeInSamples = juce::jmax(1, (int)(0.003 * sr));
    fadeInCounter = 0;
    quietSamples = 0;
    colorFilterState = 0.0f;

    pos.fill(0.0f);
    vel.fill(0.0f);

    velocityGain = velocityValue;
    isReleasing = false;

    const float freq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

    // TODO: Make this a Stability / Quality parameter.
    const float maxStableRate = 0.70f;

    // TODO: Make this a Quality parameter.
    const int minUsefulPoints = 16;

    subSteps = 1;

    while ((2.0f * freq * float(minUsefulPoints - 1))
           / (float(sr) * float(subSteps)) > maxStableRate)
    {
        ++subSteps;
    }

    const int maxAllowedPoints =
        1 + (int)std::floor((maxStableRate * float(sr) * float(subSteps))
                            / (2.0f * freq));

    numPoints = juce::jlimit(minUsefulPoints, maxPoints, maxAllowedPoints);
    pickupIndex = positionToPlayableIndex(pickupPosition, numPoints);

    rate = (2.0f * freq * float(numPoints - 1))
           / (float(sr) * float(subSteps));

    rate = juce::jlimit(0.01f, maxStableRate, rate);

    exciteString(velocityGain);
}

void StringVoice::stopNote(float, bool)
{
    if (letStringsRing)
    {
        isReleasing = false;
        return;
    }

    isReleasing = true;
}

//==============================================================================
// Rendering

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

//==============================================================================
// Excitation

void StringVoice::exciteString(float velocityValue)
{
    const float amp = juce::jmap(pickStrength, 0.0f, 1.0f, 0.01f, 0.14f) * velocityValue;
    const int pluckIndex = positionToPlayableIndex(pickPosition, numPoints);

    const int widthRadius = juce::jlimit(
        1,
        juce::jmax(1, numPoints / 3),
        (int)std::round(1.0f + pickWidth * float(numPoints) * 0.18f)
    );

    const float distanceFromMiddle = std::abs(pickPosition - 0.5f) * 2.0f;
    const float narrowPickAmount = 1.0f - pickWidth;

    // Harmonic enrichment.
    const float positionBrightness = 0.45f * distanceFromMiddle;
    const float pickBrightness = 0.22f * narrowPickAmount;
    const float harmonicAmount = std::pow(harmonics, 0.55f);

    const float harmonicBlend =
        harmonicAmount * juce::jlimit(
            0.0f,
            0.65f,
            0.10f + positionBrightness + pickBrightness
        );

    for (int i = 1; i < numPoints - 1; ++i)
    {
        const float triangle = (i <= pluckIndex)
            ? float(i) / float(pluckIndex)
            : float(numPoints - 1 - i) / float(numPoints - 1 - pluckIndex);

        const int distanceFromPick = std::abs(i - pluckIndex);
        const float widthEnvelope = juce::jlimit(
            0.0f,
            1.0f,
            1.0f - (float(distanceFromPick) / float(widthRadius))
        );

        const float shaped =
            triangle * juce::jmap(pickWidth, 0.0f, 1.0f, 1.0f, 0.30f)
            + widthEnvelope * juce::jmap(pickWidth, 0.0f, 1.0f, 0.0f, 0.70f);

        const float x = float(i) / float(numPoints - 1);

        const float mode2 = std::sin(2.0f * juce::MathConstants<float>::pi * x);
        const float mode3 = std::sin(3.0f * juce::MathConstants<float>::pi * x);
        const float mode4 = std::sin(4.0f * juce::MathConstants<float>::pi * x);
        const float mode5 = std::sin(5.0f * juce::MathConstants<float>::pi * x);

        const float harmonicShape =
            shaped * (1.0f - 0.35f * harmonicBlend)
            + harmonicBlend * (
                  0.40f * mode2
                + 0.30f * mode3
                + (0.20f + 0.25f * distanceFromMiddle) * mode4
                + (0.10f + 0.20f * distanceFromMiddle) * mode5
              );

        const float rawNoise = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;

        // Pick noise gets audible range, but stays transient/local to pick.
        const float noiseGain =
            juce::jmap(std::pow(pickNoiseAmount, 0.7f), 0.0f, 1.0f, 0.0f, 0.06f);

        const float pickNoise = rawNoise * noiseGain * velocityValue * widthEnvelope;

        pos[i] = amp * harmonicShape + pickNoise;
    }

    pos[0] = 0.0f;
    pos[numPoints - 1] = 0.0f;
}

//==============================================================================
// String simulation

void StringVoice::updateString()
{
    float acc = 0.0f;

    for (int i = 0; i < numPoints - 1; ++i)
    {
        const float disX = pos[i] - pos[i + 1];

        vel[i] = (vel[i] + rate * (acc - disX)) * damping;
        pos[i] += rate * vel[i];

        acc = disX;
    }

    pos[0] = 0.0f;
    vel[0] = 0.0f;

    pos[numPoints - 1] = 0.0f;
    vel[numPoints - 1] = 0.0f;

    // Jangle / pseudo-dispersion model.
    // This adds a tiny amount of high-frequency motion instead of smoothing it away.
    // Keep subtle; too much gets unstable fast.
    const float jangleAmount =
        juce::jmap(std::pow(dispersion, 0.7f), 0.0f, 1.0f, 0.0f, 0.02f);

    for (int i = 2; i < numPoints - 2; ++i)
    {
        const float highMode =
            pos[i - 1] - 2.0f * pos[i] + pos[i + 1];

        vel[i] -= jangleAmount * highMode;
    }

    // Palm muting model.
    if (palmDamping > 0.001f)
    {
        const int palmIndex = positionToPlayableIndex(palmPosition, numPoints);

        const int palmWidth =
            juce::jlimit(1, juce::jmax(2, numPoints / 8), numPoints / 10);

        const float maxPalmLoss =
            juce::jmap(std::pow(palmDamping, 0.7f), 0.0f, 1.0f, 0.0f, 0.012f);

        for (int i = juce::jmax(1, palmIndex - palmWidth);
             i <= juce::jmin(numPoints - 2, palmIndex + palmWidth);
             ++i)
        {
            const float dist = std::abs(float(i - palmIndex)) / float(palmWidth);
            const float envelope = 1.0f - dist;

            vel[i] *= (1.0f - maxPalmLoss * envelope);
        }
    }

    // Bridge/nut damping model.
    const float bridgeLoss =
        juce::jmap(std::pow(bridgeDamping, 0.75f), 0.0f, 1.0f, 1.0f, 0.9f);

    if (numPoints > 6)
    {
        vel[1] *= bridgeLoss;
        vel[numPoints - 2] *= bridgeLoss;

        vel[2] *= juce::jmap(bridgeDamping, 0.0f, 1.0f, 0.99998f, 0.9985f);
        vel[numPoints - 3] *= juce::jmap(bridgeDamping, 0.0f, 1.0f, 0.99998f, 0.9985f);
    }

    // Safety clamp for runaway physical-model energy.
    for (int i = 1; i < numPoints - 1; ++i)
    {
        pos[i] = juce::jlimit(-2.0f, 2.0f, pos[i]);
        vel[i] = juce::jlimit(-2.0f, 2.0f, vel[i]);
    }
}

float StringVoice::getNextSample()
{
    if (pendingNewNote)
    {
        const float fade = 1.0f - ((float)stealFadeCounter / (float)stealFadeSamples);

        for (int i = 0; i < numPoints; ++i)
        {
            pos[i] *= fade;
            vel[i] *= fade;
        }

        if (++stealFadeCounter >= stealFadeSamples)
        {
            pendingNewNote = false;
            pos.fill(0.0f);
            vel.fill(0.0f);
            startNewStringNote(pendingMidiNote, pendingVelocity);
        }
    }

    for (int i = 0; i < subSteps; ++i)
        updateString();

    pickupIndex = positionToPlayableIndex(pickupPosition, numPoints);

    float out = pos[pickupIndex];
    out += 0.5f * pos[pickupIndex - 1];
    out += 0.5f * pos[pickupIndex + 1];
    out *= outputGain;

    const float toneAmount = juce::jmap(std::pow(color, 0.8f), 0.0f, 1.0f, 0.04f, 0.98f);
    colorFilterState += toneAmount * (out - colorFilterState);
    out = colorFilterState;
    
    // Release fade. Do not mutate damping here;
    // damping is the physical string loss, releaseGain is note-off gain.
    if (isReleasing)
    {
        releaseGain *= 0.9995f;
        out *= releaseGain;
    }

    if (fadeInCounter < fadeInSamples)
    {
        const float fade = (float)fadeInCounter / (float)juce::jmax(1, fadeInSamples);
        out *= fade;
        ++fadeInCounter;
    }

    if (!letStringsRing && isReleasing && releaseGain < 0.001f)
        clearCurrentNote();

    if (letStringsRing)
    {
        float energy = 0.0f;

        for (int i = 1; i < numPoints - 1; ++i)
            energy += std::abs(pos[i]) + std::abs(vel[i]);

        energy /= (float)juce::jmax(1, numPoints - 2);

        quietSamples = (energy < 1.0e-5f) ? quietSamples + 1 : 0;

        if (quietSamples > (int)(0.25 * sr))
            clearCurrentNote();
    }

    return std::tanh(out * 1.2f);
}
