// Skinnable editor. Layout follows the user's mockup (1484 x 1060 design units).
// Everything is drawn in code for now; image art can replace the paint routines later.
#pragma once
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
        if (pid == id (0)) v = std::min (v, val (id (1)) - 0.01f);
        if (pid == id (1)) v = std::max (v, val (id (0)) + 0.01f);
        if (pid == id (2)) v = std::min (v, val (id (3)) - 0.01f);
        if (pid == id (3)) v = std::max (v, val (id (2)) + 0.01f);
        drag->setValueNotifyingHost (drag->convertTo0to1 (juce::jlimit (0.0f, 1.0f, v)));
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag) { drag->endChangeGesture(); drag = nullptr; } }
private:
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
        const char* names[4] = { "SAVE AS", "INIT", "RESCAN", "CLOSE" };
        for (int i = 0; i < 4; ++i)
        {
            btn[i] = std::make_unique<StoneButton> (names[i], true, false); btn[i]->textScale = 0.42f; addAndMakeVisible (*btn[i]);
        }
        btn[0]->onClick = [this] { if (onSave) onSave(); };
        btn[1]->onClick = [this] { proc.initPreset(); updateSelection(); };
        btn[2]->onClick = [this] { refresh(); };
        btn[3]->onClick = [this] { if (onClose) onClose(); };
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
        const int bw = bottom.getWidth() / 4;
        for (int i = 0; i < 4; ++i) btn[i]->setBounds (bottom.removeFromLeft (bw).reduced (getWidth() / 90, 0));
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
            if (q.isNotEmpty() && ! (e.name + " " + cat + " " + e.bank).toLowerCase().contains (q)) continue;
            if (e.name == proc.getPresetName() && e.bank == proc.getPresetBank()) sel = (int) shown.size();
            shown.push_back (e); names.add (e.name);
        }
        presets.setItems (names, sel); repaint();
    }
    RelicProcessor& proc; TextList banks, presets; std::unique_ptr<StoneButton> btn[4];
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
    void resized() override { slider.setBounds (getLocalBounds().withTrimmedTop (getHeight() * 40 / 100)); }
    void paint (juce::Graphics& g) override
    {
        const float lh = (float) getHeight() * 0.34f; const double v = slider.getValue();
        g.setColour (parchment); drawText (g, label, juce::Rectangle<int> (6, 0, getWidth() / 2, (int) lh), lh * 0.95f, juce::Justification::left);
        g.setColour (lime); drawText (g, juce::String (v > 0.0005 ? "+" : "") + juce::String ((int) std::lround (v * 100.0)) + "%", juce::Rectangle<int> (getWidth() / 2 - 6, 0, getWidth() / 2, (int) lh), lh * 0.95f, juce::Justification::right);
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
    std::unique_ptr<ui::StoneButton> wtPrev, wtNext, wtName, wtLoad;      // wavetable file strip on the OSC tab
    struct SyncChip { std::unique_ptr<ui::StoneButton> btn; juce::String knobId, syncId; };
    std::vector<SyncChip> syncChips; void updateSyncChips();        // a small note-value button on each rate / time knob that can follow the host tempo
    void showWavetableMenu(); void loadWavetableDialog(); void afterWavetableLoaded();
    juce::Label waveLabel;

    // FX tab controls
    std::vector<std::unique_ptr<ui::StoneButton>> fxToggles; std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> fxAtts;
    std::unique_ptr<ui::StoneButton> panShape;

    // CHORDS and ARP tabs
    std::unique_ptr<ui::ChordDisplay> chordDisp; std::unique_ptr<ui::ArpDisplay> arpDisp; std::unique_ptr<ui::RhythmLane> rhythmLane; std::unique_ptr<ui::PatternLane> patternLane; std::unique_ptr<ui::ShapeEditor> shapeEditor;
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
