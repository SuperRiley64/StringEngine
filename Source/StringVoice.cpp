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
StringVoice::StringVoice(int openMidiNoteNumber)
    : openMidiNote(openMidiNoteNumber),
      currentMidiNote(-1)
{
}

void StringVoice::prepare(double sampleRate, int)
{
    sr = sampleRate;
    pos.fill(0.0f);
    vel.fill(0.0f);
    
    hasBeenInitialized = false;

    // Pre-tune this string as its open note immediately.
    initializeOpenStringIfNeeded();
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
// Pitch control
void StringVoice::setSlideTimeMs(float newSlideTimeMs)
{
    slideTimeMs = juce::jlimit(0.0f, 5000.0f, newSlideTimeMs);
}

float StringVoice::computeRateForMidiNote(int midiNote) const
{
    const float freq =
        juce::MidiMessage::getMidiNoteInHertz(midiNote);

    return juce::jlimit(
        0.01f,
        0.70f,
        (2.0f * freq * float(numPoints - 1))
        / (float(sr) * float(subSteps))
    );
}

void StringVoice::setVibratoDepthSemitones(float newDepth)
{
    vibratoDepthSemitones = juce::jlimit(-12.0f, 12.0f, newDepth);
}

void StringVoice::setVibratoRateHz(float newRate)
{
    vibratoRateHz = juce::jlimit(0.0f, 100.0f, newRate);
}

//==============================================================================
// Sympathetic Resonance

int StringVoice::getCurrentMidiNote() const
{
    return currentMidiNote >= 0 ? currentMidiNote : openMidiNote;
}

float StringVoice::getStringEnergy() const
{
    float energy = 0.0f;

    for (int i = 1; i < numPoints - 1; ++i)
        energy += std::abs(pos[(size_t)i]) + std::abs(vel[(size_t)i]);

    return energy / (float)juce::jmax(1, numPoints - 2);
}

void StringVoice::injectSympatheticEnergy(const StringVoice& sourceVoice, float amount)
{
    if (amount <= 0.001f)
        return;

    const float sourceEnergy = sourceVoice.getStringEnergy();

    if (sourceEnergy < 1.0e-6f)
        return;

    const int sourcePoint = juce::jlimit(2, sourceVoice.getCurrentNumPoints() - 3, 4);
    const int targetPoint = juce::jlimit(2, numPoints - 3, 4);

    const float sourceMotion =
        sourceVoice.pos[(size_t)sourcePoint]
        + 0.35f * sourceVoice.vel[(size_t)sourcePoint];

    const float targetMotion =
        pos[(size_t)targetPoint]
        + 0.35f * vel[(size_t)targetPoint];

    const float coupling =
        juce::jmap(std::pow(amount, 0.65f),
                   0.0f, 1.0f,
                   0.0f, 0.35f);

    const float injected =
        coupling * (sourceMotion - targetMotion);

    pos[(size_t)targetPoint] += injected;
    vel[(size_t)targetPoint] += injected * 0.45f;

    // Mirror a smaller amount near the other end.
    const int targetMirror = numPoints - 1 - targetPoint;

    pos[(size_t)targetMirror] += injected * 0.35f;
    vel[(size_t)targetMirror] += injected * 0.18f;

    const float couplingLoss =
        juce::jmap(amount, 0.0f, 1.0f, 0.99995f, 0.9988f);

    vel[(size_t)targetPoint] *= couplingLoss;
    vel[(size_t)targetMirror] *= couplingLoss;

    pos[(size_t)targetPoint] = juce::jlimit(-0.5f, 0.5f, pos[(size_t)targetPoint]);
    vel[(size_t)targetPoint] = juce::jlimit(-0.5f, 0.5f, vel[(size_t)targetPoint]);

    pos[(size_t)targetMirror] = juce::jlimit(-0.5f, 0.5f, pos[(size_t)targetMirror]);
    vel[(size_t)targetMirror] = juce::jlimit(-0.5f, 0.5f, vel[(size_t)targetMirror]);
}

void StringVoice::initializeOpenStringIfNeeded()
{
    if (hasBeenInitialized)
        return;

    const float freq = juce::MidiMessage::getMidiNoteInHertz(openMidiNote);

    const float maxStableRate = 0.70f;
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

    rate = (2.0f * freq * float(numPoints - 1))
           / (float(sr) * float(subSteps));

    rate = juce::jlimit(0.01f, maxStableRate, rate);
    
    // Slide
    currentRate = rate;
    targetRate = rate;
    slideSamplesRemaining = 0;
    totalSlideSamples = 0;

    pickupIndex = positionToPlayableIndex(pickupPosition, numPoints);

    pos.fill(0.0f);
    vel.fill(0.0f);

    currentMidiNote = -1;
    hasBeenInitialized = true;
}

//==============================================================================
// Parameters

void StringVoice::setColor(float newColor)                     { color = juce::jlimit(0.0f, 1.0f, newColor); }
void StringVoice::setPickupPosition(float newPosition)         { pickupPosition = juce::jlimit(0.0f, 1.0f, newPosition); }
void StringVoice::setPickPosition(float newPosition)           { pickPosition = juce::jlimit(0.0f, 1.0f, newPosition); }
void StringVoice::setPickWidth(float newWidth)                 { pickWidth = juce::jlimit(0.0f, 1.0f, newWidth); }
void StringVoice::setPickStrength(float newStrength)           { pickStrength = juce::jlimit(0.0f, 1.0f, newStrength); }
void StringVoice::setPickShape(float newPickShape)             { pickShape = juce::jlimit(0.0f, 1.0f, newPickShape); }
void StringVoice::setPickShapeCenter(float newPickShapeCenter) { pickShapeCenter = juce::jlimit(0.0f, 1.0f, newPickShapeCenter); }
void StringVoice::setPalmPosition(float newPalmPosition)       { palmPosition = juce::jlimit(0.0f, 1.0f, newPalmPosition); }
void StringVoice::setPalmDamping(float newPalmDamping)         { palmDamping = juce::jlimit(0.0f, 1.0f, newPalmDamping); }
void StringVoice::setBridgeDamping(float newBridgeDamping)     { bridgeDamping = juce::jlimit(0.0f, 1.0f, newBridgeDamping); }
void StringVoice::setStiffness(float newStiffness)             { stiffness = juce::jlimit(0.0f, 1.0f, newStiffness); }
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
    velocityGain = velocityValue;

    // Slide mode: always retune this string instead of stealing/fading it.
    if (slideTimeMs > 0.0f)
    {
        startNewStringNote(midiNoteNumber, velocityValue);
        return;
    }

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

    const bool wasAudible = getStringEnergy() > 1.0e-5f;

    const int previousMidiNote = getCurrentMidiNote();

    velocityGain = velocityValue;
    currentMidiNote = midiNoteNumber;
    isReleasing = false;

    // ===== Retune string geometry to the NEW note =============================
    const float freq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

    const float maxStableRate = 0.70f;
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

    const float finalRate =
        juce::jlimit(
            0.01f,
            maxStableRate,
            (2.0f * freq * float(numPoints - 1))
            / (float(sr) * float(subSteps))
        );

    // ===== Slide from previous pitch to new pitch =============================
    if (slideTimeMs > 0.0f && previousMidiNote >= 0)
    {
        const float previousFreq =
            juce::MidiMessage::getMidiNoteInHertz(previousMidiNote);

        const float pitchRatio =
            previousFreq / juce::jmax(1.0f, freq);

        slideStartRate = juce::jlimit(0.01f, maxStableRate, finalRate * pitchRatio);
        currentRate = slideStartRate;
        targetRate = finalRate;

        totalSlideSamples = juce::jmax(
            1,
            (int)std::round((slideTimeMs / 1000.0f) * sr)
        );

        slideSamplesRemaining = totalSlideSamples;
    }
    else
    {
        currentRate = finalRate;
        targetRate = finalRate;
        slideStartRate = finalRate;
        slideSamplesRemaining = 0;
        totalSlideSamples = 0;
    }

    rate = targetRate;

    if (!wasAudible)
    {
        pos.fill(0.0f);
        vel.fill(0.0f);
    }

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
    // Do not return early here.
    // Persistent strings may be idle but still receive sympathetic energy.

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
    // ===== Pick setup ==========================================================
    const float amp = juce::jmap(pickStrength, 0.0f, 1.0f, 0.01f, 0.14f) * velocityValue;
    const int pluckIndex = positionToPlayableIndex(pickPosition, numPoints);

    const int widthRadius = juce::jlimit(
        1,
        juce::jmax(1, numPoints / 3),
        (int)std::round(1.0f + pickWidth * float(numPoints) * 0.18f)
    );

    const float distanceFromMiddle = std::abs(pickPosition - 0.5f) * 2.0f;
    const float narrowPickAmount = 1.0f - pickWidth;

    // ===== Harmonic setup ======================================================
    const float positionBrightness = 0.45f * distanceFromMiddle;
    const float pickBrightness = 0.22f * narrowPickAmount;
    const float harmonicAmount = std::pow(harmonics, 0.55f);

    const float harmonicBlend =
        harmonicAmount * juce::jlimit(
            0.0f,
            0.65f,
            0.10f + positionBrightness + pickBrightness
        );

    // ===== Pick shape setup ====================================================
    const float shape = juce::jlimit(0.0f, 1.0f, pickShape);

    const float centerBias = juce::jmap(
        juce::jlimit(0.0f, 1.0f, pickShapeCenter),
        0.0f, 1.0f,
        -0.65f, 0.65f
    );

    // ===== String displacement =================================================
    for (int i = 1; i < numPoints - 1; ++i)
    {
        // ===== Full-string displacement shaped by pick contact =====================
        const float baseTriangle = (i <= pluckIndex)
            ? float(i) / float(pluckIndex)
            : float(numPoints - 1 - i) / float(numPoints - 1 - pluckIndex);

        // ===== Local pick coordinate ===============================================
        float localX = float(i - pluckIndex) / float(juce::jmax(1, widthRadius));
        localX = juce::jlimit(-1.0f, 1.0f, localX);

        // Center bias bends the pick contact peak without chopping off the sides.
        const float biasedX = juce::jlimit(
            -1.0f,
            1.0f,
            localX - centerBias * (1.0f - localX * localX)
        );

        const float absX = std::abs(biasedX);

        // ===== Contact shape models ================================================
        // Triangle = sharp point
        const float triangleShape =
            juce::jlimit(0.0f, 1.0f, 1.0f - absX);

        // Rounded = smoother fingertip-ish curve
        const float roundedShape =
            juce::jlimit(0.0f, 1.0f, std::sqrt(juce::jmax(0.0f, 1.0f - absX * absX)));

        // Square = flatter pick face with sharper edge
        const float squareShape =
            absX < 0.75f
                ? 1.0f
                : juce::jlimit(0.0f, 1.0f, (1.0f - absX) / 0.25f);

        // Morph triangle -> rounded -> square.
        float contactShape = 0.0f;

        if (shape < 0.5f)
        {
            const float t = shape * 2.0f;
            contactShape = triangleShape * (1.0f - t) + roundedShape * t;
        }
        else
        {
            const float t = (shape - 0.5f) * 2.0f;
            contactShape = roundedShape * (1.0f - t) + squareShape * t;
        }

        // ===== Apply pick shape as actual displacement processing ===================
        // This is the important change:
        // pick shape now changes the full pluck curve instead of being a quiet overlay.
        //
        // Triangle shape keeps the original kink.
        // Rounded shape softens the kink.
        // Square shape flattens the contact area harder.
        const float shapePower = juce::jmap(shape, 0.0f, 1.0f, 1.25f, 0.20f);
        const float processedTriangle = std::pow(baseTriangle, shapePower);

        // Local shaped contact replaces the string curve near the pick.
        const float contactAmount =
            juce::jmap(std::pow(pickWidth, 0.35f), 0.0f, 1.0f, 0.55f, 1.0f);

        const float shaped =
            processedTriangle * (1.0f - contactAmount)
            + (processedTriangle * contactShape) * contactAmount;

        // ===== Pick harmonics ==================================================
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

        // ===== Pick noise =======================================================
        const float rawNoise =
            juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;

        const float noiseGain =
            juce::jmap(std::pow(pickNoiseAmount, 0.7f),
                       0.0f, 1.0f,
                       0.0f, 0.06f);

        const float pickNoise =
            rawNoise * noiseGain * velocityValue * contactShape;

        // ===== Apply excitation =================================================
        pos[i] = amp * harmonicShape + pickNoise;
    }

    // ===== Fixed endpoints =====================================================
    pos[0] = 0.0f;
    pos[numPoints - 1] = 0.0f;
}

