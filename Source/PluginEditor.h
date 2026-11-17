// Skinnable editor. Layout follows the user's mockup (1484 x 1060 design units).
// Everything is drawn in code for now; image art can replace the paint routines later.
#pragma once
#include "Dsp/SliceSnap.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "FontMetrics.h"
#include "HelpText.h"
#include <map>
#include <tuple>

namespace ui {

inline const juce::Colour stone      { 0xff1d1923 };
inline const juce::Colour stoneLight { 0xff2f2838 };
inline const juce::Colour stoneEdge  { 0xff4a3f58 };
inline const juce::Colour gold       { 0xffd2a82a };
inline const juce::Colour goldDim    { 0xff6f5a1a };
inline const juce::Colour display    { 0xff0a2a20 };
inline const juce::Colour lime       { 0xffa8ff1c };
inline const juce::Colour parchment  { 0xffeadfb4 };
inline const juce::Colour brass      { 0xffc0923a };
inline const juce::Colour brassDark  { 0xff5d4720 };
inline const juce::Colour muted      { 0xff8a7e6a };

inline juce::Font font (float h, bool bold = true) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }

struct Skin { juce::Image btn, pad, knobBody, knobPointer, screen, iconsSidebar, iconsAction, iconsEffect, keyWhiteUp, keyWhiteDown, keyBlackUp, keyBlackDown, meterSegs, fontAtlas; std::vector<juce::Image> fontLevels; };
inline Skin& skin() { static Skin s; return s; }
// One 256 x 256 cell of an icon sheet; inner = true trims the tile frame and keeps just the picture.
inline juce::Image iconCell (const juce::Image& sheet, int i, bool inner)
{
    if (! sheet.isValid()) return {};
    auto cell = sheet.getClippedImage ({ i * 256, 0, 256, 256 });
    return inner ? cell.getClippedImage (cell.getBounds().reduced (46)) : cell;
}
constexpr float kKnobFaceCyFrac = 0.455f;   // vertical centre of the knob's top face within its sprite

// Pre-scaled copies of the large artwork. Scaling a big source image down with the high-quality filter is slow, and the window used to
// redo it on every repaint (the displays repaint 30 times a second), which kept a CPU core busy. Each piece is now scaled once, at the exact
// size in device pixels it is drawn at, and kept; repainting is then a cheap copy. Only for images that live for the whole session.
inline std::map<std::tuple<const void*, int, int, int, int, int, int>, juce::Image>& scaledImageCache()
{
    static std::map<std::tuple<const void*, int, int, int, int, int, int>, juce::Image> cache; return cache;
}
inline void clearScaledImageCache() { scaledImageCache().clear(); }
inline void drawScaledCached (juce::Graphics& g, const juce::Image& img, juce::Rectangle<int> src, juce::Rectangle<float> dst)
{
    if (! img.isValid() || src.isEmpty() || dst.isEmpty()) return;
    const float sf = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int dw = juce::jmax (1, (int) std::lround (dst.getWidth() * sf)), dh = juce::jmax (1, (int) std::lround (dst.getHeight() * sf));
    auto& cache = scaledImageCache();
    const auto key = std::make_tuple ((const void*) img.getPixelData(), src.getX(), src.getY(), src.getWidth(), src.getHeight(), dw, dh);
    auto it = cache.find (key);
    if (it == cache.end())
    {
        if (cache.size() > 400) cache.clear();
        it = cache.emplace (key, img.getClippedImage (src).rescaled (dw, dh, juce::Graphics::highResamplingQuality)).first;
    }
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);      // the copy is already the right size, so this is a straight copy
    g.drawImage (it->second, dst, juce::RectanglePlacement::stretchToFit);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
}

// Corners keep their size; edges and middle stretch. src is the region of img to use.
inline void nineSlice (juce::Graphics& g, const juce::Image& img, juce::Rectangle<int> src, juce::Rectangle<int> dst, int sc, int dc)
{
    dc = juce::jmin (dc, dst.getWidth() / 2, dst.getHeight() / 2);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    const int sx[4] = { src.getX(), src.getX() + sc, src.getRight() - sc, src.getRight() };
    const int sy[4] = { src.getY(), src.getY() + sc, src.getBottom() - sc, src.getBottom() };
    const int dx[4] = { dst.getX(), dst.getX() + dc, dst.getRight() - dc, dst.getRight() };
    const int dy[4] = { dst.getY(), dst.getY() + dc, dst.getBottom() - dc, dst.getBottom() };
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i)
            drawScaledCached (g, img, { sx[i], sy[j], sx[i + 1] - sx[i], sy[j + 1] - sy[j] }, juce::Rectangle<int> (dx[i], dy[j], dx[i + 1] - dx[i], dy[j + 1] - dy[j]).toFloat());
}


// Half-size copies of the font atlas, so small text is drawn from a close-to-final-size image.
inline void buildFontLevels()
{
    auto& sk = skin();
    if (! sk.fontAtlas.isValid() || ! sk.fontLevels.empty()) return;
    sk.fontLevels.push_back (sk.fontAtlas);
    for (int k = 1; k < 5; ++k)
    {
        const auto& p = sk.fontLevels.back();
        sk.fontLevels.push_back (p.rescaled (juce::jmax (1, p.getWidth() / 2), juce::jmax (1, p.getHeight() / 2), juce::Graphics::highResamplingQuality));
    }
}

// Draws text with the bitmap font in the current colour. fontHeight is a "points" size like juce::Font.
inline void drawText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float fontHeight, juce::Justification just)
{
    auto& sk = skin();
    if (sk.fontLevels.empty()) { g.setFont (font (fontHeight)); g.drawText (text, area, just); return; }
    const float capPx = fontHeight * 0.70f, s = capPx / (float) kFontCapHeight;
    int k = 0; while (k < 4 && s * (float) (1 << (k + 1)) <= 1.0f) ++k;
    const float ls = s * (float) (1 << k), lvl = 1.0f / (float) (1 << k);
    const auto& img = sk.fontLevels[(size_t) k];
    const float tracking = capPx * 0.14f, spaceW = capPx * 0.55f;
    auto glyph = [] (juce::juce_wchar c) -> const int* { return (c >= 32 && c <= 126) ? kFontGlyphs[c - 32] : kFontGlyphs[0]; };
    float total = 0.0f;
    for (auto c : text) { const int* gl = glyph (c); total += (c == ' ' || gl[2] == 0) ? spaceW : gl[2] * s + tracking; }
    total -= tracking;
    if (total > area.getWidth() && area.getWidth() > 6.0f && fontHeight > 4.0f)        // shrink to fit instead of running over the edges
    { drawText (g, text, area, fontHeight * std::max (0.45f, area.getWidth() / total * 0.97f), just); return; }
    float x = just.testFlags (juce::Justification::right) ? area.getRight() - total
            : just.testFlags (juce::Justification::left)  ? area.getX() : area.getCentreX() - total * 0.5f;
    const float baseline = area.getCentreY() + capPx * 0.5f;
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    for (auto c : text)
    {
        const int* gl = glyph (c);
        if (c == ' ' || gl[2] == 0) { x += spaceW; continue; }
        const float dw = gl[2] * s, dh = gl[3] * s, dy = baseline + gl[4] * s - dh;
        auto src = juce::Rectangle<int> ((int) (gl[0] * lvl), (int) (gl[1] * lvl), juce::jmax (1, (int) std::ceil (gl[2] * lvl)), juce::jmax (1, (int) std::ceil (gl[3] * lvl)));
        g.drawImage (img.getClippedImage (src), juce::Rectangle<float> (x, dy, dw, dh), juce::RectanglePlacement::stretchToFit, true);
        x += dw + tracking;
    }
    (void) ls;
}
inline void drawText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, float fontHeight, juce::Justification just)
{ drawText (g, text, area.toFloat(), fontHeight, just); }

struct LnF : juce::LookAndFeel_V4
{
    LnF()
    {
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf2181018));
        setColour (juce::TooltipWindow::textColourId, parchment);
        setColour (juce::TooltipWindow::outlineColourId, goldDim);
    }
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float cy = (float) y + h * 0.5f;
        juce::Rectangle<float> track ((float) x + 10.0f, cy - 5.0f, (float) w - 20.0f, 10.0f);
        g.setColour (juce::Colour (0xff120f16)); g.fillRoundedRectangle (track, 5.0f);
        g.setColour (goldDim); g.drawRoundedRectangle (track, 5.0f, 2.0f);
        g.setColour (goldDim.withAlpha (0.6f));
        for (int i = 0; i <= 10; ++i) g.drawVerticalLine ((int) (track.getX() + track.getWidth() * i / 10.0f), cy - 9.0f, cy - 6.0f);
        auto& sk = skin(); const float d = (float) h * 0.92f;
        if (sk.knobBody.isValid())
        {
            const float bw = (float) sk.knobBody.getWidth(), bh = (float) sk.knobBody.getHeight(), s = d / bw;
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (sk.knobBody, (int) (pos - bw * s * 0.5f), (int) (cy - bh * kKnobFaceCyFrac * s), (int) (bw * s), (int) (bh * s), 0, 0, (int) bw, (int) bh);
        }
        else { g.setColour (brass); g.fillEllipse (pos - d / 2, cy - d / 2, d, d); }
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f - 4.0f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        auto& sk = skin();
        const bool art = sk.knobBody.isValid();
        const float rim = art ? r * 0.88f : r;
        juce::Path track, val;
        track.addCentredArc (c.x, c.y, rim + 5.0f, rim + 5.0f, 0.0f, a0, a1, true);
        val.addCentredArc (c.x, c.y, rim + 5.0f, rim + 5.0f, 0.0f, a0, a0 + pos * (a1 - a0), true);
        g.setColour (juce::Colour (0xff000000).withAlpha (0.55f)); g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (lime);  g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (art)
        {
            const float bw = (float) sk.knobBody.getWidth(), bh = (float) sk.knobBody.getHeight();
            const float s = rim / (bw * 0.5f);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (sk.knobBody, (int) (c.x - bw * s * 0.5f), (int) (c.y - bh * kKnobFaceCyFrac * s), (int) (bw * s), (int) (bh * s), 0, 0, (int) bw, (int) bh);
            if (sk.knobPointer.isValid())
            {
                const float gw = (float) sk.knobPointer.getWidth(), gh = (float) sk.knobPointer.getHeight();
                const float gs = (rim * 0.80f) / gh, d = rim * 0.44f;
                g.drawImageTransformed (sk.knobPointer, juce::AffineTransform::translation (-gw * 0.5f, -gh * 0.5f).scaled (gs)
                                         .rotated (ang).translated (c.x + std::sin (ang) * d, c.y - std::cos (ang) * d));
            }
            return;
        }
        g.setColour (brassDark); g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (brass);     g.fillEllipse (c.x - r + 2, c.y - r + 2, 2 * r - 4, 2 * r - 4);
        g.setColour (juce::Colour (0xffb8312f));
        g.drawLine (c.x, c.y, c.x + std::sin (ang) * (r - 3), c.y - std::cos (ang) * (r - 3), 3.0f);
    }
};

inline juce::String formatValue (const juce::String& id, double v)
{
    auto f = [] (double x, int d) { return d == 0 ? juce::String ((juce::int64) std::llround (x)) : juce::String (x, d); };   // d = 0 must round, JUCE prints 0.9999994 as 99999
    if (id.startsWith ("eq_") && id.endsWith ("_db")) return (v > 0.05 ? "+" : "") + f (v, 1);
    if (id == "lofi_tone_hz" || id == "eq_mid_hz") return v >= 1000.0 ? f (v / 1000.0, 1) + "k" : f (v, 0);
    if (id == "cutoff_hz") return v >= 1000.0 ? f (v / 1000.0, 1) + "k" : f (v, 0);
    if (id == "pdelay_semi") return (v > 0.5 ? "+" : "") + juce::String ((int) std::round (v)) + " st";
    if (id == "talk_vowel") { static const char* vn[5] = { "A", "E", "I", "O", "U" }; return vn[juce::jlimit (0, 4, (int) std::round (v * 4.0))]; }
    if (id == "sgate_threshold_db") return f (v, 0) + " dB";
    if (id.endsWith ("_fine")) return (v > 0 ? "+" : "") + f (v, 0) + " ct";
    if (id == "osc2_level" || id == "osc3_level") return f (v * 100.0, 0) + "%";
    if (id == "unison_voices" || id == "voices") return juce::String ((int) std::round (v));
    if (id == "unison_detune") return f (v, 0) + " ct";
    if (id == "crush_downsample") return f (v, 1) + "x";
    if (id == "crush_bits") return f (v, 1);
    if (id == "comp_ratio") return f (v, 1) + ":1";
    if (id.endsWith ("_rate_hz")) return f (v, 1) + " Hz";
    if (id.endsWith ("_db")) return (std::abs (v) >= 10.0 ? f (v, 0) : f (v, 1)) + " dB";
    if (id.endsWith ("_ct")) return f (v, 0) + " ct";
    if (id.endsWith ("_ms")) return v >= 1000.0 ? f (v / 1000.0, 2) + " s" : f (v, 0) + " ms";
    if (id == "sample_root" || id == "sampleb_root") return juce::MidiMessage::getMidiNoteName ((int) std::round (v), true, true, 3);
    if (id == "sample_octave" || id == "sampleb_octave") return (v > 0.5 ? "+" : "") + juce::String ((int) std::round (v));
    if (id == "sample_start" || id == "sample_end" || id == "loop_start" || id == "loop_end" || id == "sample_gain"
        || id == "sampleb_start" || id == "sampleb_end" || id == "sampleb_loop_start" || id == "sampleb_loop_end" || id == "sampleb_gain") return f (v * 100.0, 0) + "%";
    if (id == "reverb_size") return f (0.3 * std::pow (20.0, v), 1) + " s";
    if (id == "reverb_damp" || id == "reverb_mix" || id == "chorus_mix" || id == "flanger_mix" || id == "chorus_depth" || id == "flanger_depth") return f (v * 100.0, 0) + "%";
    if (id == "master" || id == "osc_level" || id == "arp_gate" || id == "arp_swing" || id == "arp_human") return f (v * 100.0, 0) + "%";
    return f (v, 2);
}

// Value knob with name above and value below.
class Knob : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setTooltip (const juce::String& s) override { juce::SettableTooltipClient::setTooltip (s); slider.setTooltip (s); }
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name) : label (name)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (slider);
        att = std::make_unique<juce::SliderParameterAttachment> (*s.getParameter (id), slider, nullptr);
        slider.textFromValueFunction = [id] (double v) { return formatValue (id, v); };
        slider.onValueChange = [this] { repaint(); };
    }
    void resized() override
    {
        auto b = getLocalBounds(); textH = juce::jmax (9, (int) (getHeight() * 0.15f));
        b.removeFromBottom (textH + 2);
        // A tall box would let the round knob run down over its own label and value, so cap the slider above the text rows.
        // Only when it overlaps by more than a few pixels, so knobs laid out earlier keep their exact position.
        const int labelTop = getHeight() - textH * 2 - 2, d = juce::jmin (b.getWidth(), b.getHeight());
        if ((b.getHeight() + d) / 2 > labelTop + textH / 4) b.setHeight (labelTop);
        slider.setBounds (b);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (parchment);
        drawText (g, label, juce::Rectangle<int> (0, getHeight() - textH * 2 - 2, getWidth(), textH), (float) textH * 0.88f, juce::Justification::centred);
        g.setColour (lime);
        drawText (g, slider.getTextFromValue (slider.getValue()), juce::Rectangle<int> (0, getHeight() - textH, getWidth(), textH), (float) textH, juce::Justification::centred);
    }
private:
    juce::Slider slider; juce::String label; int textH = 10;
    std::unique_ptr<juce::SliderParameterAttachment> att;
};

