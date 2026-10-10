#include "PluginEditor.h"

using namespace smart;

static juce::Colour bandColour (int b) { return juce::Colour::fromHSV ((float) b / (float) numBands, 0.65f, 1.0f, 1.0f); }

SmartEQAudioProcessorEditor::SmartEQAudioProcessorEditor (SmartEQAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (900, 520);
    setResizable (true, true);
    setResizeLimits (640, 380, 1800, 1000);
    startTimerHz (30);
}

SmartEQAudioProcessorEditor::~SmartEQAudioProcessorEditor() { stopTimer(); }

void SmartEQAudioProcessorEditor::resized() { plot = getLocalBounds().toFloat().withTrimmedTop (34).reduced (38, 22); }

float SmartEQAudioProcessorEditor::freqToX (float f) const
{
    return plot.getX() + plot.getWidth() * std::log (f / 20.f) / std::log (1000.f);
}
float SmartEQAudioProcessorEditor::xToFreq (float x) const
{
    return 20.f * std::pow (1000.f, juce::jlimit (0.f, 1.f, (x - plot.getX()) / plot.getWidth()));
}
float SmartEQAudioProcessorEditor::dbToY (float db) const
{
    return plot.getBottom() - plot.getHeight() * (db + 24.f) / 48.f;
}
float SmartEQAudioProcessorEditor::yToDb (float y) const
{
    return juce::jlimit (-24.f, 24.f, -24.f + 48.f * (plot.getBottom() - y) / plot.getHeight());
}

float SmartEQAudioProcessorEditor::getParam (const juce::String& id) const
{
    return proc.apvts.getRawParameterValue (id)->load();
}

void SmartEQAudioProcessorEditor::setParam (const juce::String& id, float v)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
}

juce::Point<float> SmartEQAudioProcessorEditor::nodePos (int b) const
{
    const juce::String id (b);
    const int type = (int) getParam ("t" + id);
    return { freqToX (getParam ("f" + id)), dbToY (hasGain (type) ? getParam ("g" + id) : 0.f) };
}

