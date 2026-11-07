#include "PluginEditor.h"

using namespace Rack;

static const char* freqIds[4] = { "lowCut", "xover1", "xover2", "highCut" };
static constexpr float minHz = 20.0f, maxHz = 20000.0f, minDb = -90.0f, maxDb = 0.0f;
static constexpr int earW = 36;

static juce::Point<float> polar (juce::Point<float> c, float r, float a)
{
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

static void drawLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool on)
{
    if (on)
    {
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (r.expanded (3.0f));
    }
    g.setColour (on ? c : c.darker (2.6f).withSaturation (0.4f));
    g.fillEllipse (r);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.6f : 0.12f));
    g.fillEllipse (r.reduced (r.getWidth() * 0.3f).translated (-r.getWidth() * 0.12f, -r.getHeight() * 0.12f));
}

//==============================================================================
RackLookAndFeel::RackLookAndFeel()
{
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::black);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0a0806));
    setColour (juce::Slider::textBoxTextColourId,       lcd);
    setColour (juce::Slider::textBoxHighlightColourId,  lcd.withAlpha (0.3f));
    setColour (juce::Label::textColourId,               silk);
    setColour (juce::ToggleButton::textColourId,        silk);
    setColour (juce::TextEditor::backgroundColourId,    juce::Colour (0xff0a0806));
    setColour (juce::TextEditor::textColourId,          lcd);
    setColour (juce::CaretComponent::caretColourId,     lcd);
    setColour (juce::ComboBox::backgroundColourId,      juce::Colour (0xff0a0806));
    setColour (juce::ComboBox::textColourId,            lcd);
    setColour (juce::ComboBox::outlineColourId,         juce::Colours::black);
    setColour (juce::ComboBox::arrowColourId,           lcd);
    setColour (juce::PopupMenu::backgroundColourId,     juce::Colour (0xff141414));
    setColour (juce::PopupMenu::textColourId,           silk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, lcd.withAlpha (0.3f));
}

void RackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float start, float end, juce::Slider& s)
{
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 7.0f;
    if (r < 6.0f) return;
    const auto c = area.getCentre();

    // silkscreen scale ticks
    g.setColour (silk.withAlpha (0.85f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f;
        const bool major = (i % 5 == 0);
        g.drawLine (juce::Line<float> (polar (c, r + 2.0f, a), polar (c, r + (major ? 7.0f : 5.0f), a)), major ? 1.6f : 1.0f);
    }

    // knob skirt (black, ribbed)
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c.translated (0.0f, 2.5f)));
    juce::ColourGradient skirt (juce::Colour (0xff3a3a3c), c.x, c.y - r, juce::Colour (0xff070707), c.x, c.y + r, false);
    g.setGradientFill (skirt);
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int i = 0; i < 36; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine (juce::Line<float> (polar (c, r * 0.72f, a), polar (c, r * 0.98f, a)), 1.0f);
    }

    // brushed aluminium cap
    const float rc = r * 0.62f;
    juce::ColourGradient cap (juce::Colour (0xffe9e9e6), c.x - rc, c.y - rc, juce::Colour (0xff7d7d7b), c.x + rc, c.y + rc, false);
    g.setGradientFill (cap);
    g.fillEllipse (juce::Rectangle<float> (rc * 2.0f, rc * 2.0f).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (juce::Rectangle<float> (rc * 2.0f, rc * 2.0f).withCentre (c), 1.0f);

    // pointer: white on skirt, black on cap, accent dot at tip
    const float a = start + pos * (end - start);
    g.setColour (silk);
    g.drawLine (juce::Line<float> (polar (c, rc, a), polar (c, r * 0.95f, a)), 2.4f);
    g.setColour (juce::Colour (0xff111111));
    g.drawLine (juce::Line<float> (polar (c, rc * 0.25f, a), polar (c, rc * 0.92f, a)), 2.0f);

    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    g.setColour (accent);
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (polar (c, r * 0.84f, a)));
}

void RackLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto ledColour = b.findColour (juce::ToggleButton::tickColourId);

    // square hardware push button
    auto btn = r.removeFromLeft (juce::jmin (r.getHeight(), 30.0f));
    juce::ColourGradient grad (juce::Colour (down ? 0xff2a2a2a : 0xff505052), btn.getX(), btn.getY(),
                               juce::Colour (0xff1a1a1a), btn.getX(), btn.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (btn, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (btn, 3.0f, 1.2f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (btn, 3.0f);
    }
    drawLed (g, juce::Rectangle<float> (9.0f, 9.0f).withCentre (btn.getCentre()), ledColour, on);

    g.setColour (silk);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
}

//==============================================================================
void IniLogo::paint (juce::Graphics& g)
{
    // vector trace of the iNi logo (original artwork 1282 x 844)
    juce::Path p;
    p.startNewSubPath (14, 14);  p.lineTo (356, 14);  p.lineTo (356, 116); p.lineTo (128, 116);
    p.lineTo (128, 728); p.lineTo (1154, 728); p.lineTo (1154, 116); p.lineTo (926, 116);
    p.lineTo (926, 14);  p.lineTo (1268, 14); p.lineTo (1268, 830); p.lineTo (14, 830); p.closeSubPath();
    p.addRectangle (242, 218, 114, 408);                 // i
    p.addRectangle (470, 14, 114, 612);                  // N left stem
    p.addRectangle (698, 14, 114, 612);                  // N right stem
    p.startNewSubPath (584, 14); p.lineTo (698, 312);    // N diagonal
    p.lineTo (698, 626); p.lineTo (584, 320); p.closeSubPath();
    p.addRectangle (926, 218, 114, 408);                 // i

    p.applyTransform (p.getTransformToScaleToFit (getLocalBounds().toFloat(), true));
    g.setColour (silk);
    g.fillPath (p);
}

//==============================================================================
void LedMeter::paint (juce::Graphics& g)
{
    constexpr int n = 12;
    auto r = getLocalBounds().toFloat();
    const float segW = r.getWidth() / (float) n;
    const int lit = juce::roundToInt (value * n);
    for (int i = 0; i < n; ++i)
    {
        const auto c = i < 8 ? ledGreen : (i < 10 ? ledAmber : ledRed);
        auto seg = juce::Rectangle<float> (r.getX() + segW * (float) i, r.getY(), segW, r.getHeight()).reduced (1.5f, 0.0f);
        g.setColour (i < lit ? c : c.darker (3.0f).withSaturation (0.35f));
        g.fillRect (seg);
        if (i < lit)
        {
            g.setColour (c.withAlpha (0.25f));
            g.fillRect (seg.expanded (1.0f, 2.0f));
        }
    }
}

//==============================================================================
ModeSwitch::ModeSwitch (juce::RangedAudioParameter& p, juce::String t, juce::String a, juce::String b)
    : title (std::move (t)), labelA (std::move (a)), labelB (std::move (b)),
      attachment (p, [this] (float v) { isLR = v > 0.5f; repaint(); if (onChange) onChange (isLR); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ModeSwitch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (silk);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (title, r.removeFromTop (14.0f), juce::Justification::centred);

    auto half = r.getWidth() / 2.0f;
    auto ms = r.removeFromLeft (half), lr = r;
    for (int i = 0; i < 2; ++i)
    {
        auto cell = i == 0 ? ms : lr;
        const bool on = (i == 1) == isLR;
        drawLed (g, juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ cell.getCentreX(), cell.getY() + 8.0f }), ledRed, on);
        g.setColour (on ? silk : silkDim);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (i == 0 ? labelA : labelB, cell.withTrimmedTop (16.0f), juce::Justification::centredTop);
    }
}

void ModeSwitch::mouseDown (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (isLR ? 0.0f : 1.0f);
}

//==============================================================================
SpectrumView::SpectrumView (ButterflyAudioProcessor& p) : proc (p)
{
    for (auto& m : mags) m.fill (minDb);
}

float SpectrumView::freqToX (float f) const
{
    f = juce::jlimit (minHz, maxHz, f);
    return (float) getWidth() * std::log (f / minHz) / std::log (maxHz / minHz);
}

float SpectrumView::xToFreq (float x) const
{
    return minHz * std::pow (maxHz / minHz, juce::jlimit (0.0f, 1.0f, x / (float) getWidth()));
}

