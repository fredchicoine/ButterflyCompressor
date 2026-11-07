#include "PluginProcessor.h"
#include "PluginEditor.h"

static const char* freqIds[4] = { "lowCut", "xover1", "xover2", "highCut" };

ButterflyAudioProcessor::ButterflyAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < 4; ++i)
        pFreq[i] = apvts.getRawParameterValue (freqIds[i]);

    pMode    = apvts.getRawParameterValue ("mode");
    pAttack  = apvts.getRawParameterValue ("attack");
    pRelease = apvts.getRawParameterValue ("release");
    pOutput  = apvts.getRawParameterValue ("output");
    correlation[0] = 1.0f;

    for (int b = 0; b < numBands; ++b)
    {
        auto n = juce::String (b + 1);
        pOn[b]    = apvts.getRawParameterValue ("b" + n + "_on");
        pWidth[b] = apvts.getRawParameterValue ("b" + n + "_width");
        pFeed[b]  = apvts.getRawParameterValue ("b" + n + "_feed");
        pSrc[b]   = apvts.getRawParameterValue ("b" + n + "_src");
        pRate[b]  = apvts.getRawParameterValue ("b" + n + "_rate");
        bandWidth[b] = 1.0f;
        correlation[b + 1] = 1.0f;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout ButterflyAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto freqRange = [] (float lo, float hi)
    {
        NormalisableRange<float> r (lo, hi, 1.0f);
        r.setSkewForCentre (std::sqrt (lo * hi));
        return r;
    };
    auto withUnit = [] (const String& unit, int decimals)
    {
        return AudioParameterFloatAttributes().withLabel (unit)
            .withStringFromValueFunction ([unit, decimals] (float v, int) { return String (v, decimals) + " " + unit; })
            .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); });
    };
    auto hz = AudioParameterFloatAttributes().withLabel ("Hz")
        .withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; })
        .withValueFromStringFunction ([] (const String& s) { auto v = s.getFloatValue(); return s.containsIgnoreCase ("k") ? v * 1000.0f : v; });

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lowCut", 1 },  "Low Cut",     freqRange (20.0f, 2000.0f),   120.0f,   hz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "xover1", 1 },  "Crossover 1", freqRange (40.0f, 16000.0f),  800.0f,   hz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "xover2", 1 },  "Crossover 2", freqRange (40.0f, 18000.0f),  4000.0f,  hz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "highCut", 1 }, "High Cut",    freqRange (1000.0f, 20000.0f), 12000.0f, hz));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "M/S", "L/R" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack",
                    NormalisableRange<float> (0.1f, 100.0f, 0.01f, 0.4f), 5.0f, withUnit ("ms", 1)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release",
                    NormalisableRange<float> (10.0f, 1000.0f, 0.1f, 0.4f), 150.0f, withUnit ("ms", 0)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                    NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, withUnit ("dB", 1)));

    for (int b = 1; b <= numBands; ++b)
    {
        auto n = String (b);
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "b" + n + "_on", 1 }, "Band " + n + " On", true));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "b" + n + "_width", 1 }, "Band " + n + " Width",
                        NormalisableRange<float> (0.0f, 110.0f, 0.1f), 110.0f, withUnit ("%", 1)));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "b" + n + "_feed", 1 }, "Band " + n + " Feed",
                        NormalisableRange<float> (-12.0f, 36.0f, 0.1f), 12.0f, withUnit ("dB", 1)));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "b" + n + "_src", 1 }, "Band " + n + " Trigger",
                        StringArray { "Sidechain", "Sync" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "b" + n + "_rate", 1 }, "Band " + n + " Rate",
                        rateNames, 4));
    }
    return layout;
}

bool ButterflyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()
     || layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        auto sc = layouts.getChannelSet (true, 1);
        return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
    }
    return true;
}

void ButterflyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    auto prep = [&] (LR& f) { f.prepare (spec); f.setType (juce::dsp::LinkwitzRileyFilterType::lowpass); f.reset(); };
    for (auto& f : mainX)   prep (f);
    for (auto& f : scX)     prep (f);
    for (auto& f : allpass) prep (f);

    for (auto& e : env) e = 0.0f;
    freePpq = 0.0;
    for (int i = 0; i <= numBands; ++i) { sLL[i] = sRR[i] = sLR[i] = 0.0; correlation[i] = 1.0f; }
}

