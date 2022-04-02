/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KadenzeChorusFlangerAudioProcessorEditor::KadenzeChorusFlangerAudioProcessorEditor (KadenzeChorusFlangerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
    
    auto& params = processor.getParameters();
    
    juce::AudioParameterFloat* dryWetParameter = (juce::AudioParameterFloat*) params.getUnchecked(0);
    setSlider(this, &mDryWetSlider, dryWetParameter, "dry wet", 0, 0);
    
    juce::AudioParameterFloat* depthParameter = (juce::AudioParameterFloat*) params.getUnchecked(1);
    setSlider(this, &mDepthSlider, depthParameter, "depth", 100, 0);
    
    juce::AudioParameterFloat* rateParameter = (juce::AudioParameterFloat*) params.getUnchecked(2);
    setSlider(this, &mRateSlider, rateParameter, "rate", 200, 0);
    
    juce::AudioParameterFloat* phaseOffsetParameter = (juce::AudioParameterFloat*) params.getUnchecked(3);
    setSlider(this, &mPhaseOffsetSlider, phaseOffsetParameter, "phase offset", 300, 0);
    
    juce::AudioParameterFloat* feedbackParameter = (juce::AudioParameterFloat*) params.getUnchecked(4);
    setSlider(this, &mFeedbackSlider, feedbackParameter, "feedback", 0, 100);
    
    juce::AudioParameterInt* typeParameter = (juce::AudioParameterInt*) params.getUnchecked(5);
    mType.setBounds(100, 100, 100, 30);
    mType.addItem("Chorus", 1);
    mType.addItem("Flanger", 2);
    addAndMakeVisible(mType);
    
    mType.onChange = [this, typeParameter] {
        typeParameter->beginChangeGesture();
        *typeParameter = mType.getSelectedItemIndex();
        typeParameter->endChangeGesture();
    };
    
    mType.setSelectedItemIndex(*typeParameter);
}

KadenzeChorusFlangerAudioProcessorEditor::~KadenzeChorusFlangerAudioProcessorEditor()
{
}

//==============================================================================
void KadenzeChorusFlangerAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Chorus Flanger", getLocalBounds(), juce::Justification::centred, 1);
}

void KadenzeChorusFlangerAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

void KadenzeChorusFlangerAudioProcessorEditor::setSlider(juce::Component* component, juce::Slider* slider, juce::AudioParameterFloat* param, std::string silderTitle, int boundX, int boundY)
{
    slider->setBounds(boundX, boundY, 100, 100);
    slider->setTitle(silderTitle);
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
