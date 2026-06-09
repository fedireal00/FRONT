#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class FrontAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit FrontAudioProcessorEditor (FrontAudioProcessor&);
    ~FrontAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit  (const juce::MouseEvent& e) override;

private:
    FrontAudioProcessor& processor;

    juce::Slider bodyKnob, glueKnob, presenceKnob, airKnob, edgeKnob, widthKnob;
    juce::Label  bodyLabel, glueLabel, presenceLabel, airLabel, edgeLabel, widthLabel;
    juce::ComboBox   presetBox;
    juce::TextButton gainCompButton;
    juce::Label      descriptionLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    Attachment bodyAtt, glueAtt, presenceAtt, airAtt, edgeAtt, widthAtt;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    ButtonAttachment gainCompAtt;

    struct KnobInfo { juce::Slider* knob; juce::String desc; };
    std::vector<KnobInfo> knobInfos;

    void setupKnob (juce::Slider&, juce::Label&, const juce::String& name, const juce::String& desc);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrontAudioProcessorEditor)
};
