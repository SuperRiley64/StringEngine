/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>
#include <array>
#include "PluginProcessor.h"

//=====SVG Knob LookAndFeel========================================
/**
*/
class SvgKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SvgKnobLookAndFeel()
    {
        knob = juce::Drawable::createFromImageData(
            BinaryData::knob_svg,
            BinaryData::knob_svgSize
        );
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider&) override
    {
        if (knob == nullptr)
            return;

        const float angle = rotaryStartAngle
            + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        const auto bounds = juce::Rectangle<float>(
            (float)x, (float)y, (float)width, (float)height
        ).reduced(2.0f);

        g.saveState();

        g.addTransform(
            juce::AffineTransform::rotation(
                angle,
                bounds.getCentreX(),
                bounds.getCentreY()
            )
        );

        knob->drawWithin(
            g,
            bounds,
            juce::RectanglePlacement::centred,
            1.0f
        );

        g.restoreState();
    }

private:
    std::unique_ptr<juce::Drawable> knob;
};



//==============================================================================
/**
*/
class GuitarSynthAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                         private juce::Timer
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
    
    // Visualizer
    void timerCallback() override;

    void drawStringVisualizer(juce::Graphics& g, juce::Rectangle<int> area);

    std::array<std::vector<float>, 6> stringDisplayBuffers;

    juce::Rectangle<int> stringVisualizerArea;
    
    juce::Image backgroundImage;
    
    SvgKnobLookAndFeel svgKnobLookAndFeel;
    
    // Parameters
    // Fretboard sliders
    juce::Slider pickupPositionSlider;
    juce::Slider pickPositionSlider;
    juce::Slider palmPositionSlider;

    juce::Label pickupPositionLabel;
    juce::Label pickPositionLabel;
    juce::Label palmPositionLabel;

    // Knobs
    juce::Slider slideSlider;
    juce::Slider vibratoDepthSlider;
    juce::Slider vibratoRateSlider;
    juce::Slider legatoTimeSlider;
    juce::ToggleButton monoLegatoButton;
    
    juce::Slider bodyMixSlider;
    juce::Slider bodySizeSlider;
    juce::Slider bodyDampingSlider;
    juce::Slider sympatheticSlider;
    juce::Slider strumSlider;
    
    juce::Slider pickStrengthSlider;
    juce::Slider pickWidthSlider;
    juce::Slider harmonicsSlider;
    juce::Slider pickNoiseSlider;
    
    juce::Slider pickShapeSlider;
    juce::Slider pickCenterSlider;

    juce::Slider colorSlider;
    juce::Slider bridgeDampingSlider;
    juce::Slider palmDampingSlider;
    juce::Slider stiffnessSlider;

    juce::ToggleButton letStringsRingButton;
    
    // Labels
    juce::Label slideLabel;
    juce::Label vibratoDepthLabel;
    juce::Label vibratoRateLabel;
    juce::Label legatoTimeLabel;
    juce::Label monoLegatoLabel;
    
    juce::Label bodyMixLabel;
    juce::Label bodySizeLabel;
    juce::Label bodyDampingLabel;
    juce::Label sympatheticLabel;
    juce::Label strumLabel;

    juce::Label pickStrengthLabel;
    juce::Label pickWidthLabel;
    juce::Label harmonicsLabel;
    juce::Label pickNoiseLabel;
    
    juce::Label pickShapeLabel;
    juce::Label pickCenterLabel;

    juce::Label colorLabel;
    juce::Label bridgeDampingLabel;
    juce::Label palmDampingLabel;
    juce::Label stiffnessLabel;
    juce::Label letStringsRingLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> slideAttachment;
    std::unique_ptr<SliderAttachment> vibratoDepthAttachment;
    std::unique_ptr<SliderAttachment> vibratoRateAttachment;
    std::unique_ptr<SliderAttachment> legatoTimeAttachment;
    std::unique_ptr<ButtonAttachment> monoLegatoAttachment;
    
    std::unique_ptr<SliderAttachment> bodyMixAttachment;
    std::unique_ptr<SliderAttachment> bodySizeAttachment;
    std::unique_ptr<SliderAttachment> bodyDampingAttachment;
    std::unique_ptr<SliderAttachment> sympatheticAttachment;
    std::unique_ptr<SliderAttachment> strumAttachment;
    
    std::unique_ptr<SliderAttachment> pickupPositionAttachment;
    std::unique_ptr<SliderAttachment> pickPositionAttachment;
    std::unique_ptr<SliderAttachment> palmPositionAttachment;

    std::unique_ptr<SliderAttachment> pickStrengthAttachment;
    std::unique_ptr<SliderAttachment> pickWidthAttachment;
    std::unique_ptr<SliderAttachment> harmonicsAttachment;
    std::unique_ptr<SliderAttachment> pickNoiseAttachment;
    
    std::unique_ptr<SliderAttachment> pickShapeAttachment;
    std::unique_ptr<SliderAttachment> pickCenterAttachment;

    std::unique_ptr<SliderAttachment> colorAttachment;
    std::unique_ptr<SliderAttachment> bridgeDampingAttachment;
    std::unique_ptr<SliderAttachment> palmDampingAttachment;
    std::unique_ptr<SliderAttachment> stiffnessAttachment;

    std::unique_ptr<ButtonAttachment> letStringsRingAttachment;
};
