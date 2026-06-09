#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FrontAudioProcessor::FrontAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameters())
{
    for (auto* id : { "body","glue","presence","air","edge","width","gaincomp" })
        apvts.addParameterListener (id, this);
}

FrontAudioProcessor::~FrontAudioProcessor()
{
    for (auto* id : { "body","glue","presence","air","edge","width","gaincomp" })
        apvts.removeParameterListener (id, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout FrontAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("body",     "BODY",     0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("glue",     "GLUE",     0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("presence", "PRESENCE", 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("air",      "AIR",      0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("edge",     "EDGE",     0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("width",    "WIDTH",    0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterBool>  ("gaincomp", "Gain Comp", false));
    return { p.begin(), p.end() };
}

void FrontAudioProcessor::parameterChanged (const juce::String&, float)
{
    bodyAmount    .store (apvts.getRawParameterValue ("body")    ->load());
    glueAmount    .store (apvts.getRawParameterValue ("glue")    ->load());
    presenceAmount.store (apvts.getRawParameterValue ("presence")->load());
    airAmount     .store (apvts.getRawParameterValue ("air")     ->load());
    edgeAmount    .store (apvts.getRawParameterValue ("edge")    ->load());
    widthAmount   .store (apvts.getRawParameterValue ("width")   ->load());
    gainCompEnabled.store (*apvts.getRawParameterValue ("gaincomp") > 0.5f);
    paramsDirty   .store (true);
}

//==============================================================================
const juce::String FrontAudioProcessor::getProgramName (int index)
{
    switch (index)
    {
        case 0: return "Balanced";
        case 1: return "Aggressive";
        case 2: return "Bus / Subtle";
        default: return {};
    }
}

void FrontAudioProcessor::setCurrentProgram (int index)
{
    currentProgram = index;
    // body, glue, presence, air, edge, width
    static const float presets[3][6] =
    {
        { 0.45f, 0.35f, 0.40f, 0.30f, 0.15f, 0.25f },  // Balanced
        { 0.65f, 0.55f, 0.65f, 0.45f, 0.40f, 0.35f },  // Aggressive
        { 0.20f, 0.18f, 0.22f, 0.18f, 0.05f, 0.10f },  // Bus / Subtle
    };
    const float* pr = presets[index];
    apvts.getParameter ("body")    ->setValueNotifyingHost (pr[0]);
    apvts.getParameter ("glue")    ->setValueNotifyingHost (pr[1]);
    apvts.getParameter ("presence")->setValueNotifyingHost (pr[2]);
    apvts.getParameter ("air")     ->setValueNotifyingHost (pr[3]);
    apvts.getParameter ("edge")    ->setValueNotifyingHost (pr[4]);
    apvts.getParameter ("width")   ->setValueNotifyingHost (pr[5]);
}

//==============================================================================
void FrontAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    hpf           .prepare (spec);
    bodyFilter    .prepare (spec);
    presenceFilter.prepare (spec);
    pultecPeak    .prepare (spec);
    pultecShelf   .prepare (spec);
    deEsserBP     .prepare (spec);
    edgeShelf     .prepare (spec);

    // Mono spec for side channel filters
    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate       = sampleRate;
    monoSpec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    monoSpec.numChannels      = 1;
    sideHPF .prepare (monoSpec);
    sideAir .prepare (monoSpec);

    optComp.kneeDb = 6.0f;
    optComp.prepare (sampleRate);

    // RMS smoothing for gain compensation
    float buffersPerSec = (float) sampleRate / (float) samplesPerBlock;
    rmsCoeff      = std::exp (-1.0f / (buffersPerSec * 0.5f));   // 500ms window
    gainTrimCoeff = std::exp (-1.0f / (buffersPerSec * 0.1f));   // 100ms trim smoothing
    gainTrimSmooth  = 1.0f;
    inputRmsLevel   = 0.0f;
    outputRmsLevel  = 0.0f;
    gainCompEnabled.store (*apvts.getRawParameterValue ("gaincomp") > 0.5f);

    // Fixed filters
    *hpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 80.0f, 0.707f);
    *deEsserBP.state = *juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 7000.0f, 2.0f);

    // Side HPF — removes low-frequency stereo content below 250Hz (keeps bass centred)
    *sideHPF.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 250.0f, 0.707f);
    // Side air — adds brightness to the stereo field at 8kHz
    *sideAir.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 8000.0f, 0.707f, 1.0f);

    bodyAmount    .store (apvts.getRawParameterValue ("body")    ->load());
    glueAmount    .store (apvts.getRawParameterValue ("glue")    ->load());
    presenceAmount.store (apvts.getRawParameterValue ("presence")->load());
    airAmount     .store (apvts.getRawParameterValue ("air")     ->load());
    edgeAmount    .store (apvts.getRawParameterValue ("edge")    ->load());
    widthAmount   .store (apvts.getRawParameterValue ("width")   ->load());
    paramsDirty   .store (true);

    updateFilters();
    updateCompressor();
}

void FrontAudioProcessor::releaseResources() {}

//==============================================================================
void FrontAudioProcessor::updateFilters()
{
    if (currentSampleRate <= 0.0) return;
    const float sr = (float) currentSampleRate;

    // BODY: peak cut at 400 Hz — mud removal before compressor
    float bodyGain = juce::Decibels::decibelsToGain (bodyAmount.load() * -6.0f);
    *bodyFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 400.0f, 1.0f, bodyGain);

    // PRESENCE: peak boost at 2500 Hz after compressor
    float presGain = juce::Decibels::decibelsToGain (presenceAmount.load() * 4.0f);
    *presenceFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 2500.0f, 1.5f, presGain);

    // PULTEC AIR: resonant peak at 12kHz + gentle shelf from 8kHz
    // Peak: full boost amount at 12kHz with low Q (broad resonant bump)
    float airGain    = juce::Decibels::decibelsToGain (airAmount.load() * 3.5f);
    float airGainSh  = juce::Decibels::decibelsToGain (airAmount.load() * 1.5f); // shelf lower
    *pultecPeak  .state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter    (sr, 12000.0f, 0.5f, airGain);
    *pultecShelf .state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf     (sr,  8000.0f, 0.707f, airGainSh);

    // EDGE shelf: 10kHz boost applied after polynomial (HEQ trick)
    float edgeShGain = juce::Decibels::decibelsToGain (edgeAmount.load() * 2.75f);
    *edgeShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 10000.0f, 0.707f, edgeShGain);

    // Side air gain (WIDTH section)
    float sideAirGain = juce::Decibels::decibelsToGain (widthAmount.load() * 2.0f);
    *sideAir.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 8000.0f, 0.707f, sideAirGain);

    paramsDirty.store (false);
}