class StoneButton : public juce::Button
{
public:
    StoneButton (const juce::String& text, bool live, bool toggles) : juce::Button (text)
    {
        setClickingTogglesState (live && toggles); setEnabled (live);
    }
    float textScale = 0.42f;
    bool pad = false;      // use the larger pad sprites
    juce::Image icon;      // optional picture drawn with the label
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto& sk = skin(); const auto& sheet = pad ? sk.pad : sk.btn;
        const bool lit = getToggleState(), on = isEnabled();
        if (sheet.isValid())
        {
            const int st = ! on ? 4 : (lit ? 3 : (down ? 2 : (over ? 1 : 0)));
            const int cw = sheet.getWidth() / 5, ch = sheet.getHeight();
            nineSlice (g, sheet, { st * cw, 0, cw, ch }, getLocalBounds(), pad ? 100 : 64, (int) (getHeight() * (pad ? 0.34f : 0.32f)));
            g.setColour (lit ? juce::Colour (0xff2a1608) : (on ? parchment : muted.withAlpha (0.7f)));
        }
        else
        {
            auto b = getLocalBounds().toFloat().reduced (1.5f);
            g.setColour (lit ? juce::Colour (0xff3a3410) : (on ? stoneLight : stone)); g.fillRoundedRectangle (b, 6.0f);
            g.setColour (lit ? gold : (on ? goldDim.brighter (0.2f) : stoneEdge.withAlpha (0.6f))); g.drawRoundedRectangle (b, 6.0f, 2.0f);
            g.setColour (lit ? parchment : (on ? parchment.darker (0.2f) : muted.withAlpha (0.55f)));
        }
        auto textArea = getLocalBounds().reduced (getWidth() / 12, 0);
        float ts = textScale;
        if (icon.isValid())
        {
            const auto b = getLocalBounds();
            if (getWidth() > getHeight() * 2.2f)
            {
                const int is = (int) (getHeight() * 0.74f); juce::Rectangle<int> ir (getHeight() / 4, (getHeight() - is) / 2, is, is);
                g.setOpacity (on ? 1.0f : 0.68f); g.drawImage (icon, ir.toFloat(), juce::RectanglePlacement::stretchToFit);
                textArea = b.withTrimmedLeft (ir.getRight() + 6);
            }
            else
            {
                const int is = (int) (getHeight() * 0.54f); juce::Rectangle<int> ir ((getWidth() - is) / 2, (int) (getHeight() * 0.08f), is, is);
                g.setOpacity (on ? 1.0f : 0.68f); g.drawImage (icon, ir.toFloat(), juce::RectanglePlacement::stretchToFit);
                textArea = b.withTrimmedTop ((int) (getHeight() * 0.60f)).withTrimmedBottom ((int) (getHeight() * 0.12f)).reduced (getWidth() / 12, 0); ts = textScale * 0.8f;
            }
            g.setColour (lit ? juce::Colour (0xff2a1608) : (on ? parchment : muted.withAlpha (0.7f)));
        }
        drawText (g, getButtonText(), textArea, getHeight() * ts, juce::Justification::centred);
    }
};


// A framed picture button (sidebar). Dimmed when the feature it stands for is not built yet.
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, int cell, bool live) : juce::Button (name), idx (cell) { setEnabled (live); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto img = iconCell (skin().iconsSidebar, idx, false);
        if (! img.isValid()) return;
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (isEnabled() ? (down ? 0.85f : 1.0f) : 0.72f);
        g.drawImage (img, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        if (over && isEnabled()) { g.setColour (gold.withAlpha (0.18f)); g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (4.0f), 8.0f); }
    }
private:
    int idx;
};


// Keyboard that draws its keys from the skin sprites.
class SkinnedKeyboard : public juce::MidiKeyboardComponent
{
public:
    using juce::MidiKeyboardComponent::MidiKeyboardComponent;
protected:
    void drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool down, bool over, juce::Colour line, juce::Colour text) override
    {
        auto& sk = skin();
        if (! sk.keyWhiteUp.isValid()) { juce::MidiKeyboardComponent::drawWhiteNote (note, g, area, down, over, line, text); return; }
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (down ? sk.keyWhiteDown : sk.keyWhiteUp, area, juce::RectanglePlacement::stretchToFit);
        if (note % 12 == 0)
        {
            g.setColour (juce::Colour (0xff4a3210)); g.setFont (font (juce::jmax (8.0f, area.getWidth() * 0.42f)));
            g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3), area.withTrimmedTop (area.getHeight() * 0.84f).reduced (0, 2.0f), juce::Justification::centred);
        }
    }
    void drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool down, bool over, juce::Colour fill) override
    {
        auto& sk = skin();
        if (! sk.keyBlackUp.isValid()) { juce::MidiKeyboardComponent::drawBlackNote (note, g, area, down, over, fill); return; }
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (down ? sk.keyBlackDown : sk.keyBlackUp, area, juce::RectanglePlacement::stretchToFit);
    }
};

// ---- live displays (poll parameters at 30 Hz) ----
class Display : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    Display (RelicProcessor& p) : proc (p) { startTimerHz (30); }
protected:
    float val (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void paintFrame (juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (display); g.fillRoundedRectangle (b.reduced (4.0f), 6.0f);
        g.setColour (lime.withAlpha (0.10f));
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (b.getHeight() * i / 4.0f), 10.0f, b.getWidth() - 10.0f);
        if (skin().screen.isValid()) nineSlice (g, skin().screen, skin().screen.getBounds(), getLocalBounds(), 190, juce::jlimit (10, 34, (int) (getHeight() * 0.13f)));
        else { g.setColour (gold.withAlpha (0.8f)); g.drawRoundedRectangle (b.reduced (1), 6.0f, 2.0f); }
    }
    RelicProcessor& proc;
private:
    // Redraw only when something the display shows has changed. Redrawing every tick kept a CPU core busy even when nothing moved.
    // A refresh once a second is kept as a safety net for anything the key does not cover.
    void timerCallback() override
    {
        const auto k = proc.displayStateKey();
        if (k != lastKey || ++idleTicks >= 30) { lastKey = k; idleTicks = 0; repaint(); }
    }
    uint64_t lastKey = 0; int idleTicks = 0;
};

class WaveDisplay : public Display
{
public:
    using Display::Display;
    void paint (juce::Graphics& g) override
    {
        paintFrame (g);
        const int shape = (int) val ("waveform"); const float pw = val ("pulse_width");
        const auto user = (shape == 4 && val ("wt_use_file") > 0.5f) ? proc.getWavetableBank() : nullptr;
        auto b = getLocalBounds().toFloat().reduced (14.0f, 20.0f);
        juce::Path p; const int n = 400; const float cycles = 3.0f;
        for (int i = 0; i <= n; ++i)
        {
            const float ph = std::fmod (cycles * (float) i / n, 1.0f); float v;
            if (shape == 0) v = std::sin (ph * juce::MathConstants<float>::twoPi);
            else if (shape == 1) v = 1.0f - 2.0f * ph;
            else if (shape == 2) v = ph < pw ? 1.0f : -1.0f;
            else if (shape == 4 && user != nullptr)      // a loaded wavetable file: the frames morphed at the Width position
            {
                static const rf::WaveTables lookupOnly;
                v = lookupOnly.lookupUserMorph (*user, juce::jlimit (0.0, 1.0, (double) (pw - 0.05f) / 0.9), 3, (double) ph) * 0.9f;
            }
            else if (shape == 4)       // wavetable: the bank morphed at the Width position, summed from its harmonics
            {
                const double pos = juce::jlimit (0.0, 1.0, (double) (pw - 0.05f) / 0.9) * (rf::WaveTables::WtCount - 1); const int t0 = std::min (rf::WaveTables::WtCount - 2, (int) pos); const double fr = pos - t0;
                double sum = 0; for (int k = 1; k <= 24; ++k) sum += ((1.0 - fr) * rf::WaveTables::wtAmp (t0, k) + fr * rf::WaveTables::wtAmp (t0 + 1, k)) * std::sin (2.0 * juce::MathConstants<double>::pi * k * ph);
                v = (float) (sum * 0.55);
            }
            else if (shape == 5) { const float u = (float) i / n; v = std::exp (-2.6f * u) * (std::sin (ph * 6.2832f) + 0.5f * std::sin (ph * 6.2832f * 3.0f + 1.0f) * std::exp (-6.0f * u)); }
            else if (shape == 6) { const float u = (float) i / n; v = 0.8f * std::exp (-1.5f * u) * std::sin (u * 6.2832f * 6.0f) + 0.5f * std::exp (-4.0f * u) * std::sin (u * 6.2832f * 16.5f) + 0.3f * std::exp (-8.0f * u) * std::sin (u * 6.2832f * 32.0f); }
            else v = ph < 0.25f ? ph * 4 : (ph < 0.75f ? 2 - ph * 4 : ph * 4 - 4);
            const float x = b.getX() + b.getWidth() * i / n, y = b.getCentreY() - v * b.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (lime); g.strokePath (p, juce::PathStrokeType (2.5f));
        g.setColour (lime.withAlpha (0.85f));
        static const char* names[] = { "SINE", "SAW", "SQUARE", "TRIANGLE", "WAVETABLE", "PLUCK", "BELL" };
        drawText (g, juce::String ("OSC  ") + (user != nullptr ? juce::String ("WAVETABLE  ") + juce::String (user->name).toUpperCase().substring (0, 28) + "  " + juce::String (user->frames) + (user->frames == 1 ? " FRAME" : " FRAMES") : juce::String (names[shape])) + "   UNISON " + juce::String ((int) val ("unison_voices")), juce::Rectangle<int> (14, 6, getWidth() - 28, (int) (getHeight() * 0.1f)), getHeight() * 0.07f, juce::Justification::left);
    }
};

class EnvDisplay : public Display
{
public:
    using Display::Display;
    void paint (juce::Graphics& g) override
    {
        paintFrame (g);
        auto b = getLocalBounds().toFloat().reduced (10.0f);
        const float a = std::sqrt (val ("attack_ms") / 5000.0f) , d = std::sqrt (val ("decay_ms") / 5000.0f), r = std::sqrt (val ("release_ms") / 5000.0f), s = val ("sustain");
        const float tot = a + d + 0.35f + r + 0.01f, w = b.getWidth();
        const float x1 = b.getX() + w * a / tot, x2 = x1 + w * d / tot, x3 = x2 + w * 0.35f / tot, x4 = x3 + w * r / tot;
        const float top = b.getY() + 4, bot = b.getBottom() - 2, ys = bot - (bot - top) * s;
        juce::Path p; p.startNewSubPath (b.getX(), bot); p.lineTo (x1, top); p.lineTo (x2, ys); p.lineTo (x3, ys); p.lineTo (x4, bot);
        g.setColour (lime); g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved));
        for (auto pt : { juce::Point<float> (x1, top), { x2, ys }, { x3, ys } }) g.fillEllipse (pt.x - 4, pt.y - 4, 8, 8);
    }
};

class FilterDisplay : public Display
{
public:
    using Display::Display;
    void paint (juce::Graphics& g) override
    {
        paintFrame (g);
        const float fc = val ("cutoff_hz"), q = 0.7071f + 5.2f * val ("resonance"); const int type = (int) val ("filter_type");
        const bool on = val ("filter_on") > 0.5f;
        auto b = getLocalBounds().toFloat().reduced (6.0f);
        juce::Path p;
        for (int i = 0; i <= 160; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 160.0f), w = f / fc;
            const float den2 = (1 - w * w) * (1 - w * w) + (w / q) * (w / q);
            float num2 = 1.0f; if (type == 0) num2 = 1; else if (type == 1) num2 = w * w * w * w; else if (type == 2) num2 = (w / q) * (w / q); else num2 = (1 - w * w) * (1 - w * w);
            float db = on ? 10.0f * std::log10 (std::max (num2 / den2, 1e-9f)) : 0.0f;
            db = juce::jlimit (-36.0f, 18.0f, db);
            const float x = b.getX() + b.getWidth() * i / 160.0f, y = b.getY() + b.getHeight() * (1.0f - (db + 36.0f) / 54.0f) * 0.92f + 2;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (lime); g.strokePath (p, juce::PathStrokeType (2.5f));
        const float cx = b.getX() + b.getWidth() * std::log10 (fc / 20.0f) / 3.0f;
        g.setColour (gold); g.fillEllipse (cx - 5, b.getBottom() - 12, 10, 10);
        static const char* models[4] = { "CLEAN", "OBX", "SP-12", "MPC60" }; static const char* srcs[3] = { "", "  SYN", "  SMP" };
        g.setColour (lime.withAlpha (0.85f));
        drawText (g, juce::String (models[juce::jlimit (0, 3, (int) val ("filter_model"))]) + srcs[juce::jlimit (0, 2, (int) val ("filter_source"))], juce::Rectangle<float> (8, 4, (float) getWidth() - 16, (float) getHeight() * 0.2f), (float) getHeight() * 0.15f, juce::Justification::left);
    }
    // click: next filter model; shift-click: next filter source (all / synth only / sample only)
    void mouseDown (const juce::MouseEvent& e) override
    {
        auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (e.mods.isShiftDown() ? "filter_source" : "filter_model")); if (c == nullptr) return;
        c->beginChangeGesture(); *c = (c->getIndex() + 1) % c->choices.size(); c->endChangeGesture();
    }
};

// Two-axis controller: drag the dot, it drives the XY X / XY Y matrix sources. Double-click anywhere on it to put the picture back.
class XyPad : public Display
{
public:
    using Display::Display;
    std::function<void()> onDoubleClick;
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0b1a12)); paintFrame (g);
        auto b = pad(); g.setColour (lime.withAlpha (0.14f));
        for (int i = 1; i < 4; ++i) { g.drawVerticalLine ((int) (b.getX() + b.getWidth() * i / 4), b.getY(), b.getBottom()); g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * i / 4), b.getX(), b.getRight()); }
        g.setColour (goldDim); g.drawRoundedRectangle (b, 6.0f, 2.0f);
        const float x = b.getX() + b.getWidth() * val ("xy_x"), y = b.getBottom() - b.getHeight() * val ("xy_y");
        g.setColour (lime.withAlpha (0.45f)); g.drawLine (x, b.getY(), x, b.getBottom(), 1.5f); g.drawLine (b.getX(), y, b.getRight(), y, 1.5f);
        g.setColour (gold); g.fillEllipse (x - 10, y - 10, 20, 20); g.setColour (lime); g.drawEllipse (x - 10, y - 10, 20, 20, 2.0f);
        g.setColour (parchment); drawText (g, "XY PAD", juce::Rectangle<float> (0, 6, (float) getWidth(), 26), 20, juce::Justification::centred);
        g.setColour (lime.withAlpha (0.7f)); drawText (g, "X", juce::Rectangle<float> (b.getX(), b.getBottom() + 2, b.getWidth(), 18), 14, juce::Justification::centred);
        drawText (g, "MACROS    (DOUBLE-CLICK TO CLOSE)", juce::Rectangle<float> (0, b.getBottom() + 22, (float) getWidth(), 18), 12, juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent& e) override { if (pad().contains (e.position)) { dragging = true; set (e); } }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragging) set (e); }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; for (auto* id : { "xy_x", "xy_y" }) if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture(); gestures = false; }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }
private:
    juce::Rectangle<float> pad() const { return juce::Rectangle<float> (16.0f, 34.0f, (float) getWidth() - 32.0f, (float) getHeight() * 0.52f); }
    void set (const juce::MouseEvent& e)
    {
        auto b = pad(); const float x = juce::jlimit (0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth()), y = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - b.getY()) / b.getHeight());
        if (! gestures) { for (auto* id : { "xy_x", "xy_y" }) if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture(); gestures = true; }
        if (auto* p = proc.apvts.getParameter ("xy_x")) p->setValueNotifyingHost (x);
        if (auto* p = proc.apvts.getParameter ("xy_y")) p->setValueNotifyingHost (y);
    }
    bool dragging = false, gestures = false;
};

class Meter : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    Meter (RelicProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (display); g.fillRoundedRectangle (b.reduced (3.0f), 4.0f);
        if (skin().screen.isValid()) nineSlice (g, skin().screen, skin().screen.getBounds(), getLocalBounds(), 190, juce::jlimit (8, 26, (int) (getHeight() * 0.1f)));
        const float lv[2] = { proc.levelL.load(), proc.levelR.load() };
        auto& sk = skin(); const bool art = sk.meterSegs.isValid();
        const int segs = art ? 6 : 22;
        const float x0 = b.getX() + 10.0f, w = (b.getWidth() - 20.0f - 4.0f) / 2.0f, top = b.getY() + 10.0f, hh = b.getHeight() - 20.0f;
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        for (int c = 0; c < 2; ++c)
        {
            const float db = juce::Decibels::gainToDecibels (lv[c], -48.0f), f = (db + 48.0f) / 48.0f;
            for (int i = 0; i < segs; ++i)
            {
                const float segH = hh / segs, y = top + hh - segH * (i + 1);
                const bool litSeg = (float) i / segs < f;
                auto r = juce::Rectangle<float> (x0 + c * (w + 4.0f), y + 1.0f, w, segH - 2.0f);
                if (art)
                {
                    const int cell = litSeg ? (i >= segs - 1 ? 2 : (i >= segs - 2 ? 1 : 0)) : 3;
                    if (! tiles[cell].isValid()) { const int cw = sk.meterSegs.getWidth() / 4; for (int t = 0; t < 4; ++t) tiles[t] = sk.meterSegs.getClippedImage ({ t * cw, 0, cw, sk.meterSegs.getHeight() }).createCopy(); }
                    drawScaledCached (g, tiles[cell], tiles[cell].getBounds(), r);
                }
                else
                {
                    g.setColour (litSeg ? (i > segs - 4 ? juce::Colour (0xffd9482a) : (i > segs - 8 ? juce::Colour (0xffe0b020) : lime)) : lime.withAlpha (0.12f));
                    g.fillRect (r);
                }
            }
        }
    }
