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
    decayLabel.setText("Decay", juce::dontSendNotification);
    decayLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(decayLabel);

    decaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    decaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    decaySlider.setRange(0.0, 1.0, 0.001);
    addAndMakeVisible(decaySlider);

    colorLabel.setText("Color", juce::dontSendNotification);
    colorLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(colorLabel);

    colorSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    colorSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    colorSlider.setRange(0.0, 1.0, 0.001);
    addAndMakeVisible(colorSlider);

    decayAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "decay", decaySlider);
    colorAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "color", colorSlider);

    setSize (420, 140);
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
    area.removeFromTop(28);

    auto row1 = area.removeFromTop(40);
    decayLabel.setBounds(row1.removeFromLeft(70));
    decaySlider.setBounds(row1);

    area.removeFromTop(8);

    auto row2 = area.removeFromTop(40);
    colorLabel.setBounds(row2.removeFromLeft(70));
    colorSlider.setBounds(row2);
}