void FrontAudioProcessor::updateCompressor()
{
    optComp.threshDb = glueAmount.load() * -24.0f; // 0→0dB, 1→-24dB
    optComp.ratio    = 3.5f;
}

//==============================================================================
void FrontAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (paramsDirty.load())
    {
        updateFilters();
        updateCompressor();
    }

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Measure input RMS (kept updated even when gain comp is off, so it's ready instantly)
    {
        float sum = 0.0f;
        for (int c = 0; c < numChannels; ++c) {
            auto* d = buffer.getReadPointer (c);
            for (int i = 0; i < numSamples; ++i) sum += d[i] * d[i];
        }
        float rms = (numSamples > 0) ? std::sqrt (sum / (float)(numSamples * numChannels)) : 0.0f;
        inputRmsLevel = rmsCoeff * inputRmsLevel + (1.0f - rmsCoeff) * rms;
    }

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // 1. HPF 80 Hz
    hpf.process (ctx);

    // 2. BODY — mud cut before compressor
    if (bodyAmount.load() > 0.001f)
        bodyFilter.process (ctx);

    // 3. Optical GLUE compressor (stereo-linked)
    optComp.process (buffer);

    // 4. De-esser — sidechain at 7kHz
    {
        const int n = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        juce::AudioBuffer<float> sc (numCh, n);
        for (int c = 0; c < numCh; ++c)
            sc.copyFrom (c, 0, buffer, c, 0, n);

        juce::dsp::AudioBlock<float> scBlock (sc);
        deEsserBP.process (juce::dsp::ProcessContextReplacing<float> (scBlock));

        const float thresh = juce::Decibels::decibelsToGain (-18.0f);
        for (int c = 0; c < numCh; ++c)
        {
            auto* out = buffer.getWritePointer (c);
            const auto* scData = sc.getReadPointer (c);
            for (int i = 0; i < n; ++i)
            {
                float scLevel = std::abs (scData[i]);
                if (scLevel > thresh)
                    out[i] *= (0.7f + 0.3f * (thresh / scLevel));
            }
        }
    }

    // 5. PRESENCE boost (post-comp)
    if (presenceAmount.load() > 0.001f)
        presenceFilter.process (ctx);

    // 6. PULTEC AIR — resonant peak + shelf
    if (airAmount.load() > 0.001f)
    {
        pultecPeak .process (ctx);
        pultecShelf.process (ctx);
    }

    // 7. WIDTH — M/S processing
    //    Encode → HPF side → air on side → scale side → decode
    if (buffer.getNumChannels() >= 2)
    {
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);
        const int n = buffer.getNumSamples();
        const float w = widthAmount.load();

        for (int i = 0; i < n; ++i)
        {
            float mid  = (L[i] + R[i]) * 0.5f;
            float side = (L[i] - R[i]) * 0.5f;

            // Remove sub-frequencies from stereo field (keep bass centred)
            side = sideHPF.processSample (side);

            // Add air to stereo field only when width > 0
            if (w > 0.001f)
                side = sideAir.processSample (side);

            // Scale: width 0 = unchanged, width 1 = +50% side (wider)
            side *= (1.0f + w * 0.5f);

            L[i] = mid + side;
            R[i] = mid - side;
        }
    }

    // 8. EDGE — HEQ style: asymmetric poly (even harmonics) + 10kHz shelf
    if (edgeAmount.load() > 0.001f)
    {
        const float drive = edgeAmount.load() * 3.0f;
        const int n = buffer.getNumSamples();

        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            auto* data = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                float x = data[i];
                // Asymmetric soft clipper: positive half stretched, negative compressed
                // Generates predominantly 2nd harmonic (even-order)
                float out;
                if (x >= 0.0f)
                    out = 1.0f - std::exp (-x * (1.0f + drive));
                else
                    out = -(1.0f - std::exp (x * (1.0f + drive * 0.5f)));

                // Normalize to prevent level increase
                float norm = 1.0f - std::exp (-(1.0f + drive));
                out /= (norm + 1e-6f);

                // Wet/dry blend — keep it as a spice
                float amount = edgeAmount.load() * 0.5f; // max 50% wet
                data[i] = x * (1.0f - amount) + out * amount;
            }
        }

        // 10kHz shelf boost (the Waves HEQ trick: harmonics + air together)
        edgeShelf.process (ctx);
    }

    // Measure output RMS
    {
        float sum = 0.0f;
        for (int c = 0; c < numChannels; ++c) {
            auto* d = buffer.getReadPointer (c);
            for (int i = 0; i < numSamples; ++i) sum += d[i] * d[i];
        }
        float rms = (numSamples > 0) ? std::sqrt (sum / (float)(numSamples * numChannels)) : 0.0f;
        outputRmsLevel = rmsCoeff * outputRmsLevel + (1.0f - rmsCoeff) * rms;
    }

    // Gain compensation: output RMS = input RMS (transparent A/B comparison)
    if (gainCompEnabled.load())
    {
        float trim = (outputRmsLevel > 1e-6f) ? (inputRmsLevel / (outputRmsLevel + 1e-6f)) : 1.0f;
        trim = juce::jlimit (0.125f, 8.0f, trim);  // ±18 dB max
        gainTrimSmooth = gainTrimCoeff * gainTrimSmooth + (1.0f - gainTrimCoeff) * trim;
        buffer.applyGain (gainTrimSmooth);
    }
    else
    {
        gainTrimSmooth = 1.0f;  // reset so no jump when re-enabled
    }
}

//==============================================================================
juce::AudioProcessorEditor* FrontAudioProcessor::createEditor()
{
    return new FrontAudioProcessorEditor (*this);
}

void FrontAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FrontAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FrontAudioProcessor();
}