private:
    // The meter shows at most 6 lit segments per channel, so it only needs redrawing when that count changes.
    void timerCallback() override
    {
        int lit[2]; const float lv[2] = { proc.levelL.load(), proc.levelR.load() };
        for (int c = 0; c < 2; ++c)
        {
            const float f = (juce::Decibels::gainToDecibels (lv[c], -48.0f) + 48.0f) / 48.0f;
            lit[c] = (int) std::ceil (juce::jlimit (0.0f, 1.0f, f) * 22.0f);       // 22 covers the finer fallback drawing too
        }
        if (lit[0] != lastLit[0] || lit[1] != lastLit[1]) { lastLit[0] = lit[0]; lastLit[1] = lit[1]; repaint(); }
    }
    RelicProcessor& proc; int lastLit[2] = { -1, -1 }; juce::Image tiles[4];
};


// The loaded sample: waveform overview, start and loop markers (drag them), name and root note.
class SampleDisplay : public Display
{
public:
    using Display::Display;
    void setSlot (int s) { if (s != slot) { slot = s; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        paintFrame (g);
        auto s = slot == 0 ? proc.getSample() : proc.getSampleB(); const float W = (float) getWidth(), H = (float) getHeight();
        if (s == nullptr)
        {
            g.setColour (lime.withAlpha (0.85f));
            drawText (g, slot == 0 ? "DROP A SAMPLE HERE" : "NO SAMPLE IN SLOT B", juce::Rectangle<float> (0, H * 0.34f, W, H * 0.16f), H * 0.12f, juce::Justification::centred);
            g.setColour (lime.withAlpha (0.55f));
            drawText (g, slot == 0 && proc.sampleError.isNotEmpty() ? proc.sampleError : juce::String (slot == 0 ? "or press LOAD  (wav, aiff, flac, mp3)" : "press LOAD to put a sample in slot B"),
                      juce::Rectangle<float> (0, H * 0.52f, W, H * 0.1f), H * 0.07f, juce::Justification::centred);
            return;
        }
        auto b = getLocalBounds().toFloat().reduced (16.0f, 28.0f);
        const float cy = b.getCentreY(), amp = b.getHeight() * 0.46f;
        g.setColour (lime);
        for (int x = 0; x < (int) b.getWidth(); ++x)
        {
            const int k = juce::jlimit (0, 1023, (int) (x * 1024.0f / b.getWidth()));
            const float lo = juce::jlimit (-1.0f, 1.0f, s->peaksMin[(size_t) k] / s->peak), hi = juce::jlimit (-1.0f, 1.0f, s->peaksMax[(size_t) k] / s->peak);
            g.drawLine (b.getX() + x, cy - hi * amp, b.getX() + x, cy - lo * amp + 1.0f, 1.2f);
        }
        const float st = val (id (0)), en = val (id (1)), la = val (id (2)), lb = val (id (3)); const bool loopOn = val (id (4)) > 0.5f;
        auto line = [&] (float v, juce::Colour c) { g.setColour (c); const float x = b.getX() + b.getWidth() * v; g.drawLine (x, b.getY(), x, b.getBottom(), 2.0f); g.fillEllipse (x - 5, b.getBottom() - 4, 10, 10); };
        // the part before Start and after End is not played
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        if (st > 0.0f) g.fillRect (b.getX(), b.getY(), b.getWidth() * st, b.getHeight());
        if (en < 0.999f) g.fillRect (b.getX() + b.getWidth() * en, b.getY(), b.getWidth() * (1.0f - en), b.getHeight());
        if (loopOn) { g.setColour (gold.withAlpha (0.12f)); g.fillRect (b.getX() + b.getWidth() * la, b.getY(), b.getWidth() * (lb - la), b.getHeight()); line (la, gold); line (lb, gold); }
        if (slot == 0 && val ("chop_on") > 0.5f)
        {
            double sb[18]; const int cnt = proc.chopSlices (*s, proc.getHostBpm(), sb);
            for (int i = 0; i < cnt; ++i)
            {
                const float x0 = b.getX() + b.getWidth() * (float) sb[i], x1 = b.getX() + b.getWidth() * (float) sb[i + 1];
                g.setColour ((i & 1 ? juce::Colour (0xff7fd4ff) : gold).withAlpha (0.10f)); g.fillRect (x0, b.getY(), x1 - x0, b.getHeight());
                g.setColour (juce::Colour (0xffe8e0b0).withAlpha (0.8f)); g.drawLine (x0, b.getY(), x0, b.getBottom(), 1.5f);
                g.setColour (juce::Colour (0xffe8e0b0)); drawText (g, juce::String (i + 1), juce::Rectangle<float> (x0 + 2, b.getY(), std::max (12.0f, x1 - x0 - 2), H * 0.1f), H * 0.075f, juce::Justification::left);
            }
        }
        line (st, juce::Colour (0xffe8e0b0));
        if (en < 0.999f) line (en, juce::Colour (0xffff9a7a));
        g.setColour (lime.withAlpha (0.9f));
        const int root = (int) val (id (5)); const bool follows = slot == 1 && val ("sampleb_indep") < 0.5f;
        drawText (g, juce::String (slot == 0 ? "SLOT A  " : "SLOT B  ") + juce::String (s->rate / 1000.0, 1) + "k   ROOT " + juce::MidiMessage::getMidiNoteName (root, true, true, 3) + "    " + s->name.toUpperCase() + (follows ? "    (FOLLOWS A: TURN ON B LAYER)" : ""),
                  juce::Rectangle<int> (14, 6, getWidth() - 28, (int) (H * 0.1f)), H * 0.07f, juce::Justification::left);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if ((slot == 0 ? proc.getSample() : proc.getSampleB()) == nullptr) return;
        const float x01 = juce::jlimit (0.0f, 1.0f, (e.position.x - 16.0f) / (float) (getWidth() - 32));
        if (slot == 0 && val ("chop_on") > 0.5f && e.mods.isPopupMenu()) { showChopMenu(); return; }
        if (slot == 0 && val ("chop_on") > 0.5f && (int) val ("chop_mode") == 3)        // Manual slicing: a slice marker near the click is dragged first
        {
            const int cnt = juce::jlimit (2, 16, (int) val ("chop_count")); float bd = 0.025f; chopDrag = -1;
            for (int i = 1; i < cnt; ++i) { const float d = std::abs (val (("chop_pt_" + juce::String (i)).toRawUTF8()) - x01); if (d < bd) { bd = d; chopDrag = i; } }
            if (chopDrag > 0) { drag = proc.apvts.getParameter ("chop_pt_" + juce::String (chopDrag)); drag->beginChangeGesture(); mouseDrag (e); return; }
        }
        chopDrag = -1;
        float best = 0.04f; drag = nullptr;
        for (int i = 0; i < 4; ++i)       // Start, End, Loop in, Loop out; the loop markers only count while looping
        { if (i >= 2 && val (id (4)) < 0.5f) continue; const float d = std::abs (val (id (i)) - x01); if (d < best) { best = d; drag = proc.apvts.getParameter (id (i)); } }
        if (drag) { drag->beginChangeGesture(); mouseDrag (e); }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! drag) return;
        float v = juce::jlimit (0.0f, 1.0f, (e.position.x - 16.0f) / (float) (getWidth() - 32));
        const auto pid = drag->paramID;
        if (chopDrag > 0)       // keep the marker between its neighbours so the order never changes
        {
            const int cnt = juce::jlimit (2, 16, (int) val ("chop_count")); float lo = 0.0f, hi = 1.0f; const float me = val (pid.toRawUTF8());
            for (int i = 1; i < cnt; ++i) { if (i == chopDrag) continue; const float o = val (("chop_pt_" + juce::String (i)).toRawUTF8()); if (o <= me) lo = std::max (lo, o); else hi = std::min (hi, o); }
            if (auto sm = proc.getSample(); sm != nullptr && ! e.mods.isShiftDown())       // snaps to a zero crossing (no click at the cut), or to the tempo grid with Alt; Shift places it freely
            {
                const double nF = (double) sm->frames();
                if (e.mods.isAltDown()) { static const double beats[4] = { 1.0, 0.5, 0.25, 0.125 }; v = (float) rf::snap::gridPoint (v, beats[juce::jlimit (0, 3, (int) val ("chop_grid"))], proc.getHostBpm(), sm->rate, sm->frames()); }
                else v = (float) (rf::snap::zeroCrossing (sm->L.data(), sm->R.empty() ? nullptr : sm->R.data(), sm->frames(), (double) v * nF, (int) (sm->rate * 0.006)) / nF);
            }
            v = juce::jlimit (lo + 0.003f, hi - 0.003f, v);
        }
        else if (pid == id (0)) v = std::min (v, val (id (1)) - 0.01f);
        if (pid == id (1)) v = std::max (v, val (id (0)) + 0.01f);
        if (pid == id (2)) v = std::min (v, val (id (3)) - 0.01f);
        if (pid == id (3)) v = std::max (v, val (id (2)) + 0.01f);
        drag->setValueNotifyingHost (drag->convertTo0to1 (juce::jlimit (0.0f, 1.0f, v)));
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag) { drag->endChangeGesture(); drag = nullptr; } chopDrag = -1; }
private:
    int chopDrag = -1;
    void setParam (const char* pid, float v) { if (auto* q = proc.apvts.getParameter (pid)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); } }
    void showChopMenu()
    {
        juce::PopupMenu m; m.addSectionHeader ("CHOP SLICES"); m.addItem (1, "Make the current slices manual (then drag the markers)"); m.addItem (2, "Manual: space the slices evenly");
        auto safe = juce::Component::SafePointer<SampleDisplay> (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe == nullptr || r == 0) return; auto s = safe->proc.getSample(); if (s == nullptr) return;
            double b[18]; int cnt = r == 1 ? safe->proc.chopSlices (*s, safe->proc.getHostBpm(), b) : juce::jlimit (2, 16, (int) safe->val ("chop_count"));
            if (r == 2) for (int i = 0; i <= cnt; ++i) b[i] = (double) i / cnt;
            for (int i = 1; i < cnt; ++i) safe->setParam (("chop_pt_" + juce::String (i)).toRawUTF8(), (float) b[i]);
            safe->setParam ("chop_count", (float) cnt); safe->setParam ("chop_mode", 3.0f);
        });
    }
    // parameter ids of the slot being shown: 0 start, 1 end, 2 loop in, 3 loop out, 4 loop on, 5 root
    const char* id (int k) const
    {
        static const char* a[6] = { "sample_start", "sample_end", "loop_start", "loop_end", "sample_loop", "sample_root" };
        static const char* b[6] = { "sampleb_start", "sampleb_end", "sampleb_loop_start", "sampleb_loop_end", "sampleb_loop", "sampleb_root" };
        return (slot == 0 ? a : b)[k];
    }
    juce::RangedAudioParameter* drag = nullptr; int slot = 0;
};

// SAMPLE <-> Synth crossfader.
class MixSlider : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setTooltip (const juce::String& s) override { juce::SettableTooltipClient::setTooltip (s); slider.setTooltip (s); }
    MixSlider (juce::AudioProcessorValueTreeState& s, const char* paramId = "synth_mix", const char* leftLabel = "SAMPLE", const char* rightLabel = "SYNTH") : left (leftLabel), right (rightLabel)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal); slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (slider);
        att = std::make_unique<juce::SliderParameterAttachment> (*s.getParameter (paramId), slider, nullptr);
    }
    void resized() override { slider.setBounds (getLocalBounds().withTrimmedTop (getHeight() * 38 / 100)); }
    void paint (juce::Graphics& g) override
    {
        const float lh = (float) getHeight() * 0.34f;
        g.setColour (parchment); drawText (g, left, juce::Rectangle<int> (6, 0, getWidth() / 3, (int) lh), lh * 0.95f, juce::Justification::left);
        drawText (g, right, juce::Rectangle<int> (getWidth() * 2 / 3 - 6, 0, getWidth() / 3, (int) lh), lh * 0.95f, juce::Justification::right);
    }
private:
    juce::String left, right; juce::Slider slider; std::unique_ptr<juce::SliderParameterAttachment> att;
};


// A scrolling list of text rows. Clicking a row calls onClick(row).
class TextList : public juce::Component, private juce::ListBoxModel
{
public:
    TextList()
    {
        list.setModel (this); list.setRowHeight (34);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        list.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, goldDim.brighter (0.2f));
        list.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
        list.getViewport()->setScrollBarThickness (12);
        addAndMakeVisible (list);
    }
    std::function<void (int)> onClick;
    void setItems (const juce::StringArray& s, int selected)
    {
        items = s; selRow = selected; list.updateContent(); list.repaint();
        if (selected >= 0) list.scrollToEnsureRowIsOnscreen (selected);
    }
    void resized() override { list.setBounds (getLocalBounds()); }
private:
    int getNumRows() override { return items.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool) override
    {
        if (row < 0 || row >= items.size()) return;
        if (row == selRow)
        {
            g.setColour (gold.withAlpha (0.22f)); g.fillRoundedRectangle (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f, 5.0f);
            g.setColour (gold); g.drawRoundedRectangle (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f, 5.0f, 1.5f);
        }
        g.setColour (row == selRow ? parchment : parchment.darker (0.15f));
        drawText (g, items[row], juce::Rectangle<int> (10, 0, w - 20, h), (float) h * 0.56f, juce::Justification::left);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent&) override { if (onClick && row >= 0 && row < items.size()) onClick (row); }
    juce::ListBox list; juce::StringArray items; int selRow = -1;
};

