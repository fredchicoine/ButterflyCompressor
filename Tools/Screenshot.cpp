// Renders the editor with live test audio running and saves docs/screenshot.png
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (argc > 1 ? argv[1] : "docs/screenshot.png");

    ButterflyAudioProcessor p;
    auto set = [&] (const char* id, float v) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
    set ("b3_src", 1.0f); set ("b3_rate", 2.0f);   // band 3: SYNC 1/16
    set ("b2_width", 108.0f);

    const double sr = 48000.0; const int bs = 256;
    p.setRateAndBufferSizeDetails (sr, bs);
    p.prepareToPlay (sr, bs);

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1120, 700);
    ed->setVisible (true);

    juce::AudioBuffer<float> buf (4, bs);
    juce::MidiBuffer midi;
    juce::Random rng (7);
    double t = 0.0, ph[6] {};
    const double notes[6] = { 110.0, 164.8, 220.0, 261.6, 329.6, 440.0 };
    float lpL = 0, lpR = 0;
    const double kickPeriod = 0.5;
    const int totalBlocks = (int) (sr * 3.06 / bs);   // stop ~60 ms after a kick

    for (int blk = 0; blk < totalBlocks; ++blk)
    {
        for (int s = 0; s < bs; ++s, t += 1.0 / sr)
        {
            float l = 0, r = 0;
            for (int i = 0; i < 6; ++i)
            {
                ph[i] += notes[i] / sr;
                const float sawL = (float) (2.0 * (ph[i] - std::floor (ph[i])) - 1.0);
                const float sawR = (float) (2.0 * (ph[i] * 1.004 - std::floor (ph[i] * 1.004)) - 1.0);
                l += sawL * 0.05f; r += sawR * 0.05f;
            }
            lpL += 0.15f * (l - lpL); lpR += 0.15f * (r - lpR);
            const float hat = (rng.nextFloat() * 2 - 1) * 0.03f;
            buf.setSample (0, s, lpL + hat + 0.6f * (l - lpL));
            buf.setSample (1, s, lpR - hat * 0.5f + 0.6f * (r - lpR));

            const double kt = std::fmod (t, kickPeriod);
            const float kick = (float) (std::sin (2 * juce::MathConstants<double>::pi * (50 + 120 * std::exp (-kt * 30)) * kt) * std::exp (-kt * 7));
            const float snap = (float) ((rng.nextFloat() * 2 - 1) * std::exp (-kt * 40) * 0.5);
            buf.setSample (2, s, kick + snap);
            buf.setSample (3, s, kick + snap);
        }
        p.processBlock (buf, midi);
        if (blk % 4 == 3)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (40);

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    out.getParentDirectory().createDirectory();
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    ed = nullptr;
    return 0;
}
