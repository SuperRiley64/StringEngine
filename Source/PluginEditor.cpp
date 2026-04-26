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
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    };

    auto setupFretSlider = [this] (juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
        slider.setRange(0.0, 1.0, 0.001);
        addAndMakeVisible(slider);
    };

    auto setupKnob = [this] (juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
        slider.setRange(0.0, 1.0, 0.001);
        addAndMakeVisible(slider);
    };

    // Labels/sliders
    setupLabel(pickupPositionLabel, "Pickup");
    setupFretSlider(pickupPositionSlider);

    setupLabel(pickPositionLabel, "Pick");
    setupFretSlider(pickPositionSlider);

    setupLabel(palmPositionLabel, "Palm");
    setupFretSlider(palmPositionSlider);

    setupLabel(pickStrengthLabel, "Pick Strength");
    setupKnob(pickStrengthSlider);

    setupLabel(pickWidthLabel, "Pick Width");
    setupKnob(pickWidthSlider);

    setupLabel(harmonicsLabel, "Harmonics");
    setupKnob(harmonicsSlider);

    setupLabel(pickNoiseLabel, "Pick Noise");
    setupKnob(pickNoiseSlider);

    setupLabel(colorLabel, "Tone");
    setupKnob(colorSlider);

    setupLabel(bridgeDampingLabel, "Bridge Damp");
    setupKnob(bridgeDampingSlider);

    setupLabel(palmDampingLabel, "Palm Damp");
    setupKnob(palmDampingSlider);

    setupLabel(dispersionLabel, "Dispersion");
    setupKnob(dispersionSlider);

    setupLabel(letStringsRingLabel, "Let Ring");
    addAndMakeVisible(letStringsRingButton);

    // Attachments
    pickupPositionAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pickupPosition", pickupPositionSlider);
    pickPositionAttachment   = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pickPosition", pickPositionSlider);
    palmPositionAttachment   = std::make_unique<SliderAttachment>(audioProcessor.apvts, "palmPosition", palmPositionSlider);

    pickStrengthAttachment   = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pickStrength", pickStrengthSlider);
    pickWidthAttachment      = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pickWidth", pickWidthSlider);
    harmonicsAttachment      = std::make_unique<SliderAttachment>(audioProcessor.apvts, "harmonics", harmonicsSlider);
    pickNoiseAttachment      = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pickNoise", pickNoiseSlider);

    colorAttachment          = std::make_unique<SliderAttachment>(audioProcessor.apvts, "color", colorSlider);
    bridgeDampingAttachment  = std::make_unique<SliderAttachment>(audioProcessor.apvts, "bridgeDamping", bridgeDampingSlider);
    palmDampingAttachment    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "palmDamping", palmDampingSlider);
    dispersionAttachment     = std::make_unique<SliderAttachment>(audioProcessor.apvts, "dispersion", dispersionSlider);

    letStringsRingAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "letStringsRing", letStringsRingButton);

    startTimerHz(30);
    setSize(1000, 500);
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
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(28);

    auto top = area.removeFromTop(260);

    auto leftHalf = top.removeFromLeft(area.getWidth() / 2);
    stringVisualizerArea = leftHalf.reduced(4);

    auto fretRow = area.removeFromTop(80);
    auto sliderWidth = fretRow.getWidth() / 3;

    auto layoutFretSlider = [&fretRow, sliderWidth] (juce::Label& label, juce::Slider& slider)
    {
        auto cell = fretRow.removeFromLeft(sliderWidth).reduced(8, 0);
        label.setBounds(cell.removeFromTop(20));
        slider.setBounds(cell);
    };

    layoutFretSlider(pickupPositionLabel, pickupPositionSlider);
    layoutFretSlider(pickPositionLabel, pickPositionSlider);
    layoutFretSlider(palmPositionLabel, palmPositionSlider);

    area.removeFromTop(12);

    auto knobRow = area.removeFromTop(130);

    auto layoutKnob = [&knobRow] (juce::Label& label, juce::Slider& slider)
    {
        auto cell = knobRow.removeFromLeft(90).reduced(4);
        label.setBounds(cell.removeFromTop(22));
        slider.setBounds(cell);
    };

    layoutKnob(pickStrengthLabel, pickStrengthSlider);
    layoutKnob(pickWidthLabel, pickWidthSlider);
    layoutKnob(harmonicsLabel, harmonicsSlider);
    layoutKnob(pickNoiseLabel, pickNoiseSlider);
    layoutKnob(colorLabel, colorSlider);
    layoutKnob(bridgeDampingLabel, bridgeDampingSlider);
    layoutKnob(palmDampingLabel, palmDampingSlider);
    layoutKnob(dispersionLabel, dispersionSlider);

    auto buttonCell = knobRow.removeFromLeft(90).reduced(4);
    letStringsRingLabel.setBounds(buttonCell.removeFromTop(22));
    letStringsRingButton.setBounds(buttonCell.withSizeKeepingCentre(30, 30));
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