// Overlay that lists preset banks and presets from disk.
class PresetBrowser : public juce::Component
{
public:
    explicit PresetBrowser (RelicProcessor& p) : proc (p)
    {
        addAndMakeVisible (banks); addAndMakeVisible (presets);
        banks.onClick = [this] (int r) { selBank = r; fillPresets(); };
        // search box
        search.setMultiLine (false); search.setReturnKeyStartsNewLine (false); search.setSelectAllWhenFocused (true);
        search.setTextToShowWhenEmpty ("Search presets", muted);
        search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b090e)); search.setColour (juce::TextEditor::textColourId, parchment);
        search.setColour (juce::TextEditor::outlineColourId, goldDim); search.setColour (juce::TextEditor::focusedOutlineColourId, gold);
        search.setColour (juce::TextEditor::highlightColourId, gold.withAlpha (0.3f)); search.setColour (juce::CaretComponent::caretColourId, gold);
        search.onTextChange = [this] { fillPresets(); }; search.onEscapeKey = [this] { search.clear(); };
        search.setTooltip (help::forName ("preset_search")); addAndMakeVisible (search);
        // category buttons: ALL, the seven categories, then OTHER
        const char* chipText[9] = { "ALL", "PADS", "KEYS", "LEADS", "BASS", "PLUCK", "ARPS", "FX", "OTHER" };
        for (int i = 0; i < 9; ++i)
        {
            chips[i] = std::make_unique<StoneButton> (chipText[i], true, false); chips[i]->textScale = 0.34f;
            chips[i]->setTooltip (help::forName (juce::String ("cat:") + chipText[i]));
            chips[i]->onClick = [this, i] { selCat = i; for (int j = 0; j < 9; ++j) chips[j]->setToggleState (j == selCat, juce::dontSendNotification); fillPresets(); };
            addAndMakeVisible (*chips[i]);
        }
        chips[0]->setToggleState (true, juce::dontSendNotification);
        presets.onClick = [this] (int r)
        {
            if (r >= 0 && r < (int) shown.size() && proc.loadPresetFile (shown[(size_t) r].file)) updateSelection();
        };
        const char* names[5] = { "SAVE AS", "INIT", "RESCAN", "TYPE: ALL", "CLOSE" };
        for (int i = 0; i < 5; ++i)
        {
            btn[i] = std::make_unique<StoneButton> (names[i], true, false); btn[i]->textScale = 0.42f; addAndMakeVisible (*btn[i]);
        }
        btn[0]->onClick = [this] { if (onSave) onSave(); };
        btn[1]->onClick = [this] { proc.initPreset(); updateSelection(); };
        btn[2]->onClick = [this] { refresh(); };
        btn[3]->onClick = [this] { selPart = (selPart + 1) % (presetPartNames().size() + 1); btn[3]->setButtonText (selPart == 0 ? "TYPE: ALL" : "TYPE: " + presetPartNames()[selPart - 1].toUpperCase()); fillPresets(); };
        btn[3]->setTooltip (help::forName ("preset_type"));
        btn[4]->onClick = [this] { if (onClose) onClose(); };
    }
    std::function<void()> onSave, onClose;
    void refresh()
    {
        all = proc.scanPresets(); bankNames.clear();
        for (auto& e : all) if (! bankNames.contains (e.bank)) bankNames.add (e.bank);
        if (selBank > bankNames.size()) selBank = 0;
        juce::StringArray items; items.add ("All  (" + juce::String ((int) all.size()) + ")");
        for (auto& b : bankNames) { int n = 0; for (auto& e : all) if (e.bank == b) ++n; items.add (b.fromLastOccurrenceOf ("/", false, false) + "  (" + juce::String (n) + ")"); }
        banks.setItems (items, selBank); fillPresets();
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff120f16)); g.fillRoundedRectangle (b.reduced (4.0f), 8.0f);
        if (skin().screen.isValid()) nineSlice (g, skin().screen, skin().screen.getBounds(), getLocalBounds(), 190, juce::jlimit (10, 34, (int) (getHeight() * 0.07f)));
        g.setColour (gold); drawText (g, "PRESETS", juce::Rectangle<float> (0, (float) getHeight() * 0.045f, (float) getWidth(), (float) getHeight() * 0.07f), (float) getHeight() * 0.055f, juce::Justification::centred);
        if (all.empty()) { g.setColour (muted); drawText (g, "No presets found. Press RESCAN.", getLocalBounds(), (float) getHeight() * 0.05f, juce::Justification::centred); }
        else if (shown.empty()) { g.setColour (muted); drawText (g, "No presets match.", presets.getBounds().reduced (10), (float) getHeight() * 0.045f, juce::Justification::centred); }
    }
    void resized() override
    {
        auto b = getLocalBounds().reduced (getWidth() / 28, getHeight() / 14); b.removeFromTop (getHeight() / 11);
        auto searchRow = b.removeFromTop (getHeight() / 12); b.removeFromTop (getHeight() / 80);
        search.setBounds (searchRow); search.setFont (font ((float) searchRow.getHeight() * 0.62f, false)); search.setIndents (10, (searchRow.getHeight() - (int) ((float) searchRow.getHeight() * 0.62f)) / 2);
        auto chipRow = b.removeFromTop (getHeight() / 13); b.removeFromTop (getHeight() / 80);
        for (int i = 0; i < 9; ++i) chips[i]->setBounds (chipRow.removeFromLeft (chipRow.getWidth() / (9 - i)).reduced (1, 0));
        auto bottom = b.removeFromBottom (getHeight() / 10); b.removeFromBottom (getHeight() / 40);
        banks.setBounds (b.removeFromLeft (b.getWidth() * 40 / 100)); b.removeFromLeft (getWidth() / 60); presets.setBounds (b);
        const int bw = bottom.getWidth() / 5;
        for (int i = 0; i < 5; ++i) btn[i]->setBounds (bottom.removeFromLeft (bw).reduced (getWidth() / 90, 0));
    }
    void updateSelection() { fillPresets(); }
private:
    void fillPresets()
    {
        shown.clear(); juce::StringArray names; int sel = -1;
        const auto q = search.getText().trim().toLowerCase();
        const auto wanted = selCat == 0 ? juce::String() : (selCat == 8 ? juce::String ("Other") : presetCategories()[selCat - 1]);
        for (auto& e : all)
        {
            const auto cat = presetCategories().contains (e.category) ? e.category : juce::String ("Other");
            if (selBank != 0 && e.bank != bankNames[selBank - 1]) continue;
            if (wanted.isNotEmpty() && cat != wanted) continue;
            if (selPart == 1 && e.part.isNotEmpty()) continue;
            if (selPart > 1 && e.part != presetPartNames()[selPart - 1]) continue;
            if (q.isNotEmpty() && ! (e.name + " " + cat + " " + e.bank + " " + e.part).toLowerCase().contains (q)) continue;
            if (e.part.isEmpty() && e.name == proc.getPresetName() && e.bank == proc.getPresetBank()) sel = (int) shown.size();
            shown.push_back (e); names.add (e.part.isNotEmpty() ? e.name + "   [" + e.part + "]" : e.name);
        }
        presets.setItems (names, sel); repaint();
    }
    RelicProcessor& proc; TextList banks, presets; std::unique_ptr<StoneButton> btn[5]; int selPart = 0;      // selPart: 0 all types, 1 full presets, 2.. one part
    juce::TextEditor search; std::unique_ptr<StoneButton> chips[9]; int selCat = 0;
    std::vector<PresetEntry> all, shown; juce::StringArray bankNames; int selBank = 0;
};

// Label on top, then  <  value  >  for a choice or integer parameter.
class Stepper : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setTooltip (const juce::String& s) override { juce::SettableTooltipClient::setTooltip (s); if (left) left->setTooltip (s); if (right) right->setTooltip (s); }
    Stepper (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name)
        : label (name), param (dynamic_cast<juce::RangedAudioParameter*> (s.getParameter (id)))
    {
        left  = std::make_unique<StoneButton> ("<", true, false); right = std::make_unique<StoneButton> (">", true, false);
        left->onClick = [this] { step (-1); }; right->onClick = [this] { step (+1); };
        addAndMakeVisible (*left); addAndMakeVisible (*right);
        att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); }, nullptr);
        att->sendInitialUpdate();
    }
    void step (int d)
    {
        const auto r = param->getNormalisableRange(); const int lo = (int) std::round (r.start), hi = (int) std::round (r.end), n = hi - lo + 1;
        const int cur = (int) std::round (param->convertFrom0to1 (param->getValue()));
        const int next = lo + ((cur - lo + d) % n + n) % n;
        param->beginChangeGesture(); param->setValueNotifyingHost (param->convertTo0to1 ((float) next)); param->endChangeGesture();
    }
    void resized() override
    {
        auto b = getLocalBounds(); textH = juce::jmax (9, (int) (getHeight() * 0.22f)); b.removeFromTop (textH + 2);
        const int bw = juce::jmin (b.getHeight(), getWidth() / 5);
        left->setBounds (b.removeFromLeft (bw)); right->setBounds (b.removeFromRight (bw)); box = b.reduced (3, 0);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (parchment); drawText (g, label, juce::Rectangle<int> (0, 0, getWidth(), textH), (float) textH * 0.88f, juce::Justification::centred);
        g.setColour (juce::Colour (0xff120f16)); g.fillRoundedRectangle (box.toFloat(), 5.0f);
        const auto txt = param->getCurrentValueAsText().toUpperCase(); const float fh = juce::jmin ((float) box.getHeight() * 0.52f, (float) box.getWidth() * 1.3f / (float) juce::jmax (1, txt.length()));
        g.setColour (lime); drawText (g, txt, box, fh, juce::Justification::centred);
    }
private:
    juce::String label; juce::RangedAudioParameter* param; int textH = 10; juce::Rectangle<int> box;
    std::unique_ptr<StoneButton> left, right; std::unique_ptr<juce::ParameterAttachment> att;
};

// Seven tiles, one per scale degree. Shows the chord each degree plays; lights the one a key just triggered.
class ChordDisplay : public Display
{
public:
    using Display::Display;
    void paint (juce::Graphics& g) override
    {
        paintFrame (g);
        const auto s = proc.readChordArp(); const int hot = proc.chordDegreeShown.load();
        static const char* roman[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
        auto area = getLocalBounds().toFloat().reduced (16.0f, 18.0f); area.removeFromTop (area.getHeight() * 0.16f);
        const float gap = 8.0f, tw = (area.getWidth() - gap * 6) / 7.0f;
        g.setColour (lime.withAlpha (s.chordOn ? 0.85f : 0.4f));
        drawText (g, juce::String (rf::noteName (s.key)) + "  " + kScaleNames[juce::jlimit (0, 8, s.scale)] + (s.chordOn ? "" : "   (CHORD OFF)"),
                  juce::Rectangle<float> (16.0f, 8.0f, (float) getWidth() - 32.0f, (float) getHeight() * 0.14f), (float) getHeight() * 0.09f, juce::Justification::left);
        for (int d = 0; d < 7; ++d)
        {
            auto t = juce::Rectangle<float> (area.getX() + d * (tw + gap), area.getY(), tw, area.getHeight());
            const bool lit = s.chordOn && d == hot;
            g.setColour (lit ? lime.withAlpha (0.9f) : juce::Colour (0xff120f16).withAlpha (s.chordOn ? 1.0f : 0.6f)); g.fillRoundedRectangle (t, 7.0f);
            g.setColour (lit ? lime : goldDim); g.drawRoundedRectangle (t, 7.0f, 2.0f);
            g.setColour (lit ? juce::Colour (0xff102010) : (s.chordOn ? parchment : muted));
            drawText (g, roman[d], t.withHeight (t.getHeight() * 0.40f).translated (0, t.getHeight() * 0.08f), t.getHeight() * 0.20f, juce::Justification::centred);
            drawText (g, juce::String (rf::chordName (d, s)), t.withTrimmedTop (t.getHeight() * 0.50f).withHeight (t.getHeight() * 0.4f), t.getHeight() * 0.17f, juce::Justification::centred);
        }
    }
    static inline const char* kScaleNames[9] = { "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "LOCRIAN", "HARMONIC MINOR", "MELODIC MINOR" };
};

// CUSTOM chord shapes: one row per scale degree, one cell per semitone above its root (0 to 21). Click a cell to add or remove that note.
// Right-click a row for ready-made shapes, or to copy it to every degree.
class ShapeEditor : public Display
{
public:
    ShapeEditor (RelicProcessor& p) : Display (p) { setTooltip ("Custom chord shapes: each row is a scale degree, each cell a note above its root (0 = the root, 4 = a major third, 7 = a fifth). Click to add or remove. Right-click a row for ready-made shapes."); }
    void paint (juce::Graphics& g) override
    {
        paintFrame (g); const auto s = proc.readChordArp(); const int hot = proc.chordDegreeShown.load();
        static const char* roman[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
        for (int c = 0; c < 22; ++c) { g.setColour (parchment.withAlpha (c % 12 == 0 ? 0.9f : 0.5f)); drawText (g, juce::String (c), colBounds (c).withHeight (12.0f).withY (grid().getY() - 13.0f), 9, juce::Justification::centred); }
        for (int d = 0; d < 7; ++d)
        {
            const auto lab = rowBounds (d).withWidth (labelWidth());
            g.setColour (s.chordOn && d == hot ? lime : parchment); drawText (g, juce::String (roman[d]) + "  " + juce::String (rf::chordName (d, s)), lab.translated (2.0f, 0), 11, juce::Justification::centredLeft);
            for (int c = 0; c < 22; ++c)
            {
                const auto b = cellBounds (d, c).reduced (1.5f); const bool on = (s.degShape[(size_t) d] & (1 << c)) != 0;
                g.setColour (on ? lime.withAlpha (s.chordOn && s.chordMode == 2 ? 0.85f : 0.45f) : lime.withAlpha (0.07f)); g.fillRoundedRectangle (b, 3);
            }
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int d = 0; d < 7; ++d)
        {
            if (! rowBounds (d).contains (e.position)) continue;
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu menu; juce::PopupMenu shapes; for (int i = 0; i < 24; ++i) shapes.addItem (1 + i, juce::String (rf::fixedShapeName (i)).isEmpty() ? "MAJ" : juce::String (rf::fixedShapeName (i)).toUpperCase());
                menu.addSubMenu ("Set this row to a shape", shapes); menu.addItem (100, "Copy this row to every degree"); menu.addItem (101, "Reset all rows (major scale triads)"); menu.addItem (102, "Clear this row");
                auto safe = juce::Component::SafePointer<ShapeEditor> (this);
                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, d] (int r)
                {
                    if (safe == nullptr) return;
                    if (r >= 1 && r <= 24) { int m = 0; for (int iv : rf::fixedShape (r - 1)) if (iv < 22) m |= 1 << iv; safe->set (d, m); }
                    if (r == 100) { const int m = (int) safe->proc.apvts.getRawParameterValue (safe->pid (d))->load(); for (int k = 0; k < 7; ++k) safe->set (k, m); }
                    if (r == 101) { static const int t[7] = { 0x91, 0x89, 0x89, 0x91, 0x91, 0x89, 0x49 }; for (int k = 0; k < 7; ++k) safe->set (k, t[k]); }
                    if (r == 102) safe->set (d, 0);
                });
                return;
            }
            for (int c = 0; c < 22; ++c) if (cellBounds (d, c).contains (e.position))
            { set (d, (int) proc.apvts.getRawParameterValue (pid (d))->load() ^ (1 << c)); return; }
        }
    }
private:
    juce::String pid (int d) const { return "chord_deg_" + juce::String (d + 1) + "_shape"; }
    float labelWidth() const { return 96.0f; }
    juce::Rectangle<float> grid() const { auto b = getLocalBounds().toFloat().reduced (12, 6); b.removeFromTop (14); return b; }
    juce::Rectangle<float> rowBounds (int d) const { const auto b = grid(); const float h = b.getHeight() / 7; return { b.getX(), b.getY() + h * d, b.getWidth(), h }; }
    juce::Rectangle<float> colBounds (int c) const { const auto b = grid(); const float w = (b.getWidth() - labelWidth()) / 22; return { b.getX() + labelWidth() + w * c, b.getY(), w, b.getHeight() }; }
    juce::Rectangle<float> cellBounds (int d, int c) const { const auto r = rowBounds (d), col = colBounds (c); return { col.getX(), r.getY(), col.getWidth(), r.getHeight() }; }
    void set (int d, int mask) { auto* p = proc.apvts.getParameter (pid (d)); p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 ((float) mask)); p->endChangeGesture(); repaint(); }
};

// CUSTOM chop kit: one row per pad (C1 upward) with the slice it plays, a pitch shift, reverse, a level and a choke group
// (pads sharing a choke group cut each other off, like a closed and an open hat).
class KitPanel : public juce::Component
{
public:
    KitPanel (RelicProcessor& p) : proc (p)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 16; ++i)
        {
            auto& r = rows[(size_t) i]; const juce::String k = "chop_kit_" + juce::String (i + 1) + "_";
            const int midi = RelicProcessor::kChopBase + i;
            r.name.setText ("PAD " + juce::String (i + 1) + "  " + names[midi % 12] + juce::String (midi / 12 - 1), juce::dontSendNotification); r.name.setFont (juce::FontOptions (12.5f)); r.name.setColour (juce::Label::textColourId, juce::Colour (0xffe8e0b0));
            auto* ps = proc.apvts.getParameter (k + "slice"); r.slice.addItemList (ps->getAllValueStrings(), 1); r.sliceA = std::make_unique<juce::ComboBoxParameterAttachment> (*ps, r.slice);
            auto* pc = proc.apvts.getParameter (k + "choke"); r.choke.addItemList (pc->getAllValueStrings(), 1); r.chokeA = std::make_unique<juce::ComboBoxParameterAttachment> (*pc, r.choke);
            for (auto* sl : { &r.pitch, &r.level }) { sl->setSliderStyle (juce::Slider::LinearHorizontal); sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20); }
            r.pitch.setTextValueSuffix (" st"); r.level.setTextValueSuffix (" dB"); r.level.setNumDecimalPlacesToDisplay (1);
            r.pitchA = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (k + "pitch"), r.pitch);
            r.levelA = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (k + "level"), r.level);
            r.rev.setButtonText ("REV"); r.revA = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (k + "rev"), r.rev);
            r.slice.setTooltip ("Which slice this pad plays: Own (the pad's own number), a slice 1 to 16, or Mute.");
            r.choke.setTooltip ("Pads in the same choke group cut each other off. Off = never.");
            for (juce::Component* c : { (juce::Component*) &r.name, (juce::Component*) &r.slice, (juce::Component*) &r.pitch, (juce::Component*) &r.rev, (juce::Component*) &r.level, (juce::Component*) &r.choke }) addAndMakeVisible (c);
        }
        for (auto* b : { &resetB, &reverseB }) addAndMakeVisible (b);
        resetB.setButtonText ("RESET KIT"); reverseB.setButtonText ("REVERSE ORDER");
        resetB.onClick = [this] { for (int i = 0; i < 16; ++i) { const juce::String k = "chop_kit_" + juce::String (i + 1) + "_"; for (auto* id : { "slice", "pitch", "rev", "level", "choke" }) setNorm (k + id, def (k + id)); } };
        reverseB.onClick = [this]
        {
            const int n = juce::jlimit (2, 16, (int) proc.apvts.getRawParameterValue ("chop_count")->load());
            for (int i = 0; i < 16; ++i) setNorm ("chop_kit_" + juce::String (i + 1) + "_slice", i < n ? (float) (n - i) / 17.0f : def ("chop_kit_" + juce::String (i + 1) + "_slice"));
        };
        setSize (660, 28 + 16 * 26 + 44);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff17131c)); g.setColour (juce::Colour (0xffe8e0b0)); g.setFont (juce::FontOptions (12.0f));
        const int y = 6; g.drawText ("SLICE", 92, y, 70, 16, juce::Justification::centred); g.drawText ("PITCH", 170, y, 130, 16, juce::Justification::centred);
        g.drawText ("LEVEL", 372, y, 130, 16, juce::Justification::centred); g.drawText ("CHOKE", 590, y, 60, 16, juce::Justification::centred);
    }
    void resized() override
    {
        for (int i = 0; i < 16; ++i)
        {
            auto& r = rows[(size_t) i]; const int y = 26 + i * 26;
            r.name.setBounds (8, y, 82, 24); r.slice.setBounds (92, y + 1, 70, 22); r.pitch.setBounds (166, y, 168, 24); r.rev.setBounds (338, y, 56, 24); r.level.setBounds (396, y, 168, 24); r.choke.setBounds (590, y + 1, 60, 22);
        }
        resetB.setBounds (8, getHeight() - 36, 120, 28); reverseB.setBounds (136, getHeight() - 36, 140, 28);
    }
