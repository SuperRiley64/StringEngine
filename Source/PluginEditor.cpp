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

    startTimerHz(30); // For Visualizer
    setSize(620, 620);
}

GuitarSynthAudioProcessorEditor::~GuitarSynthAudioProcessorEditor()
{
}

//==============================================================================

void GuitarSynthAudioProcessorEditor::timerCallback()
{
    audioProcessor.copyActiveStringStates(stringDisplayBuffers);
    repaint();
}

void GuitarSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("String Engine", getLocalBounds().removeFromTop(30),
                     juce::Justification::centred, 1);

    drawStringVisualizer(g, stringVisualizerArea);
}

void GuitarSynthAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(32);

    stringVisualizerArea = area.removeFromTop(180);
    area.removeFromTop(14);

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

void GuitarSynthAudioProcessorEditor::drawStringVisualizer(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colours::darkgrey);
    g.fillRoundedRectangle(area.toFloat(), 6.0f);

    g.setColour(juce::Colours::grey);
    g.drawRoundedRectangle(area.toFloat(), 6.0f, 1.0f);

    auto inner = area.reduced(12);

    const auto* pickupParam = audioProcessor.apvts.getRawParameterValue("pickupPosition");
    const auto* pickParam   = audioProcessor.apvts.getRawParameterValue("pickPosition");
    const auto* widthParam  = audioProcessor.apvts.getRawParameterValue("pickWidth");

    const float pickupPosition = pickupParam != nullptr ? pickupParam->load() : 0.85f;
    const float pickPosition   = pickParam   != nullptr ? pickParam->load()   : 0.25f;
    const float pickWidth      = widthParam  != nullptr ? widthParam->load()  : 0.25f;

    auto getXFromPosition = [&inner] (float normalized)
    {
        normalized = juce::jlimit(0.0f, 1.0f, normalized);
        return inner.getX() + normalized * inner.getWidth();
    };

    const float pickupX = getXFromPosition(pickupPosition);
    const float pickX   = getXFromPosition(pickPosition);

    // Shared pick width overlay.
    const float widthPixels = 8.0f + pickWidth * 80.0f;
    g.setColour(juce::Colours::orange.withAlpha(0.14f));
    g.fillRect(juce::Rectangle<float>(
        pickX - widthPixels * 0.5f,
        (float)inner.getY(),
        widthPixels,
        (float)inner.getHeight()));

    // Shared pick / pickup markers.
    g.setColour(juce::Colours::orange);
    g.drawLine(pickX, (float)inner.getY(), pickX, (float)inner.getBottom(), 2.0f);
    g.drawText("Pick", (int)pickX - 25, inner.getY(), 50, 18, juce::Justification::centred);

    g.setColour(juce::Colours::limegreen);
    g.drawLine(pickupX, (float)inner.getY(), pickupX, (float)inner.getBottom(), 2.0f);
    g.drawText("Pickup", (int)pickupX - 35, inner.getBottom() - 18, 70, 18, juce::Justification::centred);

    const int stringCount = 6;
    const float laneHeight = inner.getHeight() / (float)stringCount;

    for (int s = 0; s < stringCount; ++s)
    {
        auto lane = inner.withY((int)(inner.getY() + s * laneHeight))
                         .withHeight((int)laneHeight);

        const float centerY = (float)lane.getCentreY();

        // Resting string line.
        g.setColour(juce::Colours::white.withAlpha(0.20f));
        g.drawHorizontalLine((int)centerY, inner.getX(), inner.getRight());

        // String number label.
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.drawText(juce::String(s + 1), inner.getX() - 8, lane.getY(), 10, lane.getHeight(),
                   juce::Justification::centredRight);

        const auto& buffer = stringDisplayBuffers[(size_t)s];

        if (buffer.size() > 2)
        {
            juce::Path stringPath;
            const int n = (int)buffer.size();

            for (int i = 0; i < n; ++i)
            {
                const float xNorm = (float)i / (float)(n - 1);
                const float x = inner.getX() + xNorm * inner.getWidth();

                // Visual gain only.
                const float y = centerY - buffer[(size_t)i] * laneHeight;

                if (i == 0)
                    stringPath.startNewSubPath(x, y);
                else
                    stringPath.lineTo(x, y);
            }

            g.setColour(juce::Colours::cyan);
            g.strokePath(stringPath, juce::PathStrokeType(1.5f));
        }
    }
}