//==============================================================================
// String simulation

void StringVoice::updateString()
{
    // ===== Stable-ish stiff string update =======================================
    // This uses a two-buffer update so every force is calculated from the old
    // string state. That avoids the "strings going backwards through space" thing
    // caused by mutating pos[] while also reading from it.

    nextPos = pos;
    nextVel = vel;

    // Stiffness must be scaled way down for short/high-note strings.
    // Otherwise the 4th derivative gets enormous.
    const float stiffnessAmount =
        juce::jmap(std::pow(stiffness, 0.7f),
                   0.0f, 1.0f,
                   0.0f, 0.25f);

    // Compute rate for the slide
    if (slideSamplesRemaining > 0)
    {
        const float t =
            1.0f - ((float)slideSamplesRemaining / (float)totalSlideSamples);

        const float smoothT = t * t * (3.0f - 2.0f * t);

        currentRate = slideStartRate + (targetRate - slideStartRate) * smoothT;

        --slideSamplesRemaining;
    }
    else
    {
        currentRate = targetRate;
    }
    
    float pitchRate = currentRate;

    if (vibratoRateHz > 0.001f && std::abs(vibratoDepthSemitones) > 0.001f)
    {
        vibratoPhase += juce::MathConstants<float>::twoPi
                        * vibratoRateHz
                        / (float)sr;

        if (vibratoPhase >= juce::MathConstants<float>::twoPi)
            vibratoPhase -= juce::MathConstants<float>::twoPi;

        const float vibratoSemitones =
            std::sin(vibratoPhase) * vibratoDepthSemitones;

        const float vibratoRatio =
            std::pow(2.0f, vibratoSemitones / 12.0f);

        pitchRate *= vibratoRatio;

        // Keep the physical solver stable.
        pitchRate = juce::jlimit(0.01f, 0.70f, pitchRate);
    }
    
    // Extra stability damping only for stiffness energy.
    // This prevents the 4th-derivative term from turning into a trampoline.
    //const float stiffnessDamping =
    //    juce::jmap(stiffness, 0.0f, 1.0f, 1.0f, 0.995f);

    for (int i = 1; i < numPoints - 1; ++i)
    {
        // Normal flexible-string tension.
        const float tensionForce =
            pos[i - 1] - 2.0f * pos[i] + pos[i + 1];

        float force = tensionForce;

        // Stiff-string bending resistance.
        if (i > 1 && i < numPoints - 2)
        {
            const float bendForce =
                  pos[i - 2]
                - 4.0f * pos[i - 1]
                + 6.0f * pos[i]
                - 4.0f * pos[i + 1]
                + pos[i + 2];

            force -= stiffnessAmount * bendForce;
        }

        nextVel[i] = (vel[i] + pitchRate * force) * damping;
        nextPos[i] = pos[i] + pitchRate * nextVel[i];
    }

    pos = nextPos;
    vel = nextVel;

    // Fixed endpoints.
    pos[0] = 0.0f;
    vel[0] = 0.0f;

    pos[numPoints - 1] = 0.0f;
    vel[numPoints - 1] = 0.0f;

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
    {
        currentMidiNote = -1;
        clearCurrentNote();
    }

    if (letStringsRing)
    {
        float energy = 0.0f;

        for (int i = 1; i < numPoints - 1; ++i)
            energy += std::abs(pos[i]) + std::abs(vel[i]);

        energy /= (float)juce::jmax(1, numPoints - 2);

        quietSamples = (energy < 1.0e-5f) ? quietSamples + 1 : 0;

        if (quietSamples > (int)(0.25 * sr))
        {
            currentMidiNote = -1;
            clearCurrentNote();
        }
    }

    return std::tanh(out * 1.2f);
}