private:
    struct Row { juce::Label name; juce::ComboBox slice, choke; juce::Slider pitch, level; juce::ToggleButton rev;
                 std::unique_ptr<juce::ComboBoxParameterAttachment> sliceA, chokeA; std::unique_ptr<juce::SliderParameterAttachment> pitchA, levelA; std::unique_ptr<juce::ButtonParameterAttachment> revA; };
    float def (const juce::String& id) const { return proc.apvts.getParameter (id)->getDefaultValue(); }
    void setNorm (const juce::String& id, float v) { auto* q = proc.apvts.getParameter (id); q->beginChangeGesture(); q->setValueNotifyingHost (v); q->endChangeGesture(); }
    RelicProcessor& proc; std::array<Row, 16> rows; juce::TextButton resetB, reverseB;
};

// The vibrato's shape over a note: wait (Delay), grow (Fade-in), settle (Decay to Sustain), plus how much velocity and the mod wheel matter. The curve at the top follows the sliders.
class VibratoPanel : public juce::Component, private juce::Timer
{
public:
    VibratoPanel (RelicProcessor& p) : proc (p)
    {
        static const struct { const char* id; const char* label; const char* suffix; } rows[7] = { { "vib_delay_ms", "DELAY", " ms" }, { "vib_fade_ms", "FADE-IN", " ms" }, { "vib_decay_ms", "DECAY", " ms" },
                                                                                             { "vib_sustain", "SUSTAIN", "" }, { "vib_release_ms", "RELEASE", " ms" }, { "vib_vel", "VELOCITY", "" }, { "vib_wheel", "MOD WHEEL", " st" } };
        for (int i = 0; i < 7; ++i)
        {
            auto& r = row[(size_t) i]; r.label.setText (rows[i].label, juce::dontSendNotification); r.label.setColour (juce::Label::textColourId, juce::Colour (0xffe8e0b0)); r.label.setFont (juce::FontOptions (12.5f));
            r.slider.setSliderStyle (juce::Slider::LinearHorizontal); r.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20); r.slider.setTextValueSuffix (rows[i].suffix);
            r.slider.setNumDecimalPlacesToDisplay (juce::String (rows[i].suffix) == " ms" ? 0 : 2); r.slider.setTooltip (help::forId (rows[i].id));
            r.att = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (rows[i].id), r.slider); addAndMakeVisible (r.label); addAndMakeVisible (r.slider);
        }
        setSize (420, 112 + 7 * 28 + 10); startTimerHz (20);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff17131c)); const auto box = juce::Rectangle<float> (12.0f, 10.0f, (float) getWidth() - 24.0f, 92.0f);
        g.setColour (juce::Colour (0xff0b090e)); g.fillRoundedRectangle (box, 4.0f);
        const float d = val ("vib_delay_ms"), f = val ("vib_fade_ms"), dc = val ("vib_decay_ms"), su = val ("vib_sustain");
        const float total = std::max (600.0f, (d + f + dc) * 1.25f + 200.0f), hold = total - (d + f + dc);
        auto X = [&] (float ms) { return box.getX() + 6.0f + (box.getWidth() - 12.0f) * ms / total; }; auto Y = [&] (float v) { return box.getBottom() - 8.0f - (box.getHeight() - 16.0f) * v; };
        juce::Path pth; pth.startNewSubPath (X (0), Y (0)); pth.lineTo (X (d), Y (0)); pth.lineTo (X (d + f), Y (1)); pth.lineTo (X (d + f + dc), Y (su)); const float rel = val ("vib_release_ms"), kOff = d + f + dc + hold * 0.6f; pth.lineTo (X (kOff), Y (su)); if (rel > 0.0f) pth.lineTo (X (std::min (total, kOff + rel)), Y (0)); else { pth.lineTo (X (total), Y (su)); }
        g.setColour (juce::Colour (0xff9be15d)); g.strokePath (pth, juce::PathStrokeType (2.0f));
        g.setColour (juce::Colour (0xffe8e0b0).withAlpha (0.6f)); g.setFont (juce::FontOptions (11.0f)); g.drawText ("depth over one note (the note's release ends it)", box.reduced (8, 3).toNearestInt(), juce::Justification::topRight);
    }
    void resized() override { for (int i = 0; i < 7; ++i) { auto& r = row[(size_t) i]; const int y = 112 + i * 28; r.label.setBounds (14, y, 84, 24); r.slider.setBounds (100, y, getWidth() - 112, 24); } }
private:
    struct Row { juce::Label label; juce::Slider slider; std::unique_ptr<juce::SliderParameterAttachment> att; };
    float val (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void timerCallback() override { repaint (0, 0, getWidth(), 106); }
    RelicProcessor& proc; std::array<Row, 7> row;
};

// The tape stage's distortion settings: which shape (Type), which part of the spectrum it works on (Band, with the two split frequencies), and its Bias.
class DistortionPanel : public juce::Component
{
public:
    DistortionPanel (RelicProcessor& p)
    {
        auto addCombo = [&] (juce::ComboBox& cb, const char* id, const char* label, int y, std::unique_ptr<juce::ComboBoxParameterAttachment>& att, juce::Label& lb)
        {
            auto* par = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id)); cb.addItemList (par->choices, 1); att = std::make_unique<juce::ComboBoxParameterAttachment> (*par, cb); cb.setTooltip (help::forId (id));
            lb.setText (label, juce::dontSendNotification); lb.setColour (juce::Label::textColourId, juce::Colour (0xffe8e0b0)); lb.setFont (juce::FontOptions (12.5f)); addAndMakeVisible (lb); addAndMakeVisible (cb); (void) y;
        };
        addCombo (type, "tape_type", "TYPE", 0, typeA, typeL); addCombo (band, "tape_band", "BAND", 0, bandA, bandL);
        static const struct { const char* id; const char* label; const char* suffix; int dp; } rows[3] = { { "tape_bias", "BIAS", "", 2 }, { "tape_lo_hz", "LOW SPLIT", " Hz", 0 }, { "tape_hi_hz", "HIGH SPLIT", " Hz", 0 } };
        for (int i = 0; i < 3; ++i)
        {
            auto& r = row[(size_t) i]; r.label.setText (rows[i].label, juce::dontSendNotification); r.label.setColour (juce::Label::textColourId, juce::Colour (0xffe8e0b0)); r.label.setFont (juce::FontOptions (12.5f));
            r.slider.setSliderStyle (juce::Slider::LinearHorizontal); r.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 20); r.slider.setTextValueSuffix (rows[i].suffix); r.slider.setNumDecimalPlacesToDisplay (rows[i].dp);
            r.slider.setTooltip (help::forId (rows[i].id)); r.att = std::make_unique<juce::SliderParameterAttachment> (*p.apvts.getParameter (rows[i].id), r.slider); addAndMakeVisible (r.label); addAndMakeVisible (r.slider);
        }
        setSize (400, 5 * 32 + 16);
    }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff17131c)); }
    void resized() override
    {
        typeL.setBounds (14, 10, 84, 24); type.setBounds (100, 10, getWidth() - 114, 24); bandL.setBounds (14, 42, 84, 24); band.setBounds (100, 42, getWidth() - 114, 24);
        for (int i = 0; i < 3; ++i) { auto& r = row[(size_t) i]; const int y = 74 + i * 32; r.label.setBounds (14, y, 84, 24); r.slider.setBounds (100, y, getWidth() - 112, 24); }
    }
private:
    struct Row { juce::Label label; juce::Slider slider; std::unique_ptr<juce::SliderParameterAttachment> att; };
    juce::ComboBox type, band; juce::Label typeL, bandL; std::unique_ptr<juce::ComboBoxParameterAttachment> typeA, bandA; std::array<Row, 3> row;
};

// A modulation slot's response drawn by hand: nine points, drag across the graph to paint them. The horizontal axis is how far the source is from zero (0 to 1), the vertical
// axis how much of it passes (0 to 1); a negative source keeps its sign.
class CurveGraph : public juce::Component
{
public:
    CurveGraph (RelicProcessor& p, int slotNo) : proc (p), slot (slotNo) { setMouseCursor (juce::MouseCursor::CrosshairCursor); }
    void paint (juce::Graphics& g) override
    {
        const auto b = area(); g.setColour (juce::Colour (0xff0b090e)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setColour (juce::Colour (0xffe8e0b0).withAlpha (0.18f));
        for (int i = 0; i <= 4; ++i) { g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * i / 4.0f), b.getX(), b.getRight()); g.drawVerticalLine ((int) (b.getX() + b.getWidth() * i / 4.0f), b.getY(), b.getBottom()); }
        float pts[rf::kCurvePts]; for (int j = 0; j < rf::kCurvePts; ++j) pts[j] = get (j);
        const bool on = proc.apvts.getRawParameterValue (id ("cvmode"))->load() > 0.5f;
        g.setColour (juce::Colour (0xffe8e0b0).withAlpha (0.35f)); g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getY(), 1.0f);      // the straight line, for comparison
        juce::Path path; for (int i = 0; i <= 160; ++i) { const float x = (float) i / 160.0f, y = rf::curveEval (pts, x); const float px = b.getX() + x * b.getWidth(), py = b.getBottom() - y * b.getHeight(); if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py); }
        g.setColour (on ? juce::Colour (0xff9be15d) : juce::Colour (0xff9be15d).withAlpha (0.4f)); g.strokePath (path, juce::PathStrokeType (2.2f));
        for (int j = 0; j < rf::kCurvePts; ++j) { const float px = b.getX() + b.getWidth() * (float) j / (float) (rf::kCurvePts - 1), py = b.getBottom() - pts[j] * b.getHeight(); g.setColour (juce::Colour (0xffe8e0b0)); g.fillEllipse (px - 4.5f, py - 4.5f, 9.0f, 9.0f); }
    }
    void mouseDown (const juce::MouseEvent& e) override { for (int j = 0; j < rf::kCurvePts; ++j) proc.apvts.getParameter (id ("cv" + juce::String (j + 1)))->beginChangeGesture(); touched = true; lastCol = -1; paintAt (e); }
    void mouseDrag (const juce::MouseEvent& e) override { paintAt (e); }
    void mouseUp (const juce::MouseEvent&) override { if (touched) for (int j = 0; j < rf::kCurvePts; ++j) proc.apvts.getParameter (id ("cv" + juce::String (j + 1)))->endChangeGesture(); touched = false; lastCol = -1; }
    void setAll (const float* v) { for (int j = 0; j < rf::kCurvePts; ++j) { auto* q = proc.apvts.getParameter (id ("cv" + juce::String (j + 1))); q->beginChangeGesture(); q->setValueNotifyingHost (v[j]); q->endChangeGesture(); } repaint(); }
private:
    juce::String id (const juce::String& k) const { return "mod" + juce::String (slot) + "_" + k; }
    float get (int j) const { return proc.apvts.getRawParameterValue (id ("cv" + juce::String (j + 1)))->load(); }
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (14.0f, 12.0f); }
    void paintAt (const juce::MouseEvent& e)
    {
        const auto b = area(); const int j = juce::jlimit (0, rf::kCurvePts - 1, (int) std::lround ((e.position.x - b.getX()) / b.getWidth() * (float) (rf::kCurvePts - 1)));
        const float y = juce::jlimit (0.0f, 1.0f, (b.getBottom() - e.position.y) / b.getHeight());
        proc.apvts.getParameter (id ("cv" + juce::String (j + 1)))->setValueNotifyingHost (y);
        // a quick drag skips columns: fill the ones between the last and this one so the line stays continuous
        if (lastCol >= 0 && std::abs (j - lastCol) > 1) for (int k = std::min (j, lastCol) + 1; k < std::max (j, lastCol); ++k) proc.apvts.getParameter (id ("cv" + juce::String (k + 1)))->setValueNotifyingHost (lastY + (y - lastY) * (float) (k - lastCol) / (float) (j - lastCol));
        lastCol = j; lastY = y;
        if (auto* m = proc.apvts.getParameter (id ("cvmode"))) if (m->getValue() < 0.5f) m->setValueNotifyingHost (1.0f);       // drawing turns the drawn curve on
        repaint();
    }
    RelicProcessor& proc; int slot; bool touched = false; int lastCol = -1; float lastY = 0.0f;
};

class CurvePanel : public juce::Component, private juce::Timer
{
public:
    CurvePanel (RelicProcessor& p, int slotNo) : proc (p), graph (p, slotNo)
    {
        addAndMakeVisible (graph); const juce::String t = "mod" + juce::String (slotNo) + "_";
        use.setButtonText ("USE DRAWN CURVE"); use.setTooltip ("On: the slot follows the drawn curve. Off: it uses the CURVE bend.");
        useA = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (t + "cvmode"), use); addAndMakeVisible (use);
        title.setText ("MOD " + juce::String (slotNo) + " CURVE", juce::dontSendNotification); title.setColour (juce::Label::textColourId, juce::Colour (0xffe8e0b0)); title.setFont (juce::FontOptions (14.0f)); addAndMakeVisible (title);
        for (auto* b : { &lineB, &presetB }) addAndMakeVisible (b);
        lineB.setButtonText ("STRAIGHT LINE"); presetB.setButtonText ("SHAPES...");
        lineB.onClick = [this] { float v[rf::kCurvePts]; for (int j = 0; j < rf::kCurvePts; ++j) v[j] = (float) j / (float) (rf::kCurvePts - 1); graph.setAll (v); };
        presetB.onClick = [this]
        {
            juce::PopupMenu m; const char* names[6] = { "S curve", "Slow start", "Fast start", "Steps", "Valley", "Hill" }; for (int i = 0; i < 6; ++i) m.addItem (i + 1, names[i]);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetB), [this] (int r)
            {
                if (r < 1) return; float v[rf::kCurvePts];
                for (int j = 0; j < rf::kCurvePts; ++j)
                {
                    const float x = (float) j / (float) (rf::kCurvePts - 1);
                    switch (r) { case 1: v[j] = x * x * (3.0f - 2.0f * x); break; case 2: v[j] = x * x; break; case 3: v[j] = std::sqrt (x); break; case 4: v[j] = std::floor (x * 4.0f + 0.001f) / 4.0f; if (v[j] > 1.0f) v[j] = 1.0f; break;
                                 case 5: v[j] = std::abs (2.0f * x - 1.0f); break; default: v[j] = 1.0f - std::abs (2.0f * x - 1.0f); }
                }
                graph.setAll (v);
            });
        };
        setSize (400, 330); startTimerHz (10);
    }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff17131c)); }
    void resized() override { title.setBounds (12, 8, 200, 22); use.setBounds (216, 8, 176, 22); graph.setBounds (12, 36, getWidth() - 24, 240); lineB.setBounds (12, 288, 150, 28); presetB.setBounds (170, 288, 120, 28); }
private:
    void timerCallback() override { graph.repaint(); }
    RelicProcessor& proc; CurveGraph graph; juce::Label title; juce::ToggleButton use; juce::TextButton lineB, presetB; std::unique_ptr<juce::ButtonParameterAttachment> useA;
};


