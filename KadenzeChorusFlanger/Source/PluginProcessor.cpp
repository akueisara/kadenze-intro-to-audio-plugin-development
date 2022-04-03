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
    
    // Construct & Add Our Parameters
    
    addParameter(mDryWetParameter = new juce::AudioParameterFloat("drywet",
                                                         "Dry Wet",
                                                         0.0,
                                                         1.0,
                                                         0.5));
    
    addParameter(mDepthParameter = new juce::AudioParameterFloat("depth",
                                                         "Depth",
                                                         0.0,
                                                         1.0,
                                                         0.5));
    
    addParameter(mRateParameter = new juce::AudioParameterFloat("rate",
                                                         "Rate",
                                                         0.1f,
                                                         20.f,
                                                         10.f));
    
    addParameter(mPhaseOffsetParameter = new juce::AudioParameterFloat("phaseOffset",
                                                                    "Phase Offset",
                                                                    0.0f,
                                                                    1.f,
                                                                    0.f));
    
    addParameter(mFeedbackParameter = new juce::AudioParameterFloat("feedback",
                                                                    "Feedback",
                                                                    0,
                                                                    0.98,
                                                                    0.5));
    
    addParameter(mTypeParameter = new juce::AudioParameterInt("type",
                                                                    "Type",
                                                                    0,
                                                                    1,
                                                                    0));
    
    // Initialize our data to default values
    
    mCircularBufferLeft = nullptr;
    mCircularBufferRight = nullptr;
    mCircularBufferWriteHead = 0;
    mCircularBufferLength = 0;
    
    mFeedbackLeft = 0;
    mFeedbackRight = 0;
    
    mLFOPhase = 0;
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
    // initialize our data for the current sample rate, and reset things such as phase and writeheads
    
    // initialize the phase
    mLFOPhase = 0;
    
    // calculate the circular buffer length
    mCircularBufferLength = sampleRate * MAX_DELAY_TIME;
    
    // initialize the left buffer
    if (mCircularBufferLeft == nullptr) {
        mCircularBufferLeft = new float[(int)mCircularBufferLength];
    }
    
    // clear any junk data in new buffer
    juce::zeromem(mCircularBufferLeft, mCircularBufferLength * sizeof(float));
    
    // initialize the right buffer
    if (mCircularBufferRight == nullptr) {
        mCircularBufferRight = new float[(int)mCircularBufferLength];
    }
    
    // clear any junk
    juce::zeromem(mCircularBufferRight, mCircularBufferLength * sizeof(float));
    
    // init the right head to 0
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
    
