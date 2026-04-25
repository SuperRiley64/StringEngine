/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class GuitarSynthAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    GuitarSynthAudioProcessorEditor (GuitarSynthAudioProcessor&);
    ~GuitarSynthAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    GuitarSynthAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarSynthAudioProcessorEditor)
    
    // Parameters
    juce::Slider decaySlider;
    juce::Slider colorSlider;
    juce::Slider pickupPositionSlider;
    juce::Slider pickPositionSlider;
    juce::Slider pickWidthSlider;
    juce::Slider pickStrengthSlider;

    juce::ToggleButton letStringsRingButton;

    juce::Label decayLabel;
    juce::Label colorLabel;
    juce::Label pickupPositionLabel;
    juce::Label pickPositionLabel;
    juce::Label pickWidthLabel;
    juce::Label pickStrengthLabel;
    juce::Label letStringsRingLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> decayAttachment;
    std::unique_ptr<SliderAttachment> colorAttachment;
    std::unique_ptr<SliderAttachment> pickupPositionAttachment;
    std::unique_ptr<SliderAttachment> pickPositionAttachment;
    std::unique_ptr<SliderAttachment> pickWidthAttachment;
    std::unique_ptr<SliderAttachment> pickStrengthAttachment;

    std::unique_ptr<ButtonAttachment> letStringsRingAttachment;
};