int SmartEQAudioProcessorEditor::nodeAt (juce::Point<float> p) const
{
    int best = -1; float bestD = 14.f;
    for (int b = 0; b < numBands; ++b)
    {
        if (getParam ("on" + juce::String (b)) < 0.5f) continue;
        const float d = nodePos (b).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void SmartEQAudioProcessorEditor::timerCallback()
{
    proc.getAnalyzerSnapshot (fftData, fftSize);
    juce::FloatVectorOperations::clear (fftData + fftSize, fftSize);
    window.multiplyWithWindowingTable (fftData, (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData);

    for (int i = 1; i <= fftSize / 2; ++i)
    {
        const float f  = (float) i * (float) sr() / (float) fftSize;
        float db = juce::Decibels::gainToDecibels (fftData[i] / 512.f, -120.f) + 4.5f * std::log2 (f / 1000.f);
        spec[i] = 0.75f * spec[i] + 0.25f * db;   // smoothing
    }
    repaint();
}

void SmartEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14161b));

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (18.f, juce::Font::bold));
    g.drawText ("SMART EQ", 14, 6, 120, 24, juce::Justification::left);
    g.setColour (juce::Colour (0xff7a8294));
    g.setFont (juce::FontOptions (12.f));
    g.drawText ("PRODUCERTOY", 100, 10, 120, 20, juce::Justification::left);

    g.setColour (juce::Colour (0xff1b1e25));
    g.fillRoundedRectangle (plot, 6.f);

    // Grid
    g.setFont (juce::FontOptions (11.f));
    for (float f : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = freqToX (f);
        g.setColour (juce::Colour (0xff2a2e38));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (juce::Colour (0xff6b7385));
        g.drawText (f >= 1000.f ? juce::String (f / 1000.f) + "k" : juce::String ((int) f),
                    (int) x - 20, (int) plot.getBottom() + 3, 40, 14, juce::Justification::centred);
    }
    for (int db = -24; db <= 24; db += 6)
    {
        const float y = dbToY ((float) db);
        g.setColour (db == 0 ? juce::Colour (0xff434957) : juce::Colour (0xff2a2e38));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (juce::Colour (0xff6b7385));
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), 4, (int) y - 7, 32, 14, juce::Justification::right);
    }

    // Spectrum
    {
        juce::Path sp;
        bool started = false;
        for (float x = plot.getX(); x <= plot.getRight(); x += 2.f)
        {
            const float f = xToFreq (x);
            const int bin = juce::jlimit (1, fftSize / 2, (int) std::round (f * fftSize / (float) sr()));
            const float y = plot.getBottom() - plot.getHeight() * juce::jlimit (0.f, 1.f, (spec[bin] + 90.f) / 90.f);
            if (! started) { sp.startNewSubPath (x, y); started = true; } else sp.lineTo (x, y);
        }
        sp.lineTo (plot.getRight(), plot.getBottom());
        sp.lineTo (plot.getX(), plot.getBottom());
        sp.closeSubPath();
        g.setColour (juce::Colour (0x3c5ac8fa));
        g.fillPath (sp);
    }

    // Combined EQ curve
    SmartEQAudioProcessor::Coeffs::Ptr coeffs[numBands];
    int stages[numBands] {};
    for (int b = 0; b < numBands; ++b)
    {
        coeffs[b] = proc.makeBandCoeffs (b, sr());
        stages[b] = isTwoStage ((int) getParam ("t" + juce::String (b))) ? 2 : 1;
    }

    auto magDb = [&] (int b, double f)
    {
        if (coeffs[b] == nullptr) return 0.0;
        return (double) stages[b] * juce::Decibels::gainToDecibels (coeffs[b]->getMagnitudeForFrequency (f, sr()), -120.0);
    };

    juce::Path curve;
    for (float x = plot.getX(); x <= plot.getRight(); x += 2.f)
    {
        double total = 0.0;
        for (int b = 0; b < numBands; ++b) total += magDb (b, xToFreq (x));
        const float y = dbToY ((float) juce::jlimit (-24.0, 24.0, total));
        if (x == plot.getX()) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }

    // Selected band ka apna curve
    if (selected >= 0 && coeffs[selected] != nullptr)
    {
        juce::Path bp;
        for (float x = plot.getX(); x <= plot.getRight(); x += 2.f)
        {
            const float y = dbToY ((float) juce::jlimit (-24.0, 24.0, magDb (selected, xToFreq (x))));
            if (x == plot.getX()) bp.startNewSubPath (x, y); else bp.lineTo (x, y);
        }
        juce::Path fill (bp);
        fill.lineTo (plot.getRight(), dbToY (0.f));
        fill.lineTo (plot.getX(), dbToY (0.f));
        fill.closeSubPath();
        g.setColour (bandColour (selected).withAlpha (0.18f));
        g.fillPath (fill);
    }

    g.setColour (juce::Colours::white);
    g.strokePath (curve, juce::PathStrokeType (2.2f));

    // Nodes
    for (int b = 0; b < numBands; ++b)
    {
        if (getParam ("on" + juce::String (b)) < 0.5f) continue;
        const auto p = nodePos (b);
        const float r = (b == selected) ? 8.f : 6.5f;
        g.setColour (bandColour (b));
        g.fillEllipse (p.x - r, p.y - r, r * 2.f, r * 2.f);
        g.setColour (juce::Colour (0xff14161b));
        g.setFont (juce::FontOptions (10.f, juce::Font::bold));
        g.drawText (juce::String (b + 1), (int) (p.x - r), (int) (p.y - r), (int) (r * 2), (int) (r * 2), juce::Justification::centred);
    }

    // Info readout
    g.setFont (juce::FontOptions (13.f));
    g.setColour (juce::Colour (0xffb7bfd0));
    juce::String info = "Double-click: band add/remove   |   Drag: freq & gain   |   Wheel: Q   |   Right-click: filter type";
    if (selected >= 0 && getParam ("on" + juce::String (selected)) > 0.5f)
    {
        const juce::String id (selected);
        const int t = (int) getParam ("t" + id);
        const float f = getParam ("f" + id);
        info = "Band " + juce::String (selected + 1) + "  " + typeNames()[t] + "   "
             + (f >= 1000.f ? juce::String (f / 1000.f, 2) + " kHz" : juce::String (f, 1) + " Hz")
             + (hasGain (t) ? "   " + juce::String (getParam ("g" + id), 1) + " dB" : juce::String())
             + "   Q " + juce::String (getParam ("q" + id), 2);
    }
    g.drawText (info, 230, 8, getWidth() - 244, 20, juce::Justification::right);
}

void SmartEQAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const int b = nodeAt (e.position);
    if (b >= 0) selected = b;

    if (b >= 0 && e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        const auto names = typeNames();
        const int cur = (int) getParam ("t" + juce::String (b));
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur);
        m.showMenuAsync (juce::PopupMenu::Options(), [this, b] (int r)
        {
            if (r > 0) setParam ("t" + juce::String (b), (float) (r - 1));
        });
    }
    repaint();
}

void SmartEQAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (selected < 0 || e.mods.isPopupMenu()) return;
    const juce::String id (selected);
    if (getParam ("on" + id) < 0.5f) return;

    setParam ("f" + id, xToFreq (e.position.x));
    if (hasGain ((int) getParam ("t" + id)))
        setParam ("g" + id, yToDb (e.position.y));
}

void SmartEQAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = nodeAt (e.position);
    if (hit >= 0)
    {
        setParam ("on" + juce::String (hit), 0.f);   // band hata do
        if (selected == hit) selected = -1;
        return;
    }
    if (! plot.contains (e.position)) return;

    // Pehla free band: Bell bands pehle, phir shelves, phir cuts
    for (int b : { 2, 3, 4, 5, 1, 6, 0, 7 })
    {
        const juce::String id (b);
        if (getParam ("on" + id) < 0.5f)
        {
            setParam ("f" + id, xToFreq (e.position.x));
            if (hasGain ((int) getParam ("t" + id))) setParam ("g" + id, yToDb (e.position.y));
            setParam ("on" + id, 1.f);
            selected = b;
            break;
        }
    }
}

void SmartEQAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = nodeAt (e.position);
    if (b < 0) b = selected;
    if (b < 0 || getParam ("on" + juce::String (b)) < 0.5f) return;

    const juce::String id ("q" + juce::String (b));
    setParam (id, juce::jlimit (0.1f, 18.f, getParam (id) * std::exp (w.deltaY)));
}