float SpectrumView::dbToY (float db) const
{
    return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, (float) getHeight(), 0.0f);
}

juce::RangedAudioParameter* SpectrumView::freqParam (int i) const { return proc.apvts.getParameter (freqIds[i]); }
float SpectrumView::paramFreq (int i) const { return proc.apvts.getRawParameterValue (freqIds[i])->load(); }

int SpectrumView::hitTestLine (float x) const
{
    int best = -1;
    float bestD = 8.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float d = std::abs (freqToX (paramFreq (i)) - x);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void SpectrumView::update()
{
    phase += 0.05f;

    if (proc.analyser.ready.load (std::memory_order_acquire))
    {
        for (size_t c = 0; c < 3; ++c)
        {
            std::fill (work.begin(), work.end(), 0.0f);
            std::copy (proc.analyser.block[c].begin(), proc.analyser.block[c].end(), work.begin());
            window.multiplyWithWindowingTable (work.data(), (size_t) N);
            fft.performFrequencyOnlyForwardTransform (work.data());

            for (size_t i = 0; i < (size_t) N / 2; ++i)
            {
                const float db = juce::Decibels::gainToDecibels (work[i] / (float) (N / 4), minDb);
                mags[c][i] = juce::jmax (db, mags[c][i] - 1.2f);
            }
        }
        proc.analyser.ready.store (false, std::memory_order_release);
    }
    else
    {
        for (auto& m : mags)
            for (auto& v : m) v = juce::jmax (minDb, v - 0.6f);
    }
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (screen);
    g.fillRect (bounds);

    const float h = bounds.getHeight();
    const int mon = proc.monitorBand.load();

    // grid
    g.setFont (juce::FontOptions (10.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = freqToX (f);
        g.setColour (grid);
        g.drawVerticalLine ((int) x, 0.0f, h);
        g.setColour (silkDim);
        g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                    (int) x + 3, (int) h - 15, 40, 13, juce::Justification::left);
    }
    for (float db = -80.0f; db < 0.0f; db += 20.0f)
    {
        g.setColour (grid);
        g.drawHorizontalLine ((int) dbToY (db), 0.0f, bounds.getWidth());
        g.setColour (silkDim.withAlpha (0.6f));
        g.drawText (juce::String ((int) db), 3, (int) dbToY (db) - 12, 30, 11, juce::Justification::left);
    }

    // effective band regions
    float f[4];
    for (int i = 0; i < 4; ++i)
    {
        f[i] = proc.bandFreq[i].load();
        if (f[i] <= 0.0f) f[i] = paramFreq (i);
    }

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float> (0.0f, 0.0f, freqToX (f[0]), h));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (freqToX (f[3]), 0.0f, bounds.getWidth(), h));

    for (int b = 0; b < 3; ++b)
    {
        const bool on = proc.apvts.getRawParameterValue ("b" + juce::String (b + 1) + "_on")->load() > 0.5f;
        const bool focused = (mon == 0 || mon == b + 1);
        const float x0 = freqToX (f[b]), x1 = freqToX (f[b + 1]);
        const float trig = proc.bandTrigger[b].load();
        const float pulse = 0.5f + 0.5f * std::sin (phase * 3.0f + (float) b);

        auto area = juce::Rectangle<float>::leftTopRightBottom (x0, 0.0f, x1, h);
        if (on && focused)
        {
            juce::ColourGradient grad (band[b].withAlpha (0.04f + 0.3f * trig * (0.7f + 0.3f * pulse)), area.getCentreX(), h,
                                       band[b].withAlpha (0.01f), area.getCentreX(), 0.0f, false);
            g.setGradientFill (grad);
            g.fillRect (area);
        }
        if (! focused)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (area);
        }

        const float wPct = proc.bandWidth[b].load() * 100.0f;
        g.setColour (on ? band[b].withAlpha (focused ? 0.65f + 0.35f * trig : 0.35f) : silkDim);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        const auto n = juce::String (b + 1);
        const bool sync = proc.apvts.getRawParameterValue ("b" + n + "_src")->load() > 0.5f;
        const auto src = sync ? "  SYNC " + ButterflyAudioProcessor::rateNames[(int) proc.apvts.getRawParameterValue ("b" + n + "_rate")->load()] : juce::String();
        g.drawText ("B" + n + (on ? "  " + juce::String (wPct, 1) + "%" + src : "  OFF"),
                    area.reduced (2.0f, 0.0f).withTrimmedTop (50.0f).removeFromTop (16.0f).toNearestInt(), juce::Justification::centred);
    }

    // spectra (monitored source)
    auto makePath = [&] (const std::array<float, N / 2>& m, bool closed)
    {
        juce::Path p;
        const float binHz = (float) proc.getSampleRate() / (float) N;
        bool started = false;
        float lastX = -10.0f;
        for (size_t i = 1; i < m.size(); ++i)
        {
            const float fr = (float) i * binHz;
            if (fr < minHz || fr > maxHz) continue;
            const float x = freqToX (fr);
            if (x - lastX < 1.0f) continue;
            lastX = x;
            const float y = dbToY (m[i]);
            if (! started) { p.startNewSubPath (x, closed ? h : y); if (closed) p.lineTo (x, y); started = true; }
            else p.lineTo (x, y);
        }
        if (closed && started) { p.lineTo (lastX, h); p.closeSubPath(); }
        return p;
    };

    const auto outCol = mon == 0 ? sideOut : band[mon - 1];
    g.setColour (midFill.withAlpha (0.7f));
    g.fillPath (makePath (mags[0], true));
    g.setColour (sideIn.withAlpha (0.75f));
    g.strokePath (makePath (mags[1], false), juce::PathStrokeType (1.1f));
    g.setColour (outCol);
    g.strokePath (makePath (mags[2], false), juce::PathStrokeType (1.8f));

    // draggable frequency handles
    static const char* names[4] = { "LOW CUT", "X1", "X2", "HIGH CUT" };
    for (int i = 0; i < 4; ++i)
    {
        const float x = freqToX (paramFreq (i));
        const bool edge = (i == 0 || i == 3);
        g.setColour ((dragIndex == i ? lcd : silk).withAlpha (edge ? 0.9f : 0.6f));
        g.drawLine (x, 24.0f, x, h, edge ? 2.0f : 1.0f);
        g.fillRect (x - 4.0f, 24.0f, 8.0f, 8.0f);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText (names[i], (int) x - 30, 34, 60, 11, juce::Justification::centred);
    }

    // legend
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    auto leg = juce::Rectangle<int> (getWidth() - 300, getHeight() - 32, 296, 14);
    g.setColour (lcd);       g.drawText (mon == 0 ? "MONITOR: ALL" : "MONITOR: BAND " + juce::String (mon), leg.removeFromLeft (110), juce::Justification::left);
    g.setColour (midFill.brighter (0.8f)); g.drawText ("MID",      leg.removeFromLeft (40), juce::Justification::left);
    g.setColour (sideIn);    g.drawText ("SIDE IN",  leg.removeFromLeft (64), juce::Justification::left);
    g.setColour (outCol);    g.drawText ("SIDE OUT", leg.removeFromLeft (70), juce::Justification::left);

    // bezel
    g.setColour (juce::Colours::black);
    g.drawRect (bounds, 2.0f);
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (hitTestLine ((float) e.x) >= 0 ? juce::MouseCursor::LeftRightResizeCursor
                                                   : juce::MouseCursor::NormalCursor);
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    dragIndex = hitTestLine ((float) e.x);
    if (dragIndex >= 0)
        freqParam (dragIndex)->beginChangeGesture();
}

void SpectrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragIndex < 0) return;

    float fr = xToFreq ((float) e.x);
    if (dragIndex > 0) fr = juce::jmax (fr, paramFreq (dragIndex - 1) * 1.05f);
    if (dragIndex < 3) fr = juce::jmin (fr, paramFreq (dragIndex + 1) / 1.05f);

    auto* p = freqParam (dragIndex);
    p->setValueNotifyingHost (p->convertTo0to1 (fr));
}

void SpectrumView::mouseUp (const juce::MouseEvent&)
{
    if (dragIndex >= 0)
        freqParam (dragIndex)->endChangeGesture();
    dragIndex = -1;
}

//==============================================================================
void PhaseView::drawScope (juce::Graphics& g, juce::Rectangle<float> area, int src, float alpha)
{
    const auto c = area.getCentre();
    const float r = area.getWidth() * 0.5f;
    const int head = proc.scopeIndex.load (std::memory_order_relaxed);
    const auto col = sourceColour (src);
    const auto& pts = proc.scope[(size_t) src];

    for (int i = 0; i < ButterflyAudioProcessor::scopeSize; ++i)
    {
        const int idx = (head + i) % ButterflyAudioProcessor::scopeSize;
        const float l  = pts[(size_t) idx * 2].load (std::memory_order_relaxed);
        const float rr = pts[(size_t) idx * 2 + 1].load (std::memory_order_relaxed);
        const float x = juce::jlimit (-1.0f, 1.0f, (rr - l) * 0.707f);
        const float y = juce::jlimit (-1.0f, 1.0f, (l + rr) * 0.707f);
        const float age = (float) i / (float) ButterflyAudioProcessor::scopeSize;
        g.setColour (col.withAlpha (alpha * (0.1f + 0.8f * age)));
        g.fillRect (c.x + x * r, c.y - y * r, 1.6f, 1.6f);
    }
}