// One LFO's cycle drawn by hand: 32 points joined smoothly and wrapping round. Drag across the graph to paint; the LFO uses it when its Shape is Custom.
class LfoGraph : public juce::Component
{
public:
    LfoGraph (RelicProcessor& p, int lfoNo, const juce::String& laneId = {}) : proc (p), lfo (lfoNo), pre (laneId.isEmpty() ? "lfo" + juce::String (lfoNo) : laneId), shapeId (laneId.isEmpty() ? pre + "_shape" : laneId + "_src"), customIdx (laneId.isEmpty() ? (int) rf::ModLfo::Custom : (int) rf::AsDraw) { setMouseCursor (juce::MouseCursor::CrosshairCursor); }
    void paint (juce::Graphics& g) override
    {
        const auto b = area(); g.setColour (juce::Colour (0xff0b090e)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setColour (juce::Colour (0xffe8e0b0).withAlpha (0.18f));
        for (int i = 0; i <= 4; ++i) { g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * i / 4.0f), b.getX(), b.getRight()); g.drawVerticalLine ((int) (b.getX() + b.getWidth() * i / 4.0f), b.getY(), b.getBottom()); }
        g.setColour (juce::Colour (0xffe8e0b0).withAlpha (0.4f)); g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * 0.5f), b.getX(), b.getRight());
        float w[rf::kLfoPts]; for (int k = 0; k < rf::kLfoPts; ++k) w[k] = get (k);
        const bool on = (int) proc.apvts.getRawParameterValue (shapeId)->load() == customIdx;
        juce::Path path; for (int i = 0; i <= 240; ++i) { const double ph = i / 240.0; const float y = (rf::lfoCustomEval (w, ph) + 1.0f) * 0.5f; const float px = b.getX() + (float) ph * b.getWidth(), py = b.getBottom() - y * b.getHeight(); if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py); }
        g.setColour (on ? juce::Colour (0xff9be15d) : juce::Colour (0xff9be15d).withAlpha (0.4f)); g.strokePath (path, juce::PathStrokeType (2.2f));
        for (int k = 0; k < rf::kLfoPts; ++k) { g.setColour (juce::Colour (0xffe8e0b0)); g.fillEllipse (b.getX() + b.getWidth() * (float) k / (float) rf::kLfoPts - 2.5f, b.getBottom() - w[k] * b.getHeight() - 2.5f, 5.0f, 5.0f); }
    }
    void mouseDown (const juce::MouseEvent& e) override { for (int k = 0; k < rf::kLfoPts; ++k) par (k)->beginChangeGesture(); touched = true; lastCol = -1; paintAt (e); }
    void mouseDrag (const juce::MouseEvent& e) override { paintAt (e); }
    void mouseUp (const juce::MouseEvent&) override { if (touched) for (int k = 0; k < rf::kLfoPts; ++k) par (k)->endChangeGesture(); touched = false; lastCol = -1; }
    void setAll (const float* v) { for (int k = 0; k < rf::kLfoPts; ++k) { auto* q = par (k); q->beginChangeGesture(); q->setValueNotifyingHost (v[k]); q->endChangeGesture(); } useCustom(); repaint(); }
private:
    juce::RangedAudioParameter* par (int k) const { return proc.apvts.getParameter (pre + "_w" + juce::String (k + 1)); }
    float get (int k) const { return proc.apvts.getRawParameterValue (pre + "_w" + juce::String (k + 1))->load(); }
    void useCustom() { if (auto* m = proc.apvts.getParameter (shapeId)) m->setValueNotifyingHost (m->convertTo0to1 ((float) customIdx)); }       // drawing switches the LFO to Custom
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (14.0f, 12.0f); }
    void paintAt (const juce::MouseEvent& e)
    {
        const auto b = area(); const int k = juce::jlimit (0, rf::kLfoPts - 1, (int) std::floor ((e.position.x - b.getX()) / b.getWidth() * (float) rf::kLfoPts + 0.5f) % rf::kLfoPts);
        const float y = juce::jlimit (0.0f, 1.0f, (b.getBottom() - e.position.y) / b.getHeight());
        par (k)->setValueNotifyingHost (y);
        if (lastCol >= 0 && std::abs (k - lastCol) > 1) for (int j = std::min (k, lastCol) + 1; j < std::max (k, lastCol); ++j) par (j)->setValueNotifyingHost (lastY + (y - lastY) * (float) (j - lastCol) / (float) (k - lastCol));
        lastCol = k; lastY = y; useCustom(); repaint();
    }
    RelicProcessor& proc; int lfo; juce::String pre, shapeId; int customIdx; bool touched = false; int lastCol = -1; float lastY = 0.0f;
};

class LfoPanel : public juce::Component, private juce::Timer
{
public:
    LfoPanel (RelicProcessor& p, int lfoNo) : graph (p, lfoNo)
    {
        addAndMakeVisible (graph); title.setText ("LFO " + juce::String (lfoNo) + " DRAWN WAVE", juce::dontSendNotification); title.setColour (juce::Label::textColourId, juce::Colour (0xffe8e0b0)); title.setFont (juce::FontOptions (14.0f)); addAndMakeVisible (title);
        hint.setText ("drag to draw one cycle; the LFO uses it when its Shape is Custom", juce::dontSendNotification); hint.setColour (juce::Label::textColourId, juce::Colour (0xffe8e0b0).withAlpha (0.6f)); hint.setFont (juce::FontOptions (11.0f)); addAndMakeVisible (hint);
        static const char* names[6] = { "SINE", "TRI", "SAW UP", "SAW DOWN", "SQUARE", "SMOOTH RANDOM" };
        for (int i = 0; i < 6; ++i)
        {
            auto& b = btn[(size_t) i]; b.setButtonText (names[i]); addAndMakeVisible (b);
            b.onClick = [this, i]
            {
                float v[rf::kLfoPts]; uint32_t r = (uint32_t) juce::Time::getMillisecondCounter() * 2654435761u + 12345u;
                for (int k = 0; k < rf::kLfoPts; ++k)
                {
                    const float x = (float) k / (float) rf::kLfoPts; r = r * 1664525u + 1013904223u;
                    switch (i) { case 0: v[k] = 0.5f + 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * x); break; case 1: v[k] = 1.0f - std::abs (2.0f * x - 1.0f); break; case 2: v[k] = x; break;
                                 case 3: v[k] = 1.0f - x; break; case 4: v[k] = x < 0.5f ? 1.0f : 0.0f; break; default: v[k] = (float) (r >> 9) / 8388608.0f; }
                }
                graph.setAll (v);
            };
        }
        setSize (440, 330); startTimerHz (10);
    }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff17131c)); }
    void resized() override { title.setBounds (12, 8, 220, 22); hint.setBounds (12, 30, getWidth() - 24, 16); graph.setBounds (12, 50, getWidth() - 24, 220); for (int i = 0; i < 6; ++i) btn[(size_t) i].setBounds (12 + i * 70, 282, 66, 28); }
private:
    void timerCallback() override { graph.repaint(); }
    LfoGraph graph; juce::Label title, hint; std::array<juce::TextButton, 6> btn;
};

// The automation lanes and the fan-out LFO: pick a lane, edit its generator, target, timing and drawn cycle; below, the global intensity and the fan-out LFO with its eleven targets.
class AutomationPanel : public juce::Component, private juce::Timer
{
public:
    AutomationPanel (RelicProcessor& p) : proc (p)
    {
        auto label = [&] (juce::Label& l, const char* t) { l.setText (t, juce::dontSendNotification); l.setColour (juce::Label::textColourId, juce::Colour (0xffe8e0b0)); l.setFont (juce::FontOptions (12.0f)); addAndMakeVisible (l); };
        for (int k = 0; k < rf::kAutoLanes; ++k)
        {
            auto& b = laneBtn[(size_t) k]; b.setButtonText (juce::String (k + 1)); b.setClickingTogglesState (true); b.setRadioGroupId (771); addAndMakeVisible (b);
            b.onClick = [this, k] { bindLane (k + 1); };
        }
        for (auto* c : { &generator, &target, &division, &feel }) addAndMakeVisible (*c);
        { auto* par = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("auto1_src")); generator.addItemList (par->choices, 1); }
        { auto* par = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("auto1_dst")); target.addItemList (par->choices, 1); }
        { auto* par = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("auto1_div")); division.addItemList (par->choices, 1); }
        { auto* par = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("auto1_feel")); feel.addItemList (par->choices, 1); }
        laneOn.setButtonText ("ON"); trig.setButtonText ("RETRIGGER ON NOTE"); addAndMakeVisible (laneOn); addAndMakeVisible (trig);
        static const char* sl[6] = { "DEPTH", "FREE HZ", "PHASE", "ENV ATTACK", "ENV DECAY", "JOY LINK*" };
        for (int i = 0; i < 6; ++i) { label (sLab[(size_t) i], sl[i]); auto& s = laneS[(size_t) i]; s.setSliderStyle (juce::Slider::LinearHorizontal); s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18); addAndMakeVisible (s); }
        label (gLab, "GENERATOR"); label (tLab, "TARGET"); label (dLab, "DIVISION"); label (fLab, "FEEL");
        label (heatLab, "INTENSITY"); heat.setSliderStyle (juce::Slider::LinearHorizontal); heat.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18); addAndMakeVisible (heat);
        heatA = std::make_unique<juce::SliderParameterAttachment> (*p.apvts.getParameter ("mod_heat"), heat); heat.setTooltip (help::forId ("mod_heat"));
        label (fanLab, "FAN-OUT LFO  (one generator, up to eleven targets)");
        llOnT.setButtonText ("ON"); addAndMakeVisible (llOnT); llOnA = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter ("llfo_on"), llOnT);
        for (auto* c : { &llShape, &llSync }) addAndMakeVisible (*c);
        { auto* par = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("llfo_shape")); llShape.addItemList (par->choices, 1); llShapeA = std::make_unique<juce::ComboBoxParameterAttachment> (*par, llShape); }
        { auto* par = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("llfo_sync")); llSync.addItemList (par->choices, 1); llSyncA = std::make_unique<juce::ComboBoxParameterAttachment> (*par, llSync); }
        for (int i = 0; i < 2; ++i) { auto& s = llS[(size_t) i]; s.setSliderStyle (juce::Slider::LinearHorizontal); s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18); addAndMakeVisible (s); }
        llSA[0] = std::make_unique<juce::SliderParameterAttachment> (*p.apvts.getParameter ("llfo_rate_hz"), llS[0]); llSA[1] = std::make_unique<juce::SliderParameterAttachment> (*p.apvts.getParameter ("llfo_depth"), llS[1]);
        label (llLab[0], "RATE"); label (llLab[1], "DEPTH");
        static const char* tn[11] = { "CUTOFF", "RESONANCE", "DELAY MIX", "REVERB MIX", "CHORUS MIX", "FLANGER MIX", "WAH FREQ", "CRUSH MIX", "STEREO WIDTH", "TREMOLO DEPTH", "OSC/SAMPLE MIX" };
        for (int b = 0; b < 11; ++b)
        {
            const auto n = juce::String (b + 1); auto& t = llT[(size_t) b]; t.setButtonText (tn[b]); addAndMakeVisible (t); llTA[(size_t) b] = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter ("llfo_t" + n), t);
            auto& s = llA[(size_t) b]; s.setSliderStyle (juce::Slider::LinearHorizontal); s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); addAndMakeVisible (s); llAA[(size_t) b] = std::make_unique<juce::SliderParameterAttachment> (*p.apvts.getParameter ("llfo_a" + n), s);
            t.setTooltip (help::forId ("llfo_t1")); s.setTooltip (help::forId ("llfo_a1"));
        }
        note.setText ("* stored from imports, no effect yet.  Pitch, pan, master and synth volume depths are calibrated; other targets are estimates.", juce::dontSendNotification); note.setColour (juce::Label::textColourId, juce::Colour (0xffe8e0b0).withAlpha (0.55f)); note.setFont (juce::FontOptions (10.5f)); addAndMakeVisible (note);
        laneBtn[0].setToggleState (true, juce::dontSendNotification); bindLane (1);
        setSize (760, 540); startTimerHz (10);
    }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff17131c)); g.setColour (juce::Colour (0xffe8e0b0).withAlpha (0.2f)); g.drawHorizontalLine (322, 12.0f, (float) getWidth() - 12.0f); }
    void resized() override
    {
        for (int k = 0; k < rf::kAutoLanes; ++k) laneBtn[(size_t) k].setBounds (12 + k * 44, 10, 40, 26);
        heatLab.setBounds (380, 10, 80, 26); heat.setBounds (460, 10, 288, 26);
        laneOn.setBounds (12, 46, 60, 24); gLab.setBounds (84, 46, 78, 24); generator.setBounds (162, 46, 130, 24); tLab.setBounds (304, 46, 56, 24); target.setBounds (362, 46, 150, 24);
        dLab.setBounds (12, 80, 70, 24); division.setBounds (84, 80, 90, 24); fLab.setBounds (186, 80, 40, 24); feel.setBounds (228, 80, 96, 24); trig.setBounds (338, 80, 170, 24);
        for (int i = 0; i < 6; ++i) { sLab[(size_t) i].setBounds (12, 114 + i * 28, 84, 24); laneS[(size_t) i].setBounds (98, 114 + i * 28, 250, 24); }
        if (graph) graph->setBounds (360, 112, 388, 170);
        for (int i = 0; i < 6; ++i) shapeBtn[(size_t) i].setBounds (360 + i * 65, 288, 62, 24);
        fanLab.setBounds (12, 330, 500, 22); llOnT.setBounds (12, 356, 60, 24); llShape.setBounds (80, 356, 100, 24); llLab[0].setBounds (192, 356, 36, 24); llS[0].setBounds (230, 356, 190, 24); llSync.setBounds (432, 356, 90, 24);
        llLab[1].setBounds (534, 356, 44, 24); llS[1].setBounds (580, 356, 170, 24);
        for (int b = 0; b < 11; ++b) { const int col = b % 2, row = b / 2; llT[(size_t) b].setBounds (12 + col * 372, 394 + row * 22, 150, 20); llA[(size_t) b].setBounds (166 + col * 372, 394 + row * 22, 200, 20); }
        note.setBounds (12, 514, getWidth() - 24, 18);
    }
private:
    void timerCallback() override { if (graph) graph->repaint(); }
    void bindLane (int n)
    {
        lane = n; const auto t = "auto" + juce::String (n); auto& a = proc.apvts;
        laneOnA.reset(); trigA.reset(); genA.reset(); tgtA.reset(); divA.reset(); feelA.reset(); for (auto& x : laneSA) x.reset();
        laneOnA = std::make_unique<juce::ButtonParameterAttachment> (*a.getParameter (t + "_on"), laneOn); trigA = std::make_unique<juce::ButtonParameterAttachment> (*a.getParameter (t + "_trig"), trig);
        genA = std::make_unique<juce::ComboBoxParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (a.getParameter (t + "_src")), generator);
        tgtA = std::make_unique<juce::ComboBoxParameterAttachment> (*a.getParameter (t + "_dst"), target); divA = std::make_unique<juce::ComboBoxParameterAttachment> (*a.getParameter (t + "_div"), division);
        feelA = std::make_unique<juce::ComboBoxParameterAttachment> (*a.getParameter (t + "_feel"), feel);
        static const char* ids[6] = { "_depth", "_hz", "_phase", "_env_a_ms", "_env_d_ms", "_joy" };
        for (int i = 0; i < 6; ++i) { laneSA[(size_t) i] = std::make_unique<juce::SliderParameterAttachment> (*a.getParameter (t + ids[i]), laneS[(size_t) i]); laneS[(size_t) i].setTooltip (help::forId (t + ids[i])); }
        laneOn.setTooltip (help::forId (t + "_on")); trig.setTooltip (help::forId (t + "_trig")); generator.setTooltip (help::forId (t + "_src")); target.setTooltip (help::forId (t + "_dst"));
        division.setTooltip (help::forId (t + "_div")); feel.setTooltip (help::forId (t + "_feel"));
        graph = std::make_unique<LfoGraph> (proc, n, t); addAndMakeVisible (*graph);
        static const char* names[6] = { "SINE", "TRI", "SAW UP", "SAW DOWN", "SQUARE", "S.RANDOM" };
        for (int i = 0; i < 6; ++i)
        {
            auto& b = shapeBtn[(size_t) i]; b.setButtonText (names[i]); addAndMakeVisible (b);
            b.onClick = [this, i]
            {
                float v[rf::kAutoPts]; uint32_t r = (uint32_t) juce::Time::getMillisecondCounter() * 2654435761u + 12345u;
                for (int k = 0; k < rf::kAutoPts; ++k)
                {
                    const float x = (float) k / (float) rf::kAutoPts; r = r * 1664525u + 1013904223u;
                    switch (i) { case 0: v[k] = 0.5f + 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * x); break; case 1: v[k] = 1.0f - std::abs (2.0f * x - 1.0f); break; case 2: v[k] = x; break;
                                 case 3: v[k] = 1.0f - x; break; case 4: v[k] = x < 0.5f ? 1.0f : 0.0f; break; default: v[k] = (float) (r >> 9) / 8388608.0f; }
                }
                if (graph) graph->setAll (v);
            };
        }
        resized(); repaint();
    }
    RelicProcessor& proc; int lane = 1;
    std::array<juce::TextButton, rf::kAutoLanes> laneBtn; std::array<juce::TextButton, 6> shapeBtn;
    juce::ComboBox generator, target, division, feel; juce::ToggleButton laneOn, trig; std::array<juce::Slider, 6> laneS; std::array<juce::Label, 6> sLab; juce::Label gLab, tLab, dLab, fLab, heatLab, fanLab, note;
    std::unique_ptr<juce::ButtonParameterAttachment> laneOnA, trigA; std::unique_ptr<juce::ComboBoxParameterAttachment> genA, tgtA, divA, feelA; std::array<std::unique_ptr<juce::SliderParameterAttachment>, 6> laneSA;
    std::unique_ptr<LfoGraph> graph; juce::Slider heat; std::unique_ptr<juce::SliderParameterAttachment> heatA;
    juce::ToggleButton llOnT; std::unique_ptr<juce::ButtonParameterAttachment> llOnA; juce::ComboBox llShape, llSync; std::unique_ptr<juce::ComboBoxParameterAttachment> llShapeA, llSyncA;
    std::array<juce::Slider, 2> llS; std::array<std::unique_ptr<juce::SliderParameterAttachment>, 2> llSA; std::array<juce::Label, 2> llLab;
    std::array<juce::ToggleButton, 11> llT; std::array<juce::Slider, 11> llA; std::array<std::unique_ptr<juce::ButtonParameterAttachment>, 11> llTA; std::array<std::unique_ptr<juce::SliderParameterAttachment>, 11> llAA;
};

