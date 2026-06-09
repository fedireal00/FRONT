#pragma once
#include <JuceHeader.h>

//==============================================================================
// FRONT — Vocal Insert Plugin
// Chain: HPF(80Hz) → BODY cut → [GLUE optical comp + de-esser] → PRESENCE →
//        PULTEC AIR → WIDTH (M/S) → EDGE (HEQ poly + 10kHz shelf) → OUT
//==============================================================================

// Tube-Tech CL-1B style optical compressor
// Gain computer + smoothing following Giannoulis, Massberg & Reiss (JAES 2013)
// Soft knee 6 dB, dB-domain processing, decoupled attack/release (Eq. 3 & 5)
struct OpticalCompressor
{
    float env1       = 0.0f;   // decoupled release stage  (y1 in paper)
    float envL       = 0.0f;   // attack-smoothed GR in dB (yL in paper)
    float gainSmooth = 1.0f;   // final linear gain (extra 1ms smoother)
    float ratio      = 3.5f;
    float threshDb   = -12.0f;
    float kneeDb     = 6.0f;   // soft knee width in dB

    float attackCoeff  = 0.0f;
    float relCoeff1    = 0.0f;
    float relCoeff2    = 0.0f;
    float gainSmCoeff  = 0.0f;

    void prepare (double sampleRate)
    {
        attackCoeff  = std::exp (-1.0 / (sampleRate * 0.030)); // 30 ms
        relCoeff1    = std::exp (-1.0 / (sampleRate * 0.080)); // 80 ms fast
        relCoeff2    = std::exp (-1.0 / (sampleRate * 0.600)); // 600 ms slow tail
        gainSmCoeff  = std::exp (-1.0 / (sampleRate * 0.001)); // 1 ms final smooth
        env1 = 0.0f; envL = 0.0f; gainSmooth = 1.0f;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n  = buffer.getNumSamples();
        const int ch = buffer.getNumChannels();
        const float halfKnee = kneeDb * 0.5f;

        for (int i = 0; i < n; ++i)
        {
            // Stereo-linked peak detection
            float peak = 0.0f;
            for (int c = 0; c < ch; ++c)
                peak = std::max (peak, std::abs (buffer.getSample (c, i)));

            float xG = (peak > 1e-9f) ? juce::Decibels::gainToDecibels (peak) : -120.0f;

            // Gain computer with soft knee (Giannoulis 2013, Eq. 3)
            float above = xG - threshDb;
            float yG;
            if (2.0f * above < -kneeDb)
                yG = xG;
            else if (2.0f * std::abs (above) <= kneeDb)
                yG = xG + (1.0f / ratio - 1.0f) * (above + halfKnee) * (above + halfKnee) / (2.0f * kneeDb);
            else
                yG = threshDb + above / ratio;

            float xL = xG - yG;  // gain reduction in dB (positive = reducing)

            // Opto program-dependent release: high GR → fast, low GR → slow tail
            float blend   = std::min (1.0f, envL / 12.0f);
            float alphaR  = relCoeff1 * blend + relCoeff2 * (1.0f - blend);

            // Decoupled smoothing (Giannoulis 2013, Eq. 5)
            float smoothedR = alphaR * env1 + (1.0f - alphaR) * xL;
            env1  = std::max (xL, smoothedR);
            envL  = attackCoeff * envL + (1.0f - attackCoeff) * env1;

            float target = juce::Decibels::decibelsToGain (-envL);
            gainSmooth = gainSmCoeff * gainSmooth + (1.0f - gainSmCoeff) * target;

            for (int c = 0; c < ch; ++c)
                buffer.setSample (c, i, buffer.getSample (c, i) * gainSmooth);
        }
    }
};

//==============================================================================
class FrontAudioProcessor : public juce::AudioProcessor,
                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    FrontAudioProcessor();
    ~FrontAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 3; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    void parameterChanged (const juce::String&, float) override;

    juce::AudioProcessorValueTreeState apvts;
    int currentProgram = 0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    // --- Stereo filters (applied to both channels) ---
    using StereoFilter = juce::dsp::ProcessorDuplicator<
                             juce::dsp::IIR::Filter<float>,
                             juce::dsp::IIR::Coefficients<float>>;

    StereoFilter hpf;
    StereoFilter bodyFilter;
    StereoFilter presenceFilter;
    StereoFilter pultecPeak;    // Pultec: resonant peak at fc
    StereoFilter pultecShelf;   // Pultec: gentle shelf starting below fc
    StereoFilter deEsserBP;
    StereoFilter edgeShelf;     // HEQ: 10kHz shelf applied after polynomial

    // --- Mono filters for M/S side channel ---
    juce::dsp::IIR::Filter<float> sideHPF;   // cut low-freq stereo content
    juce::dsp::IIR::Filter<float> sideAir;   // add air to stereo field only

    // --- Optical compressor ---
    OpticalCompressor optComp;

    // --- Parameter cache ---
    std::atomic<float> bodyAmount    { 0.0f };
    std::atomic<float> glueAmount    { 0.3f };
    std::atomic<float> presenceAmount{ 0.0f };
    std::atomic<float> airAmount     { 0.0f };
    std::atomic<float> edgeAmount    { 0.0f };
    std::atomic<float> widthAmount   { 0.0f };

    std::atomic<bool> paramsDirty    { true };
    std::atomic<bool> gainCompEnabled { false };
    double currentSampleRate = 44100.0;

    float inputRmsLevel  { 0.0f };
    float outputRmsLevel { 0.0f };
    float rmsCoeff       { 0.9f };
    float gainTrimSmooth { 1.0f };
    float gainTrimCoeff  { 0.99f };

    void updateFilters();
    void updateCompressor();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrontAudioProcessor)
};