void PhaseView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (screen);
    g.fillRect (bounds);

    const int mon = proc.monitorBand.load();

    auto meters = bounds.removeFromBottom (92.0f).reduced (8.0f, 4.0f);
    auto scopeArea = bounds.reduced (10.0f);
    const float size = juce::jmin (scopeArea.getWidth(), scopeArea.getHeight());
    scopeArea = scopeArea.withSizeKeepingCentre (size, size);
    const auto c = scopeArea.getCentre();
    const float r = size * 0.5f;

    // goniometer graticule
    g.setColour (grid.brighter (0.4f));
    g.drawEllipse (scopeArea, 1.0f);
    g.drawLine (c.x, c.y - r, c.x, c.y + r);
    g.drawLine (c.x - r, c.y, c.x + r, c.y);
    g.drawLine (c.x - r * 0.707f, c.y - r * 0.707f, c.x + r * 0.707f, c.y + r * 0.707f);
    g.drawLine (c.x + r * 0.707f, c.y - r * 0.707f, c.x - r * 0.707f, c.y + r * 0.707f);
    g.setColour (silkDim);
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    g.drawText ("M", juce::Rectangle<float> (c.x - 10, c.y - r + 2, 20, 11), juce::Justification::centred);
    g.drawText ("L", juce::Rectangle<float> (c.x - r * 0.707f - 12, c.y - r * 0.707f - 12, 12, 12), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (c.x + r * 0.707f, c.y - r * 0.707f - 12, 12, 12), juce::Justification::centred);

    if (mon == 0)
    {
        for (int b = 1; b <= ButterflyAudioProcessor::numBands; ++b)
            drawScope (g, scopeArea, b, 0.8f);  // all bands overlaid in their colours
    }
    else
    {
        drawScope (g, scopeArea, mon, 1.0f);
    }

    // correlation meters: ALL + every band, always visible
    static const char* names[4] = { "ALL", "B1", "B2", "B3" };
    const float rowH = meters.getHeight() / 4.0f;
    for (int i = 0; i <= ButterflyAudioProcessor::numBands; ++i)
    {
        smoothCorr[i] += 0.25f * (proc.correlation[i].load() - smoothCorr[i]);
        const float v = smoothCorr[i];
        auto row = meters.removeFromTop (rowH).reduced (0.0f, 2.0f);

        g.setColour (i == mon ? lcd : sourceColour (i));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (names[i], row.removeFromLeft (26.0f), juce::Justification::centredLeft);
        auto val = row.removeFromRight (34.0f);

        g.setColour (juce::Colour (0xff141414));
        g.fillRect (row);
        const float zeroX = row.getCentreX();
        const float valX  = juce::jmap (juce::jlimit (-1.0f, 1.0f, v), -1.0f, 1.0f, row.getX(), row.getRight());
        const auto col = v < 0.0f ? ledRed : (v < 0.3f ? ledAmber : ledGreen);
        g.setColour (col);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zeroX, valX), row.getY() + 2, juce::jmax (zeroX, valX), row.getBottom() - 2));
        g.setColour (silk.withAlpha (0.6f));
        g.drawVerticalLine ((int) zeroX, row.getY(), row.getBottom());

        g.setColour (col);
        g.drawText (juce::String (v, 2), val, juce::Justification::centredRight);
    }

    g.setColour (juce::Colours::black);
    g.drawRect (getLocalBounds().toFloat(), 2.0f);
}

