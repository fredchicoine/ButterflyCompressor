#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

/*  The Butterfly Compressor
    A sidechain-triggered stereo image spreader.

    Signal path (per sample):
      input -> [LowCut] -> band1 -> [X1] -> band2 -> [X2] -> band3 -> [HighCut]
      Everything below LowCut / above HighCut passes untouched.
      Bands are split with phase-compensated Linkwitz-Riley crossovers, so the
      bands sum back flat (allpass) when nothing is widened.

      The sidechain is split with the same crossovers. Each band's sidechain
      envelope (x Feed) drives that band's width from 100% towards its Width
      target (max 110%).
*/
class ButterflyAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int fftOrder  = 11;
    static constexpr int fftSize   = 1 << fftOrder;
    static constexpr int scopeSize = 1024;
    static constexpr int numBands  = 3;

    ButterflyAudioProcessor();
    ~ButterflyAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    //==== Metering (audio thread -> GUI) =====================================
    std::atomic<float> bandFreq[4] {};          // effective sorted crossover freqs
    std::atomic<float> bandTrigger[numBands] {}; // 0..1 sidechain trigger amount
    std::atomic<float> bandWidth[numBands] {};   // current width (1.0 = 100%)
    std::atomic<float> correlation[numBands + 1] {};   // 0 = all, 1..3 = bands
    std::atomic<int>   monitorBand { 0 };               // GUI -> audio: 0 = all, 1..3 = band
    std::atomic<bool>  sidechainActive { false };

    struct Analyser
    {
        // channel 0 = mid (out), 1 = side (in), 2 = side (out)
        std::array<std::array<float, fftSize>, 3> fifo {}, block {};
        int index = 0;
        std::atomic<bool> ready { false };

        void push (float mid, float sideIn, float sideOut) noexcept
        {
            if (index == fftSize)
            {
                if (! ready.load (std::memory_order_acquire))
                {
                    block = fifo;
                    ready.store (true, std::memory_order_release);
                }
                index = 0;
            }
            fifo[0][(size_t) index] = mid;
            fifo[1][(size_t) index] = sideIn;
            fifo[2][(size_t) index] = sideOut;
            ++index;
        }
    } analyser;

    // goniometer points per source (0 = all, 1..3 = bands), interleaved L,R
    std::array<std::array<std::atomic<float>, scopeSize * 2>, numBands + 1> scope {};
    std::atomic<int> scopeIndex { 0 };

private:
    using LR = juce::dsp::LinkwitzRileyFilter<float>;

    std::array<LR, 4> mainX, scX;
    // phase compensation: band0 -> f1,f2,f3 ; band1 -> f2,f3 ; band2 -> f3
    std::array<LR, 6> allpass;

    float env[numBands] {};
    double sLL[numBands + 1] {}, sRR[numBands + 1] {}, sLR[numBands + 1] {};
    int scopeDecim = 0;

    std::atomic<float>* pFreq[4] {};
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pOn[numBands] {};
    std::atomic<float>* pWidth[numBands] {};
    std::atomic<float>* pFeed[numBands] {};
    std::atomic<float>* pSrc[numBands] {};
    std::atomic<float>* pRate[numBands] {};
    double freePpq = 0.0;

public:
    static inline const juce::StringArray rateNames { "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1/1" };
    static double rateToBeats (int i) { return 4.0 / std::pow (2.0, 6 - i); } // quarter-note beats
private:

    void split (std::array<LR, 4>& x, float in, int ch, float* out) noexcept;
    static float allpassProcess (LR& f, int ch, float x) noexcept
    {
        float lo, hi;
        f.processSample (ch, x, lo, hi);
        return lo + hi;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ButterflyAudioProcessor)
};