void ButterflyAudioProcessor::split (std::array<LR, 4>& x, float in, int ch, float* o) noexcept
{
    float lo, hi;
    x[0].processSample (ch, in, lo, hi); o[0] = lo;
    x[1].processSample (ch, hi, lo, hi); o[1] = lo;
    x[2].processSample (ch, hi, lo, hi); o[2] = lo;
    x[3].processSample (ch, hi, o[3], o[4]);
}

void ButterflyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    const double sr = getSampleRate();

    //---- crossover frequencies (sorted, spaced, below Nyquist) --------------
    float f[4];
    for (int i = 0; i < 4; ++i) f[i] = pFreq[i]->load();
    std::sort (f, f + 4);
    const float fMax = (float) (sr * 0.45);
    f[3] = juce::jmin (f[3], fMax);
    for (int i = 1; i < 4; ++i) f[i] = juce::jmax (f[i], f[i - 1] * 1.05f);
    for (int i = 2; i >= 0; --i) f[i] = juce::jmin (f[i], f[i + 1] / 1.05f);

    for (int i = 0; i < 4; ++i)
    {
        mainX[(size_t) i].setCutoffFrequency (f[i]);
        scX[(size_t) i].setCutoffFrequency (f[i]);
        bandFreq[i] = f[i];
    }
    allpass[0].setCutoffFrequency (f[1]);
    allpass[1].setCutoffFrequency (f[2]);
    allpass[2].setCutoffFrequency (f[3]);
    allpass[3].setCutoffFrequency (f[2]);
    allpass[4].setCutoffFrequency (f[3]);
    allpass[5].setCutoffFrequency (f[3]);

    //---- parameters --------------------------------------------------------
    const bool  modeLR = pMode->load() > 0.5f;
    const float aC = (float) std::exp (-1.0 / (pAttack->load()  * 0.001 * sr));
    const float rC = (float) std::exp (-1.0 / (pRelease->load() * 0.001 * sr));
    const float outGain = juce::Decibels::decibelsToGain (pOutput->load());
    const float corrC = (float) std::exp (-1.0 / (0.3 * sr));

    //---- tempo sync (Kickstart-style pulse) ---------------------------------
    double bpm = 120.0, ppq = freePpq;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jmax (20.0, *b);
            playing = pos->getIsPlaying();
            if (playing)
                if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    const double ppqPerSample = bpm / 60.0 / sr;
    freePpq = ppq + ppqPerSample * n;   // keeps pulsing (free-running) when stopped

    bool  on[numBands], sync[numBands];
    double beats[numBands];
    float target[numBands], feed[numBands];
    for (int b = 0; b < numBands; ++b)
    {
        on[b]     = pOn[b]->load() > 0.5f;
        target[b] = pWidth[b]->load() * 0.01f;
        feed[b]   = juce::Decibels::decibelsToGain (pFeed[b]->load());
        sync[b]   = pSrc[b]->load() > 0.5f;
        beats[b]  = rateToBeats (juce::jlimit (0, 6, (int) pRate[b]->load()));
    }

    //---- buffers -----------------------------------------------------------
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
        {
            scL = sc.getReadPointer (0);
            scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL;
        }
    }
    sidechainActive = (scL != nullptr);

    float trigMax[numBands] {}, widthLast[numBands] { 1.0f, 1.0f, 1.0f };
    const int mon = juce::jlimit (0, numBands, monitorBand.load());

    for (int s = 0; s < n; ++s)
    {
        float bl[5], br[5], sb[5] {};
        split (mainX, L[s], 0, bl);
        split (mainX, R[s], 1, br);

        // phase-align lower bands with the extra crossovers the upper bands passed through
        for (int ch = 0; ch < 2; ++ch)
        {
            float* v = ch == 0 ? bl : br;
            v[0] = allpassProcess (allpass[0], ch, v[0]);
            v[0] = allpassProcess (allpass[1], ch, v[0]);
            v[0] = allpassProcess (allpass[2], ch, v[0]);
            v[1] = allpassProcess (allpass[3], ch, v[1]);
            v[1] = allpassProcess (allpass[4], ch, v[1]);
            v[2] = allpassProcess (allpass[5], ch, v[2]);
        }

        if (scL != nullptr)
            split (scX, 0.5f * (scL[s] + scR[s]), 0, sb);

        const float sideIn = 0.5f * (L[s] - R[s]);

        // untouched regions (below Low Cut / above High Cut)
        float oL = bl[0] + bl[4];
        float oR = br[0] + br[4];
        float bOutL[numBands + 1], bOutR[numBands + 1], bSideIn[numBands + 1];

        for (int b = 0; b < numBands; ++b)
        {
            float e;
            if (sync[b])
            {
                // full width on the beat, curving back to 100% before the next one
                const double pos = (ppq + ppqPerSample * s) / beats[b];
                const float ph = (float) (pos - std::floor (pos));
                const float down = 1.0f - ph;
                e = down * down;
            }
            else
            {
                e = std::abs (sb[b + 1]);
            }
            env[b] = e > env[b] ? aC * env[b] + (1.0f - aC) * e
                                : rC * env[b] + (1.0f - rC) * e;

            const float trig = on[b] ? juce::jmin (1.0f, sync[b] ? env[b] : env[b] * feed[b]) : 0.0f;
            const float w = 1.0f + (target[b] - 1.0f) * trig;

            float m  = 0.5f * (bl[b + 1] + br[b + 1]);
            float sd = 0.5f * (bl[b + 1] - br[b + 1]);
            bSideIn[b + 1] = sd;

            if (modeLR)
            {
                // level-safe: keep L/R energy constant while changing the image
                const float k = std::sqrt (2.0f / (1.0f + w * w));
                m  *= k;
                sd *= w * k;
            }
            else
            {
                sd *= w; // classic M/S: side gain only
            }

            bOutL[b + 1] = (m + sd) * outGain;
            bOutR[b + 1] = (m - sd) * outGain;
            oL += m + sd;
            oR += m - sd;

            trigMax[b]   = juce::jmax (trigMax[b], trig);
            widthLast[b] = w;
        }

        oL *= outGain;
        oR *= outGain;
        L[s] = oL;
        R[s] = oR;

        bOutL[0] = oL;
        bOutR[0] = oR;
        bSideIn[0] = sideIn;

        // metering: correlation for the full output and every band
        for (int i = 0; i <= numBands; ++i)
        {
            const double l = bOutL[i], r = bOutR[i];
            sLL[i] = corrC * sLL[i] + (1.0 - corrC) * l * l;
            sRR[i] = corrC * sRR[i] + (1.0 - corrC) * r * r;
            sLR[i] = corrC * sLR[i] + (1.0 - corrC) * l * r;
        }

        analyser.push (0.5f * (bOutL[mon] + bOutR[mon]), bSideIn[mon], 0.5f * (bOutL[mon] - bOutR[mon]));

        if (++scopeDecim >= 4)
        {
            scopeDecim = 0;
            const int idx = scopeIndex.load (std::memory_order_relaxed);
            for (int i = 0; i <= numBands; ++i)
            {
                scope[(size_t) i][(size_t) idx * 2]     .store (bOutL[i], std::memory_order_relaxed);
                scope[(size_t) i][(size_t) idx * 2 + 1] .store (bOutR[i], std::memory_order_relaxed);
            }
            scopeIndex.store ((idx + 1) % scopeSize, std::memory_order_relaxed);
        }
    }

    for (int i = 0; i <= numBands; ++i)
    {
        const double denom = std::sqrt (sLL[i] * sRR[i]);
        correlation[i] = denom > 1.0e-12 ? (float) (sLR[i] / denom) : 1.0f;
    }

    for (int b = 0; b < numBands; ++b)
    {
        bandTrigger[b] = trigMax[b];
        bandWidth[b]   = widthLast[b];
    }

    // clear any extra output channels
    for (int ch = 2; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

void ButterflyAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ButterflyAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ButterflyAudioProcessor::createEditor()
{
    return new ButterflyAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ButterflyAudioProcessor();
}
