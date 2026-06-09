#include "PluginProcessor.h"
#include "PluginEditor.h"

FrontAudioProcessorEditor::FrontAudioProcessorEditor (FrontAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      bodyAtt     (p.apvts, "body",     bodyKnob),
      glueAtt     (p.apvts, "glue",     glueKnob),
      presenceAtt (p.apvts, "presence", presenceKnob),
      airAtt      (p.apvts, "air",      airKnob),
      edgeAtt     (p.apvts, "edge",     edgeKnob),
      widthAtt    (p.apvts, "width",    widthKnob),
      gainCompAtt (p.apvts, "gaincomp", gainCompButton)
{
    setupKnob (bodyKnob, bodyLabel, "BODY",
        "BODY — Taglio peak a 400 Hz (Q 1.0), da 0 a -6 dB. "
        "Rimuove le frequenze di fango prima del compressore.");

    setupKnob (glueKnob, glueLabel, "GLUE",
        "GLUE — Compressore ottico (modello Tube-Tech CL-1B). "
        "Ratio 3.5:1, attack 30 ms, release bifase 80/600 ms. "
        "Soglia da 0 a -24 dB. Release si adatta al programma.");

    setupKnob (presenceKnob, presenceLabel, "PRESENCE",
        "PRESENCE — Boost peak a 2.5 kHz (Q 1.5), da 0 a +4 dB, dopo il compressore. "
        "Aumenta la leggibilita' della voce nel mix.");

    setupKnob (airKnob, airLabel, "AIR",
        "AIR — Stile Pultec EQP-1A: peak risonante a 12 kHz (Q 0.5, +3.5 dB) "
        "piu' shelf da 8 kHz (+1.5 dB). Crea il caratteristico bump passivo dell'induttore.");

    setupKnob (edgeKnob, edgeLabel, "EDGE",
        "EDGE — Saturazione asimmetrica (armoniche pari, 2a dominante) "
        "piu' shelf a 10 kHz (+2.75 dB). Stile Waves HEQ THD. Usare con parsimonia.");

    setupKnob (widthKnob, widthLabel, "WIDTH",
        "WIDTH — Processamento M/S: HPF sul canale Side a 250 Hz (sub centrato), "
        "shelf a 8 kHz sul Side, ampiezza Side +50% a fondo scala.");

    gainCompButton.setButtonText ("GAIN COMP");
    gainCompButton.setClickingTogglesState (true);
    gainCompButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1e1e35));
    gainCompButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff6655cc));
    gainCompButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff5544aa));
    gainCompButton.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffddccff));
    addAndMakeVisible (gainCompButton);

    presetBox.addItem ("Balanced",    1);
    presetBox.addItem ("Aggressive",  2);
    presetBox.addItem ("Bus / Subtle",3);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { processor.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    descriptionLabel.setText ("Passa il mouse su una manopola per leggere la descrizione.",
                               juce::dontSendNotification);
    descriptionLabel.setJustificationType (juce::Justification::centredLeft);
    descriptionLabel.setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
    addAndMakeVisible (descriptionLabel);

    setSize (600, 240);
}

FrontAudioProcessorEditor::~FrontAudioProcessorEditor() {}

void FrontAudioProcessorEditor::setupKnob (juce::Slider& knob, juce::Label& label,
                                            const juce::String& name, const juce::String& desc)
{
    knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    knob.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xff6655cc));
    knob.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3a5e));
    knob.setColour (juce::Slider::thumbColourId,               juce::Colour (0xffccbbff));
    knob.addMouseListener (this, true);
    addAndMakeVisible (knob);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xffe0e0ff));
    addAndMakeVisible (label);

    knobInfos.push_back ({ &knob, desc });
}

void FrontAudioProcessorEditor::mouseEnter (const juce::MouseEvent& e)
{
    for (auto& info : knobInfos)
        if (e.eventComponent == info.knob || info.knob->isParentOf (e.eventComponent))
            { descriptionLabel.setText (info.desc, juce::dontSendNotification); return; }
}

void FrontAudioProcessorEditor::mouseExit (const juce::MouseEvent& e)
{
    for (auto& info : knobInfos)
        if (e.eventComponent == info.knob || info.knob->isParentOf (e.eventComponent))
            { descriptionLabel.setText ("Passa il mouse su una manopola per leggere la descrizione.",
                                        juce::dontSendNotification); return; }
}

void FrontAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff12121f));

    g.setColour (juce::Colour (0xff1e1e35));
    g.fillRect (0, 0, getWidth(), 48);

    g.setColour (juce::Colour (0xffddccff));
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("FRONT", 16, 6, 120, 22, juce::Justification::centredLeft);

    g.setFont (juce::Font (11.0f));
    g.setColour (juce::Colour (0xff7766aa));
    g.drawText ("fedireal tools  |  vocal insert", 16, 28, 300, 16, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff2a2a45));
    g.drawHorizontalLine (48, 0.0f, (float) getWidth());

    g.setColour (juce::Colour (0xff0e0e1a));
    g.fillRect (0, getHeight() - 36, getWidth(), 36);
    g.setColour (juce::Colour (0xff2a2a45));
    g.drawHorizontalLine (getHeight() - 36, 0.0f, (float) getWidth());
}

void FrontAudioProcessorEditor::resized()
{
    gainCompButton.setBounds (getWidth() / 2 - 55, 10, 110, 28);
    presetBox.setBounds      (getWidth() - 180, 10, 168, 28);

    const int knobSize = 68;
    const int numKnobs = 6;
    const int topY     = 54;
    const int spacing  = getWidth() / numKnobs;
    const int knobY    = topY + 8;
    const int labelY   = knobY - 18;

    auto place = [&] (juce::Slider& k, juce::Label& l, int i) {
        int cx = spacing * i + spacing / 2;
        l.setBounds (cx - knobSize/2, labelY, knobSize, 16);
        k.setBounds (cx - knobSize/2, knobY,  knobSize, knobSize + 18);
    };

    place (bodyKnob,     bodyLabel,     0);
    place (glueKnob,     glueLabel,     1);
    place (presenceKnob, presenceLabel, 2);
    place (airKnob,      airLabel,      3);
    place (edgeKnob,     edgeLabel,     4);
    place (widthKnob,    widthLabel,    5);

    descriptionLabel.setBounds (10, getHeight() - 33, getWidth() - 20, 30);
}