//==============================================================================
ButterflyAudioProcessorEditor::ButterflyAudioProcessorEditor (ButterflyAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p), phase (p),
      modeSwitch (*p.apvts.getParameter ("mode"), "STEREO MODE", "M/S", "L/R")
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (logo);
    addAndMakeVisible (spectrum);
    addAndMakeVisible (phase);
    addAndMakeVisible (modeSwitch);

    static const char* monNames[4] = { "ALL", "B1", "B2", "B3" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = monitorButtons[(size_t) i];
        b.setButtonText (monNames[i]);
        b.setRadioGroupId (4242);
        b.setClickingTogglesState (true);
        b.setColour (juce::ToggleButton::tickColourId, i == 0 ? ledRed : band[i - 1]);
        b.setToggleState (proc.monitorBand.load() == i, juce::dontSendNotification);
        b.onClick = [this, i] { if (monitorButtons[(size_t) i].getToggleState()) proc.monitorBand = i; };
        addAndMakeVisible (b);
    }

    attack  = addKnob ("attack",  "ATTACK",  silk);
    release = addKnob ("release", "RELEASE", silk);
    output  = addKnob ("output",  "OUTPUT",  silk);

    for (int b = 0; b < 3; ++b)
    {
        auto n = juce::String (b + 1);
        freqLo[b] = addKnob (freqIds[b],     b == 0 ? "LOW CUT" : "FROM", band[b]);
        freqHi[b] = addKnob (freqIds[b + 1], b == 2 ? "HIGH CUT" : "TO", band[b]);
        width[b]  = addKnob ("b" + n + "_width", "WIDTH", band[b]);
        feed[b]   = addKnob ("b" + n + "_feed",  "FEED", band[b]);

        bandOn[(size_t) b].setButtonText ("IN");
        bandOn[(size_t) b].setColour (juce::ToggleButton::tickColourId, ledRed);
        addAndMakeVisible (bandOn[(size_t) b]);
        bandOnAttachment[(size_t) b] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "b" + n + "_on", bandOn[(size_t) b]);

        addAndMakeVisible (spreadMeters[(size_t) b]);

        auto& rb = rateBox[(size_t) b];
        rb.addItemList (ButterflyAudioProcessor::rateNames, 1);
        rb.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (rb);
        rateAttachment[(size_t) b] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "b" + n + "_rate", rb);

        srcSwitch[(size_t) b] = std::make_unique<ModeSwitch> (*proc.apvts.getParameter ("b" + n + "_src"), "TRIGGER", "SC", "SYNC");
        srcSwitch[(size_t) b]->onChange = [this, b] (bool s) { updateSyncState (b, s); };
        addAndMakeVisible (*srcSwitch[(size_t) b]);
        updateSyncState (b, proc.apvts.getRawParameterValue ("b" + n + "_src")->load() > 0.5f);
    }

    scWarning.setText ("NO SIDECHAIN", juce::dontSendNotification);
    scWarning.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    scWarning.setColour (juce::Label::textColourId, ledAmber);
    scWarning.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (scWarning);

    setResizable (true, true);
    setResizeLimits (1000, 660, 1800, 1200);
    setSize (1120, 700);
    startTimerHz (30);
}

ButterflyAudioProcessorEditor::~ButterflyAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

ButterflyAudioProcessorEditor::Knob* ButterflyAudioProcessorEditor::addKnob (const juce::String& id, const juce::String& name, juce::Colour accent)
{
    auto k = std::make_unique<Knob>();
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    k->slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff050505));
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0a0806));
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

void ButterflyAudioProcessorEditor::placeKnob (Knob* k, juce::Rectangle<int> r)
{
    k->label.setBounds (r.removeFromTop (14));
    k->slider.setBounds (r);
}

