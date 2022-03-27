/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KadenzePluginAudioProcessor::KadenzePluginAudioProcessor()
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
    addParameter(mGainParameter = new juce::AudioParameterFloat("gain",
                                                                "Gain",
                                                                0.0f,
                                                                1.0f,
                                                                0.5f));
    mGainSmoothed = mGainParameter->get();
    
    mCircularBuffer = nullptr;
    mCircularBufferWriteHead = 0;
    mCircularBufferLength = 0;
    mDelayTimeInSamples = 0;
    mDelayReadHead = 0;
}

KadenzePluginAudioProcessor::~KadenzePluginAudioProcessor()
{
    if (mCircularBuffer != nullptr) {
        delete [] mCircularBuffer;
        mCircularBuffer = nullptr;
    }
}

//==============================================================================
const juce::String KadenzePluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool KadenzePluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool KadenzePluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool KadenzePluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double KadenzePluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int KadenzePluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int KadenzePluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void KadenzePluginAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String KadenzePluginAudioProcessor::getProgramName (int index)
{
    return {};
}

void KadenzePluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void KadenzePluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    
    mDelayTimeInSamples = sampleRate * 0.5;
    
    mCircularBufferLength = sampleRate * MAX_DELAY_TIME;
    
    if (mCircularBuffer == nullptr) {
        mCircularBuffer = new float[(int)(mCircularBufferLength)];
    }
    
    mCircularBufferWriteHead = 0;
}

void KadenzePluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool KadenzePluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void KadenzePluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
    
    for (int sample = 0; sample < buffer.getNumSamples(); sample++)
    {
        // Frequency Gain Formula: x = x - z * (x - y), where x = smoothed value, y = target value, z = scalar (speed)
        mGainSmoothed = mGainSmoothed - 0.004 * (mGainSmoothed - mGainParameter->get());
        
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            
            channelData[sample] *= mGainSmoothed;
            
            mCircularBuffer[mCircularBufferWriteHead] = channelData[sample];
            
            mDelayReadHead = mCircularBufferWriteHead - mDelayTimeInSamples;
            
            if (mDelayReadHead < 0) {
                mDelayReadHead += mCircularBufferLength;
            }
            
            buffer.addSample(channel, sample, mCircularBuffer[(int)mDelayReadHead]);

            mCircularBufferWriteHead++;
            
            if (mCircularBufferWriteHead >= mCircularBufferLength) {
                mCircularBufferWriteHead = 0;
            }
        }
    }
}

//==============================================================================
bool KadenzePluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* KadenzePluginAudioProcessor::createEditor()
{
    return new KadenzePluginAudioProcessorEditor (*this);
}

//==============================================================================
void KadenzePluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void KadenzePluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KadenzePluginAudioProcessor();
}
