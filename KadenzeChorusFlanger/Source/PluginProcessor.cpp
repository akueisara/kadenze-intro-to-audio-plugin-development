/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KadenzeChorusFlangerAudioProcessor::KadenzeChorusFlangerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    addParameter(mDryWetParameter = new juce::AudioParameterFloat("drywet",
                                                         "Dry Wet",
                                                         0.0,
                                                         1.0,
                                                         0.5));
    
    addParameter(mDryWetParameter = new juce::AudioParameterFloat("depth",
                                                         "Depth",
                                                         0.0,
                                                         1.0,
                                                         0.5));
    
    addParameter(mDryWetParameter = new juce::AudioParameterFloat("rate",
                                                         "Rate",
                                                         0.1f,
                                                         20.f,
                                                         10.f));
    
    addParameter(mFeedbackParameter = new juce::AudioParameterFloat("phaseOffset",
                                                                    "Phase Offset",
                                                                    0.0f,
                                                                    1.f,
                                                                    0.f));
    
    addParameter(mFeedbackParameter = new juce::AudioParameterFloat("feedback",
                                                                    "Feedback",
                                                                    0,
                                                                    0.98,
                                                                    0.5));
    
    addParameter(mFeedbackParameter = new juce::AudioParameterFloat("type",
                                                                    "Type",
                                                                    0,
                                                                    1,
                                                                    0));
    
    mDelayTimeSmoothed = 0;
    mCircularBufferLeft = nullptr;
    mCircularBufferRight = nullptr;
    mCircularBufferWriteHead = 0;
    mCircularBufferLength = 0;
    mDelayTimeInSamples = 0;
    mDelayReadHead = 0;
    
    mFeedbackLeft = 0;
    mFeedbackRight = 0;
}

KadenzeChorusFlangerAudioProcessor::~KadenzeChorusFlangerAudioProcessor()
{
    if (mCircularBufferLeft != nullptr) {
        delete [] mCircularBufferLeft;
        mCircularBufferLeft = nullptr;
    }
    
    if (mCircularBufferRight != nullptr) {
        delete [] mCircularBufferRight;
        mCircularBufferRight = nullptr;
    }
}

//==============================================================================
const juce::String KadenzeChorusFlangerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool KadenzeChorusFlangerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool KadenzeChorusFlangerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool KadenzeChorusFlangerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double KadenzeChorusFlangerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int KadenzeChorusFlangerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int KadenzeChorusFlangerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void KadenzeChorusFlangerAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String KadenzeChorusFlangerAudioProcessor::getProgramName (int index)
{
    return {};
}

void KadenzeChorusFlangerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void KadenzeChorusFlangerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mDelayTimeSmoothed = 1;
    
    mCircularBufferLength = sampleRate * MAX_DELAY_TIME;
    
    if (mCircularBufferLeft == nullptr) {
        mCircularBufferLeft = new float[(int)mCircularBufferLength];
    }
    
    juce::zeromem(mCircularBufferLeft, mCircularBufferLength * sizeof(float));
    
    if (mCircularBufferRight == nullptr) {
        mCircularBufferRight = new float[(int)mCircularBufferLength];
    }
    
    juce::zeromem(mCircularBufferRight, mCircularBufferLength * sizeof(float));
    
    mCircularBufferWriteHead = 0;
}

void KadenzeChorusFlangerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool KadenzeChorusFlangerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void KadenzeChorusFlangerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);
    
    for (int sample = 0; sample < buffer.getNumSamples(); sample++)
    {
        mDelayTimeSmoothed = mDelayTimeSmoothed - 0.001 * (mDelayTimeSmoothed - 0.2);
        mDelayTimeInSamples = getSampleRate() * mDelayTimeSmoothed;
        
        mCircularBufferLeft[mCircularBufferWriteHead] = leftChannel[sample] + mFeedbackLeft;
        mCircularBufferRight[mCircularBufferWriteHead] = rightChannel[sample] + mFeedbackRight;
        
        mDelayReadHead = mCircularBufferWriteHead - mDelayTimeInSamples;
        
        if (mDelayReadHead < 0) {
            mDelayReadHead += mCircularBufferLength;
        }
        
        int readHeadX0 = (int)mDelayReadHead;
        int readHeadX1 = readHeadX0 + 1;
        float readHeadFloat = mDelayReadHead - readHeadX0;
        
        if (readHeadX1 >= mCircularBufferLength) {
            readHeadX1 -= mCircularBufferLength;
        }
        
        float delaySampleLeft = linInterp(mCircularBufferLeft[readHeadX0], mCircularBufferLeft[readHeadX1], readHeadFloat);
        float delaySampleRight = linInterp(mCircularBufferRight[readHeadX0], mCircularBufferRight[readHeadX1], readHeadFloat);
        
        mFeedbackLeft = delaySampleLeft * *mFeedbackParameter;
        mFeedbackRight = delaySampleRight * *mFeedbackParameter;
        
        mCircularBufferWriteHead++;
        
        buffer.setSample(0, sample,  buffer.getSample(0, sample) * (1 - *mDryWetParameter) + delaySampleLeft * *mDryWetParameter);
        buffer.setSample(1, sample,  buffer.getSample(1, sample) * (1 - *mDryWetParameter) + delaySampleRight * *mDryWetParameter);
        
        if (mCircularBufferWriteHead >= mCircularBufferLength) {
            mCircularBufferWriteHead = 0;
        }
    }
}

//==============================================================================
bool KadenzeChorusFlangerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* KadenzeChorusFlangerAudioProcessor::createEditor()
{
    return new KadenzeChorusFlangerAudioProcessorEditor (*this);
}

//==============================================================================
void KadenzeChorusFlangerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void KadenzeChorusFlangerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KadenzeChorusFlangerAudioProcessor();
}

float KadenzeChorusFlangerAudioProcessor::linInterp(float sampleX0, float sampleX1, float inPhase) {
    return (1 - inPhase) * sampleX0 + inPhase * sampleX1;
}