static void drawScrew (juce::Graphics& g, juce::Point<float> c)
{
    const float r = 7.0f;
    juce::ColourGradient grad (juce::Colour (0xffd0d0cc), c.x - r, c.y - r, juce::Colour (0xff555553), c.x + r, c.y + r, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
    g.drawLine (c.x - r * 0.65f, c.y + r * 0.3f, c.x + r * 0.65f, c.y - r * 0.3f, 2.0f);
}

static void drawSilkBox (juce::Graphics& g, juce::Rectangle<int> box, const juce::String& title, juce::Colour titleCol)
{
    auto r = box.toFloat();
    g.setColour (silk.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), title) + 14.0f;
    auto t = juce::Rectangle<float> (tw, 14.0f).withCentre ({ r.getCentreX(), r.getY() });
    g.setColour (face);
    g.fillRect (t);
    g.setColour (titleCol);
    g.drawText (title, t, juce::Justification::centred);
}

void ButterflyAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto all = getLocalBounds();

    // rack ears
    g.setColour (ear);
    g.fillRect (all);
    juce::ColourGradient earShade (juce::Colour (0xff3a3a3d), 0, 0, juce::Colour (0xff1a1a1c), (float) earW, 0, false);
    g.setGradientFill (earShade);
    g.fillRect (all.withWidth (earW));
    g.fillRect (all.withLeft (getWidth() - earW));
    for (float y : { 34.0f, (float) getHeight() - 34.0f })
    {
        drawScrew (g, { earW * 0.5f, y });
        drawScrew (g, { (float) getWidth() - earW * 0.5f, y });
    }

    // brushed black front panel
    auto panel = all.reduced (earW, 0);
    g.setColour (face);
    g.fillRect (panel);
    juce::Random rng (1234);
    for (int y = panel.getY(); y < panel.getBottom(); y += 2)
    {
        g.setColour (juce::Colours::white.withAlpha (0.008f + 0.012f * rng.nextFloat()));
        g.drawHorizontalLine (y, (float) panel.getX(), (float) panel.getRight());
    }
    g.setColour (juce::Colours::black);
    g.drawVerticalLine (panel.getX(), 0.0f, (float) getHeight());
    g.drawVerticalLine (panel.getRight() - 1, 0.0f, (float) getHeight());
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine (1, (float) panel.getX(), (float) panel.getRight());

    // title silkscreen
    auto header = panel.reduced (16, 0).removeFromTop (70);
    header.removeFromLeft (logo.getWidth() + 14);
    g.setColour (silk);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("BUTTERFLY", header.removeFromTop (44).withTrimmedTop (12), juce::Justification::bottomLeft);
    g.setColour (silkDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("SIDECHAIN STEREO IMAGE SPREADER  /  COMPRESSOR", header.removeFromTop (16), juce::Justification::centredLeft);

    // section boxes
    drawSilkBox (g, displayBox, "ANALYZER", silk);
    drawSilkBox (g, globalBox, "DETECTOR / OUTPUT", silk);
    for (int b = 0; b < 3; ++b)
        drawSilkBox (g, bandBox[b], "BAND " + juce::String (b + 1), band[b]);

    // labels near selectors
    g.setColour (silk);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    auto monRect = monitorButtons[0].getBounds().getUnion (monitorButtons[3].getBounds());
    g.drawText ("MONITOR", monRect.withY (monRect.getY() - 15).withHeight (14), juce::Justification::centred);
    for (int b = 0; b < 3; ++b)
    {
        auto m = spreadMeters[(size_t) b].getBounds();
        g.setColour (silkDim);
        g.drawText ("100%", m.getX() - 34, m.getY() - 1, 32, m.getHeight() + 2, juce::Justification::centredRight);
        g.drawText ("MAX", m.getRight() + 3, m.getY() - 1, 30, m.getHeight() + 2, juce::Justification::centredLeft);

        auto rb = rateBox[(size_t) b].getBounds();
        g.setColour (silk);
        g.drawText ("RATE", rb.getX(), rb.getY() - 15, rb.getWidth(), 14, juce::Justification::centred);
    }
}