// Bars for one pass of the arpeggio on a demo triad; the bar for the step that just played lights up.
class ArpDisplay : public Display
{
public:
    using Display::Display;
    void paint (juce::Graphics& g) override
    {
        paintFrame (g);
        const auto s = proc.readChordArp(); const int hot = proc.arpStepShown.load();
        const int N = 3 * juce::jlimit (1, 4, s.octaves); std::vector<int> pos;
        auto up = [&] { std::vector<int> v; for (int i = 0; i < N; ++i) v.push_back (i); return v; };
        auto down = [&] { auto v = up(); std::reverse (v.begin(), v.end()); return v; };
        switch (s.mode)
        {
            case 1: pos = down(); break;
            case 2: pos = up(); for (int i = N - 2; i >= 1; --i) pos.push_back (i); break;
            case 3: pos = down(); for (int i = 1; i <= N - 2; ++i) pos.push_back (i); break;
            case 4: { static const int r[] = { 0, 2, 1, 2, 0, 1 }; for (int i = 0; i < N; ++i) pos.push_back ((r[i % 6] + i * 2) % N); break; }
            case 6: pos.assign ((size_t) N, -1); break;
            case 7: for (int k = 0; k < 16; ++k) pos.push_back (rf::waterfallPos (s, k, N)); break;       // Waterfall (shared with the engine)
            default: pos = up(); break;
        }
        if (pos.empty()) pos = { 0 };
        const int steps = 16; auto area = getLocalBounds().toFloat().reduced (18.0f, 20.0f); area.removeFromTop (area.getHeight() * 0.16f);
        static const char* rates[] = { "1/1", "1/2", "1/2D", "1/2T", "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T", "1/16", "1/16D", "1/16T", "1/32" }; static const char* modes[] = { "UP", "DOWN", "UP-DOWN", "DOWN-UP", "RANDOM", "PLAYED", "CHORD", "WATERFALL", "CUSTOM" };
        g.setColour (lime.withAlpha (s.arpOn ? 0.85f : 0.4f));
        drawText (g, juce::String (modes[juce::jlimit (0, 8, s.mode)]) + "   " + rates[juce::jlimit (0, 13, s.grouping != 0 && s.mode == 7 ? 10 : s.rate)] + "   " + juce::String (s.octaves) + " OCT" + (! s.arpOn ? "   (ARP OFF)" : ! s.run && proc.arpWaitingShown.load() ? "   (STOP)" : ""),
                  juce::Rectangle<float> (18.0f, 8.0f, (float) getWidth() - 36.0f, (float) getHeight() * 0.14f), (float) getHeight() * 0.09f, juce::Justification::left);
        const float cw = area.getWidth() / steps;
        for (int i = 0; i < steps; ++i)
        {
            const int p = pos[(size_t) (i % (int) pos.size())];
            const float h = p < 0 ? 1.0f : (float) (p + 1) / (float) N, bw = cw * (0.2f + 0.7f * s.gate);
            const float x = area.getX() + i * cw + ((i % 2) ? cw * s.swing * 0.5f : 0.0f);
            auto bar = juce::Rectangle<float> (x, area.getBottom() - area.getHeight() * h, bw, area.getHeight() * h);
            const bool lit = s.arpOn && hot >= 0 && (hot % steps) == i;
            g.setColour (lit ? lime : lime.withAlpha (s.arpOn ? 0.45f : 0.2f)); g.fillRoundedRectangle (bar, 3.0f);
        }
    }
};

// Original rhythm editor: click hit/rest, Shift-click tie, vertical drag velocity.
class RhythmLane : public Display
{
public:
    RhythmLane (RelicProcessor& p) : Display (p)
    {
        setTooltip ("Click: hit/rest. Shift-click: tie. Drag vertically: velocity (6% minimum hit). Right-click: length/reset.");
    }
    ~RhythmLane() override { if (dragged && cell >= 0) proc.apvts.getParameter (id (cell, "velocity"))->endChangeGesture(); }
    void paint (juce::Graphics& g) override
    {
        paintFrame (g); const auto s = proc.readChordArp();
        const int hot = proc.arpStepShown.load() - 1;
        for (int i = 0; i < 16; ++i)
        {
            auto b = cellBounds (i); const bool enabled = s.rhythmOn && i < s.rhythmLength;
            const auto c = s.rhythm[(size_t) i];
            g.setColour (lime.withAlpha (enabled ? 0.16f : 0.04f)); g.fillRoundedRectangle (b, 3);
            if (enabled && hot >= 0 && hot % s.rhythmLength == i)
            { g.setColour (gold); g.drawRoundedRectangle (b, 3, 1.5f); }
            g.setColour (lime.withAlpha (enabled ? 0.8f : 0.25f));
            if (c.type == 0)
            {
                auto bar = b.reduced (5, 4); bar.removeFromTop (bar.getHeight() * (1 - juce::jmax (0.06f, c.velocity)));
                g.fillRect (bar);
            }
            else drawText (g, c.type == 1 ? "-" : "~", b, b.getHeight() * 0.6f, juce::Justification::centred);
            g.setColour (parchment.withAlpha (i < s.rhythmLength ? 0.8f : 0.3f));
            drawText (g, juce::String (i + 1), b.withY (4).withHeight (12), 9, juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu; const auto s = proc.readChordArp();
            for (int n = 1; n <= 16; ++n) menu.addItem (n, juce::String (n) + " steps", true, n == s.rhythmLength);
            menu.addSeparator(); menu.addItem (100, "Reset: all hits, full velocity");
            auto safe = juce::Component::SafePointer<RhythmLane> (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
            {
                if (safe == nullptr) return;
                if (result >= 1 && result <= 16) safe->set ("arp_rhythm_length", (float) result);
                if (result == 100) for (int i = 0; i < 16; ++i)
                { safe->set (safe->id (i, "type"), 0); safe->set (safe->id (i, "velocity"), 1); }
            });
            return;
        }
        cell = -1;
        for (int i = 0; i < proc.readChordArp().rhythmLength; ++i) if (cellBounds (i).contains (e.position)) cell = i;
        if (cell < 0) return;
        shifted = e.mods.isShiftDown(); dragged = false; origin = e.position.y;
        velocity = proc.readChordArp().rhythm[(size_t) cell].velocity;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (cell < 0 || shifted || e.getDistanceFromDragStart() < 3) return;
        if (! dragged)
        {
            dragged = true; set (id (cell, "type"), 0);
            proc.apvts.getParameter (id (cell, "velocity"))->beginChangeGesture();
        }
        auto* p = proc.apvts.getParameter (id (cell, "velocity"));
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.0f, 1.0f, velocity + (origin - e.position.y) / juce::jmax (24.0f, (float) getHeight()))));
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (cell < 0) return;
        if (dragged) proc.apvts.getParameter (id (cell, "velocity"))->endChangeGesture();
        else
        {
            const auto type = proc.readChordArp().rhythm[(size_t) cell].type;
            set (id (cell, "type"), shifted ? (type == 2 ? 0.0f : 2.0f) : (type == 0 ? 1.0f : 0.0f));
        }
        cell = -1;
    }
private:
    int cell = -1; bool shifted = false, dragged = false; float velocity = 1, origin = 0;
    juce::String id (int i, const char* field) const { return "arp_rhythm_" + juce::String (i + 1) + "_" + field; }
    juce::Rectangle<float> cellBounds (int i) const
    {
        auto b = getLocalBounds().toFloat().reduced (12, 4); b.removeFromTop (14);
        const float width = b.getWidth() / 16;
        return { b.getX() + width * i + 1, b.getY(), width - 2, b.getHeight() };
    }
    void set (const juce::String& key, float value)
    {
        auto* p = proc.apvts.getParameter (key);
        p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (value)); p->endChangeGesture(); repaint();
    }
};

// CUSTOM arp pattern: up to 64 steps in four bars of 16. Drag a bar up or down to choose the note number of that step (0 = lowest held note, 1 = the next, past the last
// one it continues an octave up; below 0 it goes down). Right-click: length and reset.
class PatternLane : public Display
{
public:
    PatternLane (RelicProcessor& p) : Display (p)
    {
        setTooltip ("Custom pattern: drag a bar to set the note number of that step (0 = lowest held note). Click BAR 1-4 to change page. Right-click: length and reset.");
    }
    void paint (juce::Graphics& g) override
    {
        paintFrame (g); const auto s = proc.readChordArp(); const int len = juce::jlimit (1, 64, s.patLength);
        const int hot = proc.arpStepShown.load() - 1;
        g.setColour (lime.withAlpha (0.85f)); drawText (g, "CUSTOM PATTERN   " + juce::String (len) + " STEPS", juce::Rectangle<float> (18.0f, 8.0f, 300.0f, 14.0f), 11, juce::Justification::left);
        for (int b = 0; b < 4; ++b)
        {
            auto t = tabBounds (b); g.setColour (b == page ? gold : goldDim); g.drawRoundedRectangle (t, 3, 1.2f);
            g.setColour (parchment.withAlpha (b * 16 < len ? 0.9f : 0.3f)); drawText (g, "BAR " + juce::String (b + 1), t, 10, juce::Justification::centred);
        }
        const auto area = laneArea(); g.setColour (lime.withAlpha (0.25f)); g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * 0.5f), area.getX(), area.getRight());
        for (int i = 0; i < 16; ++i)
        {
            const int idx = page * 16 + i; auto c = cellBounds (i); const bool on = idx < len;
            g.setColour (lime.withAlpha (on ? 0.12f : 0.03f)); g.fillRoundedRectangle (c, 3);
            if (! on) continue;
            const int d = s.pattern[(size_t) idx]; const float mid = c.getCentreY(), y = c.getY() + c.getHeight() * (12 - d) / 24.0f;
            g.setColour (lime.withAlpha (s.arpOn && s.mode == 8 ? 0.8f : 0.4f)); g.fillRect (juce::Rectangle<float> (c.getX() + 4, juce::jmin (mid, y), c.getWidth() - 8, juce::jmax (2.0f, std::abs (y - mid))));
            if (s.arpOn && s.mode == 8 && hot >= 0 && hot % len == idx) { g.setColour (gold); g.drawRoundedRectangle (c, 3, 1.5f); }
            g.setColour (parchment.withAlpha (0.85f)); drawText (g, juce::String (d), c.withHeight (12.0f).withY (c.getY() + 1), 9, juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu; const auto s = proc.readChordArp();
            for (int n : { 4, 8, 12, 16, 24, 32, 48, 64 }) menu.addItem (n, juce::String (n) + " steps", true, n == s.patLength);
            menu.addSeparator(); menu.addItem (100, "Reset: 0 1 2 3 4 5 6 7 ..."); menu.addItem (101, "Clear: every step on the lowest note");
            auto safe = juce::Component::SafePointer<PatternLane> (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
            {
                if (safe == nullptr) return;
                if (r >= 1 && r <= 64) safe->set ("arp_pat_length", (float) r);
                if (r == 100) for (int i = 0; i < 64; ++i) safe->set ("arp_pat_" + juce::String (i + 1), (float) (i % 8));
                if (r == 101) for (int i = 0; i < 64; ++i) safe->set ("arp_pat_" + juce::String (i + 1), 0.0f);
            });
            return;
        }
        for (int b = 0; b < 4; ++b) if (tabBounds (b).contains (e.position)) { page = b; repaint(); return; }
        grab (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { grab (e); }
    void mouseUp (const juce::MouseEvent&) override { if (gesture != nullptr) { gesture->endChangeGesture(); gesture = nullptr; } }
private:
    int page = 0; juce::RangedAudioParameter* gesture = nullptr;
    juce::Rectangle<float> laneArea() const { auto b = getLocalBounds().toFloat().reduced (12, 4); b.removeFromTop (22); return b; }
    juce::Rectangle<float> tabBounds (int b) const { return { (float) getWidth() - 12.0f - 4 * 52.0f + (float) b * 52.0f, 6.0f, 48.0f, 16.0f }; }
    juce::Rectangle<float> cellBounds (int i) const { const auto a = laneArea(); const float w = a.getWidth() / 16; return { a.getX() + w * i + 1, a.getY(), w - 2, a.getHeight() }; }
    void grab (const juce::MouseEvent& e)
    {
        const int len = juce::jlimit (1, 64, proc.readChordArp().patLength);
        for (int i = 0; i < 16; ++i)
        {
            const int idx = page * 16 + i; auto c = cellBounds (i);
            if (idx >= len || e.position.x < c.getX() || e.position.x >= c.getRight() + 1) continue;
            auto* p = proc.apvts.getParameter ("arp_pat_" + juce::String (idx + 1));
            if (gesture != p) { if (gesture != nullptr) gesture->endChangeGesture(); gesture = p; p->beginChangeGesture(); }
            const int d = juce::jlimit (-12, 12, (int) std::lround (12.0f - 24.0f * (e.position.y - c.getY()) / c.getHeight()));
            p->setValueNotifyingHost (p->convertTo0to1 ((float) d)); repaint(); return;
        }
    }
    void set (const juce::String& key, float value) { auto* p = proc.apvts.getParameter (key); p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (value)); p->endChangeGesture(); repaint(); }
};


