/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#define MAX_DELAY_TIME 2

//==============================================================================
/**
*/
class KadenzeDelayAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    KadenzeDelayAudioProcessor();
    ~KadenzeDelayAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    float linInterp(float sampleX0, float sampleX1, float inPhase);
    
private:
    
    juce::AudioParameterFloat* mDryWetParameter;
    juce::AudioParameterFloat* mFeedbackParameter;
    juce::AudioParameterFloat* mDelayTimeParameter;
    
    float mFeedbackLeft;
    float mFeedbackRight;
    
    float mDelayTimeInSamples;
    float mDelayReadHead;
    
    int mCircularBufferWriteHead;
    int mCircularBufferLength;
    
    float* mCircularBufferLeft;
    float* mCircularBufferRight;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KadenzeDelayAudioProcessor)
};