void ButterflyAudioProcessorEditor::resized()
{
    auto panel = getLocalBounds().reduced (earW, 0).reduced (16, 8);

    // header: logo + title | NO SC | monitor | mode
    auto header = panel.removeFromTop (62);
    logo.setBounds (header.removeFromLeft (76).withSizeKeepingCentre (76, 50));
    modeSwitch.setBounds (header.removeFromRight (110).withSizeKeepingCentre (110, 46));
    header.removeFromRight (16);
    auto mon = header.removeFromRight (4 * 62).withTrimmedTop (22).withHeight (28);
    for (auto& b : monitorButtons)
        b.setBounds (mon.removeFromLeft (62).reduced (2, 0));
    scWarning.setBounds (header.removeFromRight (110).withSizeKeepingCentre (110, 20));

    panel.removeFromTop (12);

    // controls
    auto controls = panel.removeFromBottom (262);
    panel.removeFromBottom (14);

    // analyzer
    displayBox = panel;
    auto disp = panel.reduced (10, 12);
    phase.setBounds (disp.removeFromRight (250));
    disp.removeFromRight (8);
    spectrum.setBounds (disp);

    // detector / output
    globalBox = controls.removeFromLeft (220);
    {
        auto r = globalBox.reduced (8, 14);
        const int kw = r.getWidth() / 3;
        for (auto* k : { attack, release, output })
            placeKnob (k, r.removeFromLeft (kw).withTrimmedTop (36).reduced (1, 0));
    }

    controls.removeFromLeft (10);
    const int bw = (controls.getWidth() - 20) / 3;
    for (int b = 0; b < 3; ++b)
    {
        bandBox[b] = controls.removeFromLeft (bw);
        controls.removeFromLeft (10);

        auto r = bandBox[b].reduced (10, 14);
        auto top = r.removeFromTop (28);
        bandOn[(size_t) b].setBounds (top.removeFromLeft (64));
        spreadMeters[(size_t) b].setBounds (top.reduced (38, 9));
        r.removeFromTop (8);

        auto trig = r.removeFromBottom (44);
        srcSwitch[(size_t) b]->setBounds (trig.removeFromLeft (trig.getWidth() / 2).withSizeKeepingCentre (110, 44));
        rateBox[(size_t) b].setBounds (trig.withTrimmedTop (14).withSizeKeepingCentre (90, 24));
        r.removeFromBottom (6);

        const int kw = r.getWidth() / 4;
        placeKnob (freqLo[b], r.removeFromLeft (kw).reduced (1, 0));
        placeKnob (freqHi[b], r.removeFromLeft (kw).reduced (1, 0));
        placeKnob (width[b],  r.removeFromLeft (kw).reduced (1, 0));
        placeKnob (feed[b],   r.reduced (1, 0));
    }
}

void ButterflyAudioProcessorEditor::updateSyncState (int b, bool sync)
{
    // Feed only applies to the sidechain; Rate only to sync
    rateBox[(size_t) b].setEnabled (sync);
    rateBox[(size_t) b].setAlpha (sync ? 1.0f : 0.35f);
    feed[b]->slider.setEnabled (! sync);
    feed[b]->slider.setAlpha (sync ? 0.35f : 1.0f);
    feed[b]->label.setAlpha (sync ? 0.35f : 1.0f);
}

void ButterflyAudioProcessorEditor::timerCallback()
{
    spectrum.update();
    spectrum.repaint();
    phase.repaint();
    bool anySc = false;
    for (int b = 1; b <= 3; ++b)
        anySc |= proc.apvts.getRawParameterValue ("b" + juce::String (b) + "_src")->load() < 0.5f;
    scWarning.setVisible (anySc && ! proc.sidechainActive.load());

    for (int b = 0; b < 3; ++b)
    {
        // how far the band is pushed from 100% toward its width target
        const float target = proc.apvts.getRawParameterValue ("b" + juce::String (b + 1) + "_width")->load() * 0.01f;
        const float w = proc.bandWidth[b].load();
        const float span = std::abs (target - 1.0f);
        spreadMeters[(size_t) b].setValue (span > 1.0e-4f ? juce::jlimit (0.0f, 1.0f, std::abs (w - 1.0f) / span) : 0.0f);
    }
}
