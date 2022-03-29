/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KadenzeDelayAudioProcessorEditor::KadenzeDelayAudioProcessorEditor (KadenzeDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(400, 300);
    
    auto& params = processor.getParameters();
    
    juce::AudioParameterFloat* dryWetParameter = (juce::AudioParameterFloat*) params.getUnchecked(0);
    setSlider(this, &mDryWetSlider, dryWetParameter, 0);
    
    juce::AudioParameterFloat* feedbackParameter = (juce::AudioParameterFloat*) params.getUnchecked(1);
    setSlider(this, &mFeedbackSlider, feedbackParameter, 100);

    juce::AudioParameterFloat* delayTimeParameter = (juce::AudioParameterFloat*) params.getUnchecked(2);
    setSlider(this, &mDelayTimeSlider, delayTimeParameter, 200);
}

KadenzeDelayAudioProcessorEditor::~KadenzeDelayAudioProcessorEditor()
{
}

//==============================================================================
void KadenzeDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void KadenzeDelayAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

void KadenzeDelayAudioProcessorEditor::setSlider(juce::Component* component, juce::Slider* slider, juce::AudioParameterFloat* param, int boundX)
{
    slider->setBounds(boundX, 0, 100, 100);
    slider->setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    slider->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    slider->setRange(param->range.start, param->range.end);
    slider->setValue(param->get());
    component->addAndMakeVisible(slider);

    slider->onValueChange = [component, slider, param]() {
        *param = slider->getValue();
    };
    slider->onDragStart = [param] {
        param->beginChangeGesture();
    };
    slider->onDragEnd = [param] {
        param->endChangeGesture();
    };
}