//    DBG("DRY WET: " << *mDryWetParameter);
//    DBG("DEPTH: " << *mDepthParameter);
//    DBG("RATE: " << *mRateParameter);
//    DBG("PHASE OFFSET: " << *mPhaseOffsetParameter);
//    DBG("FEEDBACK: " << *mFeedbackParameter);
//    DBG("TYPE: " << *mTypeParameter);

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // obtain the left and right audio data pointers
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);
    
    // iterate through all the samples in the buffer
    for (int sample = 0; sample < buffer.getNumSamples(); sample++)
    {
        // write into our circular buffer
        mCircularBufferLeft[mCircularBufferWriteHead] = leftChannel[sample] + mFeedbackLeft;
        mCircularBufferRight[mCircularBufferWriteHead] = rightChannel[sample] + mFeedbackRight;
        
        // generate the left lfo output
        float lfoOutLeft = sin(2 * M_PI * mLFOPhase);
        
        // calculate the right channnel lfo phase
        float lfoPhaseRight = mLFOPhase + *mPhaseOffsetParameter;
        
        if (lfoPhaseRight > 1) {
            lfoPhaseRight -= 1;
        }
        
        // generate the right channel lfo output
        float lfoOutRight = sin(2 * M_PI * lfoPhaseRight);
        
        // moving our lfo phase forward
        mLFOPhase += *mRateParameter / getSampleRate();
        
        if (mLFOPhase > 1) {
            mLFOPhase -= 1;
        }
        
        // control the lfo depth
        lfoOutLeft *= *mDepthParameter;
        lfoOutRight *= *mDepthParameter;
        
        float lfoOutMappedLeft = 0;
        float lfoOutMappedRight = 0;
        
        // map out lfo output to our desired delay times
        
        /** chrous */
        if (*mTypeParameter == 0) {
            // jmap: Remaps a normalised value (between 0 and 1) to a target range.
            lfoOutMappedLeft = juce::jmap(lfoOutLeft, -1.f, 1.f, 0.005f, 0.03f);
            lfoOutMappedRight = juce::jmap(lfoOutRight, -1.f, 1.f, 0.005f, 0.03f);
        /** flanger */
        } else {
            lfoOutMappedLeft = juce::jmap(lfoOutLeft, -1.f, 1.f, 0.001f, 0.005f);
            lfoOutMappedRight = juce::jmap(lfoOutRight, -1.f, 1.f, 0.001f, 0.005f);
        }
        
        // calculate the delay lengths in samples
        float delayTimeInSamplesLeft = getSampleRate() * lfoOutMappedLeft;
        float delayTimeInSamplesRight = getSampleRate() * lfoOutMappedRight;
        
        // calculate the left read head position
        float delayReadHeadLeft = mCircularBufferWriteHead - delayTimeInSamplesLeft;
        
        if (delayReadHeadLeft < 0) {
            delayReadHeadLeft += mCircularBufferLength;
        }
        
        // calculate the right read head position
        float delayReadHeadRight = mCircularBufferWriteHead - delayTimeInSamplesRight;
        
        if (delayReadHeadRight < 0) {
            delayReadHeadRight += mCircularBufferLength;
        }
        
        // calculate linear interpolation points for left channel
        int readHeadLeftX0 = (int)delayReadHeadLeft;
        int readHeadLeftX1 = readHeadLeftX0 + 1;
        float readHeadFloatLeft = delayReadHeadLeft - readHeadLeftX0;
        
        if (readHeadLeftX1 >= mCircularBufferLength) {
            readHeadLeftX1 -= mCircularBufferLength;
        }
        
        int readHeadRightX0 = (int)delayReadHeadRight;
        int readHeadRightX1 = readHeadRightX0 + 1;
        float readHeadFloatRight = delayReadHeadRight - readHeadRightX0;
        
        if (readHeadRightX1 >= mCircularBufferLength) {
            readHeadRightX1 -= mCircularBufferLength;
        }
        
        // generate left and right output samples
        float delaySampleLeft = linInterp(mCircularBufferLeft[readHeadLeftX0], mCircularBufferLeft[readHeadLeftX1], readHeadFloatLeft);
        float delaySampleRight = linInterp(mCircularBufferRight[readHeadRightX0], mCircularBufferRight[readHeadRightX1], readHeadFloatRight);
        
        mFeedbackLeft = delaySampleLeft * *mFeedbackParameter;
        mFeedbackRight = delaySampleRight * *mFeedbackParameter;
        
        mCircularBufferWriteHead++;
        
        if (mCircularBufferWriteHead >= mCircularBufferLength) {
            mCircularBufferWriteHead = 0;
        }
        
        float dryAmount = 1 - *mDryWetParameter;
        float wetAmount  = *mDryWetParameter;
        
        buffer.setSample(0, sample,  buffer.getSample(0, sample) * dryAmount + delaySampleLeft * wetAmount);
        buffer.setSample(1, sample,  buffer.getSample(1, sample) * dryAmount + delaySampleRight * wetAmount);
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
    std::unique_ptr<juce::XmlElement> xml(new juce::XmlElement("FlangerChorus"));
    
    xml->setAttribute("DryWet", *mDryWetParameter);
    xml->setAttribute("Depth", *mDepthParameter);
    xml->setAttribute("Rate", *mRateParameter);
    xml->setAttribute("PhaseOffset", *mPhaseOffsetParameter);
    xml->setAttribute("Feedback", *mFeedbackParameter);
    xml->setAttribute("Type", *mTypeParameter);
    
    copyXmlToBinary(*xml, destData);
}

void KadenzeChorusFlangerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    
    if (xml.get() != nullptr && xml->hasTagName(("FlangerChorus"))) {
        *mDryWetParameter = xml->getDoubleAttribute(("DryWet"));
        *mDepthParameter = xml->getDoubleAttribute(("Depth"));
        *mRateParameter = xml->getDoubleAttribute(("Rate"));
        *mPhaseOffsetParameter = xml->getDoubleAttribute(("PhaseOffset"));
        *mFeedbackParameter = xml->getDoubleAttribute(("Feedback"));
        *mTypeParameter = xml->getIntAttribute("Type");
    }
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
