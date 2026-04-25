/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GuitarSynthAudioProcessorEditor::GuitarSynthAudioProcessorEditor (GuitarSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    auto setupLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(label);
    };

    auto setupSlider = [this] (juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
        slider.setRange(0.0, 1.0, 0.001);
        addAndMakeVisible(slider);
    };

    setupLabel(decayLabel, "Decay");
    setupSlider(decaySlider);

    setupLabel(colorLabel, "Color");
    setupSlider(colorSlider);

    setupLabel(pickupPositionLabel, "Pickup Position");
    setupSlider(pickupPositionSlider);

    setupLabel(pickPositionLabel, "Pick Position");
    setupSlider(pickPositionSlider);

    setupLabel(pickWidthLabel, "Pick Width");
    setupSlider(pickWidthSlider);

    setupLabel(pickStrengthLabel, "Pick Strength");
    setupSlider(pickStrengthSlider);

    setupLabel(letStringsRingLabel, "Let Strings Ring");
    letStringsRingButton.setButtonText("");
    addAndMakeVisible(letStringsRingButton);

    decayAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "decay", decaySlider);
    colorAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "color", colorSlider);
    pickupPositionAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pickupPosition", pickupPositionSlider);
    pickPositionAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pickPosition", pickPositionSlider);
    pickWidthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pickWidth", pickWidthSlider);
    pickStrengthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pickStrength", pickStrengthSlider);
    letStringsRingAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "letStringsRing", letStringsRingButton);

    setSize (520, 340);
}

GuitarSynthAudioProcessorEditor::~GuitarSynthAudioProcessorEditor()
{
}

//==============================================================================
void GuitarSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("GuitarSynth", getLocalBounds().removeFromTop(30),
                      juce::Justification::centred, 1);
}

void GuitarSynthAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(32);

    auto layoutSliderRow = [&area] (juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop(36);
        label.setBounds(row.removeFromLeft(130));
        slider.setBounds(row);
        area.removeFromTop(6);
    };
    
    layoutSliderRow(pickupPositionLabel, pickupPositionSlider);
    layoutSliderRow(pickPositionLabel, pickPositionSlider);
    layoutSliderRow(pickWidthLabel, pickWidthSlider);
    layoutSliderRow(pickStrengthLabel, pickStrengthSlider);
    
    layoutSliderRow(decayLabel, decaySlider);
    layoutSliderRow(colorLabel, colorSlider);

    auto row = area.removeFromTop(36);
    letStringsRingLabel.setBounds(row.removeFromLeft(130));
    letStringsRingButton.setBounds(row.removeFromLeft(30));
}