// The arp's EVENTS pattern: up to 64 steps in four bars of 16. Each column is a step; the rows are held-note numbers 0 (bottom) to 11, so a column with several cells is a chord.
// Click a cell to switch that note on or off, drag a lit cell sideways to make it last longer, drag in the strip at the bottom to set the step's velocity, right-click for the rest.
class EventLane : public Display
{
public:
    EventLane (RelicProcessor& p) : Display (p)
    {
        setTooltip ("Events pattern: click a cell to add or remove a note (row = held-note number, 0 at the bottom); drag a lit cell sideways to lengthen it; drag in the bottom strip to set velocity. Click BAR 1-4 to change page. Right-click: step length, pattern length, reset.");
    }
    void paint (juce::Graphics& g) override
    {
        paintFrame (g); const auto s = proc.readChordArp(); const int len = juce::jlimit (1, 64, s.evLength), hot = proc.arpStepShown.load() - 1; const bool live = s.arpOn && s.mode == 9;
        g.setColour (lime.withAlpha (0.85f)); drawText (g, "EVENTS   " + juce::String (len) + " STEPS", juce::Rectangle<float> (18.0f, 8.0f, 300.0f, 14.0f), 11, juce::Justification::left);
        for (int b = 0; b < 4; ++b)
        {
            auto t = tabBounds (b); g.setColour (b == page ? gold : goldDim); g.drawRoundedRectangle (t, 3, 1.2f);
            g.setColour (parchment.withAlpha (b * 16 < len ? 0.9f : 0.3f)); drawText (g, "BAR " + juce::String (b + 1), t, 10, juce::Justification::centred);
        }
        for (int i = 0; i < 16; ++i)
        {
            const int idx = page * 16 + i; const bool on = idx < len; auto col = colBounds (i); auto grid = gridArea (); auto strip = stripArea ();
            g.setColour (lime.withAlpha (on ? (i % 4 == 0 ? 0.14f : 0.08f) : 0.02f)); g.fillRoundedRectangle (juce::Rectangle<float> (col.getX(), grid.getY(), col.getWidth(), grid.getHeight()), 2);
            if (! on) continue;
            const int mask = s.evMask[(size_t) idx], l = s.evLen[(size_t) idx];
            for (int r = 0; r < 12; ++r) if (mask & (1 << r))
            {
                float w = col.getWidth(); if (l > 1) w = std::min ((float) l, (float) (16 - i)) * (col.getWidth() + 2.0f) - 2.0f;
                g.setColour (lime.withAlpha (live ? 0.85f : 0.5f)); g.fillRoundedRectangle (col.getX(), rowY (r) + 1, w, rowH() - 2, 2);
            }
            const float v = (float) s.evVel[(size_t) idx] / 127.0f; g.setColour (gold.withAlpha (mask ? 0.85f : 0.3f)); g.fillRect (col.getX(), strip.getBottom() - strip.getHeight() * v, col.getWidth(), strip.getHeight() * v);
            if (live && hot >= 0 && hot % len == idx) { g.setColour (gold); g.drawRoundedRectangle (juce::Rectangle<float> (col.getX() - 1, grid.getY(), col.getWidth() + 2, strip.getBottom() - grid.getY()), 3, 1.5f); }
        }
        g.setColour (parchment.withAlpha (0.5f)); drawText (g, "VEL", juce::Rectangle<float> (0.0f, stripArea().getY(), 14.0f, stripArea().getHeight()), 8, juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { menu (e); return; }
        for (int b = 0; b < 4; ++b) if (tabBounds (b).contains (e.position)) { page = b; repaint(); return; }
        const int len = juce::jlimit (1, 64, proc.readChordArp().evLength); const int i = colAt (e.position.x); if (i < 0 || page * 16 + i >= len) return;
        const int idx = page * 16 + i; drag = Drag::None; dragStep = idx;
        if (stripArea().expanded (0, 4).contains (e.position)) { drag = Drag::Vel; velAt (e); return; }
        if (! gridArea().contains (e.position)) return;
        dragRow = rowAt (e.position.y); const int mask = (int) proc.readChordArp().evMask[(size_t) idx];
        if (mask & (1 << dragRow)) { drag = Drag::Resize; moved = false; } else { put (maskKey (idx), (float) (mask | (1 << dragRow))); drag = Drag::None; }
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag == Drag::Vel) { velAt (e); return; }
        if (drag != Drag::Resize) return;
        const int i = colAt (e.position.x); if (i < 0) return; const int steps = juce::jlimit (1, 16, page * 16 + i - dragStep + 1); if (page * 16 + i == dragStep && ! moved) return;
        moved = true; put ("arp_ev_" + juce::String (dragStep + 1) + "_len", steps == 1 ? 0.0f : (float) steps); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag == Drag::Resize && ! moved) { const int mask = (int) proc.readChordArp().evMask[(size_t) dragStep]; put (maskKey (dragStep), (float) (mask & ~(1 << dragRow))); }       // a plain click on a lit cell clears it
        drag = Drag::None; repaint();
    }
private:
    enum class Drag { None, Resize, Vel };
    int page = 0, dragStep = 0, dragRow = 0; bool moved = false; Drag drag = Drag::None;
    juce::Rectangle<float> body() const { auto b = getLocalBounds().toFloat().reduced (12, 4); b.removeFromTop (22); b.removeFromLeft (14); return b; }
    juce::Rectangle<float> gridArea() const { auto b = body(); return b.withTrimmedBottom (34); }
    juce::Rectangle<float> stripArea() const { auto b = body(); return b.removeFromBottom (30); }
    juce::Rectangle<float> tabBounds (int b) const { return { (float) getWidth() - 12.0f - 4 * 52.0f + (float) b * 52.0f, 6.0f, 48.0f, 16.0f }; }
    juce::Rectangle<float> colBounds (int i) const { const auto a = gridArea(); const float w = a.getWidth() / 16; return { a.getX() + w * i + 1, a.getY(), w - 2, a.getHeight() }; }
    float rowH() const { return gridArea().getHeight() / 12.0f; }
    float rowY (int r) const { return gridArea().getBottom() - rowH() * (float) (r + 1); }
    int colAt (float x) const { const auto a = gridArea(); const int i = (int) std::floor ((x - a.getX()) / (a.getWidth() / 16)); return i < 0 || i > 15 ? -1 : i; }
    int rowAt (float y) const { return juce::jlimit (0, 11, (int) std::floor ((gridArea().getBottom() - y) / rowH())); }
    static juce::String maskKey (int idx) { return "arp_ev_" + juce::String (idx + 1) + "_mask"; }
    void put (const juce::String& key, float value) { auto* p = proc.apvts.getParameter (key); p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (value)); p->endChangeGesture(); }
    void velAt (const juce::MouseEvent& e)
    {
        const int i = colAt (e.position.x); const int len = juce::jlimit (1, 64, proc.readChordArp().evLength); if (i < 0 || page * 16 + i >= len) return; const auto a = stripArea();
        put ("arp_ev_" + juce::String (page * 16 + i + 1) + "_vel", (float) juce::jlimit (1, 127, (int) std::lround (127.0f * (a.getBottom() - e.position.y) / a.getHeight()))); repaint();
    }
    void menu (const juce::MouseEvent& e)
    {
        const auto st = proc.readChordArp(); const int i = colAt (e.position.x), idx = i < 0 ? -1 : page * 16 + i; juce::PopupMenu m, lens;
        if (idx >= 0 && idx < st.evLength)
        {
            lens.addItem (200, "Normal gate", true, st.evLen[(size_t) idx] == 0);
            for (int n : { 1, 2, 3, 4, 6, 8, 12, 16 }) lens.addItem (200 + n, juce::String (n) + (n == 1 ? " step" : " steps"), true, st.evLen[(size_t) idx] == n);
            m.addSubMenu ("Step " + juce::String (idx + 1) + " length", lens); m.addItem (300, "Clear step " + juce::String (idx + 1)); m.addSeparator();
        }
        juce::PopupMenu pl; for (int n : { 4, 8, 12, 16, 24, 32, 48, 64 }) pl.addItem (n, juce::String (n) + " steps", true, n == st.evLength);
        m.addSubMenu ("Pattern length", pl); m.addItem (400, "Reset: one rising note per step"); m.addItem (401, "Clear: every step a rest"); m.addItem (402, "All velocities 100");
        auto safe = juce::Component::SafePointer<EventLane> (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, idx] (int r)
        {
            if (safe == nullptr || r < 1) return;
            if (r <= 64) safe->put ("arp_ev_length", (float) r);
            else if (r >= 200 && r <= 216) safe->put ("arp_ev_" + juce::String (idx + 1) + "_len", (float) (r - 200));
            else if (r == 300) safe->put (maskKey (idx), 0.0f);
            else if (r == 400) for (int k = 0; k < 64; ++k) { safe->put (maskKey (k), (float) (1 << (k % 8))); safe->put ("arp_ev_" + juce::String (k + 1) + "_len", 0.0f); }
            else if (r == 401) for (int k = 0; k < 64; ++k) safe->put (maskKey (k), 0.0f);
            else if (r == 402) for (int k = 0; k < 64; ++k) safe->put ("arp_ev_" + juce::String (k + 1) + "_vel", 100.0f);
            safe->repaint();
        });
    }
};

// Horizontal slider for a signed amount: name on the left, value on the right.
class AmountSlider : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setTooltip (const juce::String& s) override { juce::SettableTooltipClient::setTooltip (s); slider.setTooltip (s); }
    AmountSlider (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name) : label (name)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal); slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (slider);
        att = std::make_unique<juce::SliderParameterAttachment> (*s.getParameter (id), slider, nullptr);
        slider.onValueChange = [this] { repaint(); };
    }
    std::function<void()> onLabelClick;          // clicking the label strip (used by CURVE to open the curve editor)
    std::function<bool()> showAlt;               // when it returns true the value reads "DRAWN"
    void resized() override { slider.setBounds (getLocalBounds().withTrimmedTop (getHeight() * 40 / 100)); }
    void mouseUp (const juce::MouseEvent& e) override { if (onLabelClick && e.position.y < (float) getHeight() * 0.4f && contains (e.getPosition())) onLabelClick(); }
    void paint (juce::Graphics& g) override
    {
        const float lh = (float) getHeight() * 0.34f; const double v = slider.getValue();
        g.setColour (parchment); drawText (g, label, juce::Rectangle<int> (6, 0, getWidth() / 2, (int) lh), lh * 0.95f, juce::Justification::left);
        g.setColour (lime); drawText (g, showAlt && showAlt() ? juce::String ("DRAWN") : juce::String (v > 0.0005 ? "+" : "") + juce::String ((int) std::lround (v * 100.0)) + "%", juce::Rectangle<int> (getWidth() / 2 - 6, 0, getWidth() / 2, (int) lh), lh * 0.95f, juce::Justification::right);
    }
private:
    juce::Slider slider; juce::String label; std::unique_ptr<juce::SliderParameterAttachment> att;
};

// An invisible button-like area.
struct ClickArea : juce::Component, juce::SettableTooltipClient
{
    std::function<void()> onClick;
    ClickArea() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void mouseUp (const juce::MouseEvent& e) override { if (onClick && contains (e.getPosition())) onClick(); }
};

} // namespace ui

class RelicEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit RelicEditor (RelicProcessor&);
    ~RelicEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void setChoice (const char* id, int index);
    RelicProcessor& proc;
    ui::LnF lnf;
    static constexpr int kTooltipDelayMs = 1500;                       // how long the mouse rests on a control before its help appears
    juce::TooltipWindow tooltipWindow { this, kTooltipDelayMs };
    void applyHelp();                                                    // hover help for the controls that are not created through the add... helpers
    static constexpr float DW = 1484.0f, DH = 1060.0f;     // design size (mockup)

    // tabs
    std::unique_ptr<ui::StoneButton> tabs[11]; std::unique_ptr<ui::StoneButton> delayMode;
    int currentTab = 1;
    void setTab (int t);
    std::vector<juce::Component*> oscGroup, fxGroup, fx2Group, fx3Group, arpGroup, chordGroup, modGroup, osc23Group, envGroup, fx4Group, texGroup;

    // presets
    std::unique_ptr<ui::PresetBrowser> browser; std::unique_ptr<ui::StoneButton> saveBtn, prevPreset, nextPreset; std::unique_ptr<ui::ClickArea> barClick;
    juce::String lastPresetName, lastWtName;
    void doSave(); void stepPreset (int delta); void showBrowser (bool show);

    // SAMPLER tab
    std::unique_ptr<ui::SampleDisplay> sampDisp; std::unique_ptr<ui::MixSlider> mixSlider, abSlider; std::unique_ptr<ui::StoneButton> loadB; bool expanded = false;
    std::vector<juce::Component*> sampGroup;
    std::vector<std::unique_ptr<ui::StoneButton>> sampButtons; std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> sampAtts;
    std::unique_ptr<juce::FileChooser> chooser;
    ui::Knob* addKnobCopy (const char* id, const char* name);
    std::vector<ui::Knob*> sampKnobs;
    // SAMPLER tab: which sample slot is being edited and what is shown for it
    int sampSlot = 0;                                                    // 0 = slot A, 1 = slot B
    std::unique_ptr<ui::StoneButton> slotBtn[2];
    std::vector<juce::Component*> sampSetA, sampSetB, sampEnvVoice, sampEnvA, sampEnvB;
    std::vector<ui::Knob*> sampKnobsA, sampKnobsB, envKnobsVoice, envKnobsA, envKnobsB;
    void updateSamplerView();
    // TEXTURE tab: grains page or own-file page
    int texView = 0; std::unique_ptr<ui::StoneButton> texViewBtn[2], loadTex; std::vector<juce::Component*> texGrains, texShape, texOwnOnly;
    void updateTextureView();
    void updateArpView(); void updateChordView();
    // MOD tab: slots 1-4 or 5-8
    int modPage = 0; std::unique_ptr<ui::StoneButton> modPageBtn[2]; void updateModView();

    // OSC tab
    std::unique_ptr<ui::WaveDisplay> wave; std::unique_ptr<ui::EnvDisplay> env; std::unique_ptr<ui::Meter> meter;
    std::vector<std::unique_ptr<ui::Knob>> knobs;     // all knobs, named lookup by index below
    ui::Knob* K (const char* id) { return knobById[id]; }
    std::map<juce::String, ui::Knob*> knobById;
    ui::Knob* addKnob (const char* id, const char* name);

    // filter
    std::unique_ptr<ui::FilterDisplay> filterDisp;
    std::unique_ptr<ui::StoneButton> ftype[4], filterOn;
    std::unique_ptr<juce::ParameterAttachment> ftypeAtt;
    std::unique_ptr<juce::ButtonParameterAttachment> filterOnAtt;

    // effect pads and bottom row
    struct Pad { std::unique_ptr<ui::StoneButton> btn; std::unique_ptr<juce::ButtonParameterAttachment> att; const char* id; };
    std::vector<Pad> pads;
    std::unique_ptr<ui::StoneButton> waveLeft, waveRight, waveName, polyBtn, allOn, bypassFx;
    std::unique_ptr<ui::StoneButton> wtPrev, wtNext, wtName, wtLoad, kitBtn, vibEnvBtn, wtOscBtn, tapeShapeBtn, autoBtn; std::array<std::unique_ptr<ui::StoneButton>, 2> lfoDrawBtn; int wtOsc = 0;      // wavetable file strip on the OSC tab
    struct SyncChip { std::unique_ptr<ui::StoneButton> btn; juce::String knobId, syncId; };
    std::vector<SyncChip> syncChips; void updateSyncChips();        // a small note-value button on each rate / time knob that can follow the host tempo
    void updateWtStrip(); void showWavetableMenu(); void loadWavetableDialog(); void afterWavetableLoaded();
    juce::Label waveLabel;

    // FX tab controls
    std::vector<std::unique_ptr<ui::StoneButton>> fxToggles; std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> fxAtts;
    std::unique_ptr<ui::StoneButton> panShape;

    // CHORDS and ARP tabs
    std::unique_ptr<ui::ChordDisplay> chordDisp; std::unique_ptr<ui::ArpDisplay> arpDisp; std::unique_ptr<ui::RhythmLane> rhythmLane; std::unique_ptr<ui::PatternLane> patternLane; std::unique_ptr<ui::EventLane> eventLane; std::unique_ptr<ui::ShapeEditor> shapeEditor;
    std::map<juce::String, std::unique_ptr<ui::Stepper>> steppers;
    std::map<juce::String, std::unique_ptr<ui::StoneButton>> noteToggles; std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> noteAtts;
    ui::Stepper* addStepper (const char* id, const char* name, std::vector<juce::Component*>& group);
    ui::StoneButton* addNoteToggle (const char* id, const char* text, float textScale, const char* key = nullptr);
    std::unique_ptr<ui::StoneButton> wahMode;

    std::vector<std::unique_ptr<ui::AmountSlider>> amountSliders;

    // placeholders (features that do not exist yet)
    struct Ph { std::unique_ptr<ui::StoneButton> b; juce::Rectangle<float> r; };
    std::vector<Ph> phs;
    std::vector<std::pair<std::unique_ptr<ui::IconButton>, juce::Rectangle<float>>> sideBtns;
    ui::StoneButton* placeholder (const juce::String& text, float x, float y, float w, float h);
    ui::StoneButton *craftBtn = nullptr, *undoBtn = nullptr, *redoBtn = nullptr, *chaosBtn = nullptr, *lockFxBtn = nullptr;

    std::unique_ptr<ui::XyPad> xyPad; std::vector<juce::Component*> xyGroup; bool xyShown = false;
    void showXy (bool show);
    void mouseDoubleClick (const juce::MouseEvent&) override;
    ui::StoneButton *fxAB = nullptr, *splitBtn = nullptr;
    struct RightClick : juce::MouseListener { std::function<void()> f; void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && f) f(); } };
    std::unique_ptr<RightClick> splitRc;
    juce::Image bgImg, logoImg, sceneImg, frameImg;   // skin art
    int sceneIdx = 0; std::unique_ptr<ui::StoneButton> scenePrev, sceneNext;   // the switchable picture on the left
    void setScene (int idx);
    ui::SkinnedKeyboard keyboard; int octShift = 0;
    juce::Rectangle<int> R (float x, float y, float w, float h) const;
    void cycleChoice (const char* id, int delta);
    void setAllEffects (bool on);
    float scale() const { return getWidth() / DW; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RelicEditor)
};
