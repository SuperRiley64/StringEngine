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
    for (int i = 0; i < 6; ++i)
        synth.addVoice (new StringVoice());

    synth.addSound (new StringSound());
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
        if (auto* voice = dynamic_cast<StringVoice*>(synth.getVoice(i)))
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

    // Fretboard sliders
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickupPosition", "Pickup Position", 0.0f, 1.0f, 0.85f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickPosition",   "Pick Position",   0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("palmPosition",   "Palm Position",   0.0f, 1.0f, 0.88f));

    // Picking
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickStrength",   "Pick Strength",   0.0f, 1.0f, 0.10f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickWidth",      "Pick Width",      0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("harmonics",      "Harmonics",       0.0f, 1.0f, 0.18f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pickNoise",      "Pick Noise",      0.0f, 1.0f, 0.25f));

    // Damping
    params.push_back(std::make_unique<juce::AudioParameterFloat>("color",          "Tone",            0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("bridgeDamping",  "Bridge Damping",  0.0f, 1.0f, 0.05f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("palmDamping",    "Palm Damping",    0.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>("letStringsRing",  "Let Strings Ring", true));

    // Strings
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

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<StringVoice*> (synth.getVoice(i)))
            voice->prepare (sampleRate, samplesPerBlock);
    }
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

    auto color          = apvts.getRawParameterValue("color");
    auto bridgeDamping  = apvts.getRawParameterValue("bridgeDamping");
    auto palmDamping    = apvts.getRawParameterValue("palmDamping");
    auto letStringsRing = apvts.getRawParameterValue("letStringsRing");

    auto stiffness     = apvts.getRawParameterValue("stiffness");

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<StringVoice*>(synth.getVoice(i)))
        {
            voice->setPickupPosition(pickupPosition->load());
            voice->setPickPosition(pickPosition->load());
            voice->setPalmPosition(palmPosition->load());

            voice->setPickStrength(pickStrength->load());
            voice->setPickWidth(pickWidth->load());
            voice->setHarmonics(harmonics->load());
            voice->setPickNoiseAmount(pickNoise->load());

            voice->setColor(color->load());
            voice->setBridgeDamping(bridgeDamping->load());
            voice->setPalmDamping(palmDamping->load());
            voice->setLetStringsRing(letStringsRing->load() >= 0.5f);

            voice->setStiffness(stiffness->load());
        }
    }

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
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
