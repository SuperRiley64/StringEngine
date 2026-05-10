/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GuitarSynthAudioProcessor::GuitarSynthAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
#else
     : apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
}

GuitarSynthAudioProcessor::~GuitarSynthAudioProcessor()
{
}

// Visualizer
void GuitarSynthAudioProcessor::copyActiveStringStates(std::array<std::vector<float>, 6>& destinations) const
{
    for (auto& dest : destinations)
        dest.clear();

    int stringIndex = 0;

    for (int i = 0; i < synth.getNumVoices() && stringIndex < 6; ++i)
    {
        if (auto* voice = synth.getVoice(i))
        {
            if (voice->isVoiceActive())
            {
                voice->copyStringState(destinations[(size_t)stringIndex]);
                ++stringIndex;
            }
        }
    }
}

// ======PARAMETERS=======
juce::AudioProcessorValueTreeState::ParameterLayout GuitarSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Body
    params.push_back(std::make_unique<juce::AudioParameterFloat>("bodyMix",     "Body Mix",     0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("bodySize",    "Body Size",    0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("bodyDamping", "Body Damping", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sympathetic", "Sympathetic", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("strum", "Strum", -1.0f, 1.0f, 0.0f));
    
    // Fretboard sliders
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickupPosition", "Pickup Position", 0.0f, 1.0f, 0.85f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickPosition",   "Pick Position",   0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("palmPosition",   "Palm Position",   0.0f, 1.0f, 0.88f));

    // Picking
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickStrength",   "Pick Strength",   0.0f, 1.0f, 0.10f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickWidth",      "Pick Width",      0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("harmonics",      "Harmonics",       0.0f, 1.0f, 0.18f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickNoise",      "Pick Noise",      0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickShape",      "Pick Shape",      0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickShapeCenter", "Pick Center",      0.0f, 1.0f, 0.5f));

    // Damping
    params.push_back(std::make_unique<juce::AudioParameterFloat>("color",          "Tone",            0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("bridgeDamping",  "Bridge Damping",  0.0f, 1.0f, 0.05f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("palmDamping",    "Palm Damping",    0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("letStringsRing",  "Let Strings Ring", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("stiffness",     "Stiffness",      0.0f, 1.0f, 0.05f));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String GuitarSynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool GuitarSynthAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool GuitarSynthAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool GuitarSynthAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double GuitarSynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int GuitarSynthAudioProcessor::getNumPrograms()
{
    return 1;
}

int GuitarSynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void GuitarSynthAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String GuitarSynthAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void GuitarSynthAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void GuitarSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    // String voices
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<StringVoice*> (synth.getVoice(i)))
            voice->prepare (sampleRate, samplesPerBlock);
    }
    
    // Body resonator
    for (int i = 0; i < bodyModeCount; ++i)
    {
        bodyFiltersL[(size_t)i].reset();
        bodyFiltersR[(size_t)i].reset();
    }

    updateBodyResonatorParameters(0.0f, 0.5f, 0.5f);
}

void GuitarSynthAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GuitarSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void GuitarSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    auto pickupPosition = apvts.getRawParameterValue("pickupPosition");
    auto pickPosition   = apvts.getRawParameterValue("pickPosition");
    auto palmPosition   = apvts.getRawParameterValue("palmPosition");

    auto pickStrength   = apvts.getRawParameterValue("pickStrength");
    auto pickWidth      = apvts.getRawParameterValue("pickWidth");
    auto harmonics      = apvts.getRawParameterValue("harmonics");
    auto pickNoise      = apvts.getRawParameterValue("pickNoise");
    
    auto pickShape      = apvts.getRawParameterValue("pickShape");
    auto pickCenter      = apvts.getRawParameterValue("pickShapeCenter");

    auto color          = apvts.getRawParameterValue("color");
    auto bridgeDamping  = apvts.getRawParameterValue("bridgeDamping");
    auto palmDamping    = apvts.getRawParameterValue("palmDamping");
    auto letStringsRing = apvts.getRawParameterValue("letStringsRing");

    auto stiffness     = apvts.getRawParameterValue("stiffness");

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = synth.getVoice(i))
        {
            voice->setPickupPosition(pickupPosition->load());
            voice->setPickPosition(pickPosition->load());
            voice->setPalmPosition(palmPosition->load());

            voice->setPickStrength(pickStrength->load());
            voice->setPickWidth(pickWidth->load());
            voice->setHarmonics(harmonics->load());
            voice->setPickNoiseAmount(pickNoise->load());
            
            voice->setPickShape(pickShape->load());
            voice->setPickShapeCenter(pickCenter->load());

            voice->setColor(color->load());
            voice->setBridgeDamping(bridgeDamping->load());
            voice->setPalmDamping(palmDamping->load());
            voice->setLetStringsRing(letStringsRing->load() >= 0.5f);

            voice->setStiffness(stiffness->load());
        }
    }
    
    auto bodyMix     = apvts.getRawParameterValue("bodyMix");
    auto bodySize    = apvts.getRawParameterValue("bodySize");
    auto bodyDamping = apvts.getRawParameterValue("bodyDamping");
    
    updateBodyResonatorParameters(
        bodyMix->load(),
        bodySize->load(),
        bodyDamping->load()
    );
    
    auto sympathetic = apvts.getRawParameterValue("sympathetic");
    auto strum = apvts.getRawParameterValue("strum");
    synth.setSympatheticAmount(sympathetic->load() * 0.1); // Scale the knob for sym. DSP
    synth.setStrumAmount(strum->load());
    
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    processBodyResonator(buffer);
}

//===== Body resonator functions ===============================================
void GuitarSynthAudioProcessor::updateBodyResonatorParameters(float bodyMix,
                                                              float bodySize,
                                                              float bodyDamping)
{
    currentBodyMix = juce::jlimit(0.0f, 1.0f, bodyMix);
    currentBodySize = juce::jlimit(0.0f, 1.0f, bodySize);
    currentBodyDamping = juce::jlimit(0.0f, 1.0f, bodyDamping);

    if (currentBodyMix <= 0.001f)
        return;

    const double sampleRate = getSampleRate();

    if (sampleRate <= 0.0)
        return;

    // Smaller body = higher resonances.
    // Larger body = lower resonances.
    const float sizeScale =
        juce::jmap(currentBodySize, 0.0f, 1.0f, 1.45f, 0.75f);

    // Lower damping value = ringier / higher Q.
    // Higher damping value = tighter / lower Q.
    const float q =
        juce::jmap(currentBodyDamping, 0.0f, 1.0f, 12.0f, 2.0f);

    for (int i = 0; i < bodyModeCount; ++i)
    {
        const float freq = juce::jlimit(
            40.0f,
            6000.0f,
            bodyBaseFrequencies[(size_t)i] * sizeScale
        );

        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(
            sampleRate,
            freq,
            q
        );

        bodyFiltersL[(size_t)i].coefficients = coeffs;
        bodyFiltersR[(size_t)i].coefficients = coeffs;
    }
}

void GuitarSynthAudioProcessor::processBodyResonator(juce::AudioBuffer<float>& buffer)
{
    if (currentBodyMix <= 0.001f)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels <= 0)
        return;

    const float wetGain = juce::jmap(currentBodyMix, 0.0f, 1.0f, 0.0f, 2.5f);
    const float dryGain = 1.0f - (currentBodyMix * 0.25f);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryL = buffer.getSample(0, sample);
        const float dryR = numChannels > 1 ? buffer.getSample(1, sample) : dryL;

        float bodyL = 0.0f;
        float bodyR = 0.0f;

        for (int mode = 0; mode < bodyModeCount; ++mode)
        {
            const float gain = bodyModeGains[(size_t)mode];

            bodyL += gain * 2.0f * bodyFiltersL[(size_t)mode].processSample(dryL);
            bodyR += gain * 2.0f * bodyFiltersR[(size_t)mode].processSample(dryR);
        }

        const float outL = dryL * dryGain + bodyL * wetGain;
        const float outR = dryR * dryGain + bodyR * wetGain;

        buffer.setSample(0, sample, std::tanh(outL * 1.1f));

        if (numChannels > 1)
            buffer.setSample(1, sample, std::tanh(outR * 1.1f));
    }
}
//==============================================================================
bool GuitarSynthAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* GuitarSynthAudioProcessor::createEditor()
{
    return new GuitarSynthAudioProcessorEditor (*this);
}

//==============================================================================
void GuitarSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void GuitarSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarSynthAudioProcessor();
}
