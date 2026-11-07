#pragma once

#include "PluginProcessor.h"

namespace Rack
{
    const juce::Colour face     { 0xff1d1d1f };
    const juce::Colour faceDark { 0xff121213 };
    const juce::Colour ear      { 0xff2b2b2e };
    const juce::Colour silk     { 0xffe8e6df };   // silkscreen white
    const juce::Colour silkDim  { 0xff8d8b85 };
    const juce::Colour screen   { 0xff07090a };
    const juce::Colour grid     { 0xff1f2a26 };
    const juce::Colour ledRed   { 0xffff3b30 };
    const juce::Colour ledGreen { 0xff3dff6a };
    const juce::Colour ledAmber { 0xffffc21a };
    const juce::Colour lcd      { 0xffffb347 };
    const juce::Colour midFill  { 0xff3a4a44 };
    const juce::Colour sideIn   { 0xff6fb7c8 };
    const juce::Colour sideOut  { 0xffffffff };
    const juce::Colour band[3]  { juce::Colour (0xffffb000), juce::Colour (0xff4fe07a), juce::Colour (0xff4cb8ff) };

    inline juce::Colour sourceColour (int src) { return src == 0 ? silk : band[src - 1]; }
}

//==============================================================================
class RackLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

//==============================================================================
class IniLogo : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
};

//==============================================================================
class LedMeter : public juce::Component
{
public:
    void setValue (float v) { if (std::abs (v - value) > 0.001f) { value = v; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    float value = 0.0f;
};

//==============================================================================
class ModeSwitch : public juce::Component
{
public:
    ModeSwitch (juce::RangedAudioParameter& p, juce::String title, juce::String a, juce::String b);
    std::function<void (bool)> onChange;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    bool isLR = false;
    juce::String title, labelA, labelB;
    juce::ParameterAttachment attachment;
};

//==============================================================================
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (ButterflyAudioProcessor&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void update();

private:
    ButterflyAudioProcessor& proc;
    static constexpr int N = ButterflyAudioProcessor::fftSize;
    juce::dsp::FFT fft { ButterflyAudioProcessor::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) N, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, N * 2> work {};
    std::array<std::array<float, N / 2>, 3> mags {};
    float phase = 0.0f;
    int dragIndex = -1;

    float freqToX (float f) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    juce::RangedAudioParameter* freqParam (int i) const;
    float paramFreq (int i) const;
    int hitTestLine (float x) const;
};

//==============================================================================
class PhaseView : public juce::Component
{
public:
    explicit PhaseView (ButterflyAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    ButterflyAudioProcessor& proc;
    float smoothCorr[ButterflyAudioProcessor::numBands + 1] { 1.0f, 1.0f, 1.0f, 1.0f };
    void drawScope (juce::Graphics&, juce::Rectangle<float>, int source, float alpha);
};

//==============================================================================
class ButterflyAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ButterflyAudioProcessorEditor (ButterflyAudioProcessor&);
    ~ButterflyAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    Knob* addKnob (const juce::String& paramId, const juce::String& name, juce::Colour accent);
    static void placeKnob (Knob*, juce::Rectangle<int>);

    ButterflyAudioProcessor& proc;
    RackLookAndFeel lnf;

    IniLogo logo;
    SpectrumView spectrum;
    PhaseView phase;
    ModeSwitch modeSwitch;

    std::array<juce::ToggleButton, 4> monitorButtons;

    Knob* attack = nullptr;
    Knob* release = nullptr;
    Knob* output = nullptr;
    Knob* freqLo[3] {};
    Knob* freqHi[3] {};
    Knob* width[3] {};
    Knob* feed[3] {};
    std::array<LedMeter, 3> spreadMeters;
    std::array<std::unique_ptr<ModeSwitch>, 3> srcSwitch;
    std::array<juce::ComboBox, 3> rateBox;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 3> rateAttachment;
    void updateSyncState (int band, bool sync);

    std::array<juce::ToggleButton, 3> bandOn;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 3> bandOnAttachment;

    std::vector<std::unique_ptr<Knob>> knobs;
    juce::Label scWarning;

    // layout rects used by paint() for silkscreen boxes
    juce::Rectangle<int> globalBox, bandBox[3], displayBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ButterflyAudioProcessorEditor)
};
