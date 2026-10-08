#include "PluginEditor.h"

// Drawing is a port of ableton/whatkey-ui.js so both versions look the same.
namespace
{
const juce::StringArray NOTES { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const juce::StringArray AUDIO_EXTS { ".wav", ".mp3", ".flac", ".aiff", ".aif", ".m4a", ".ogg", ".opus", ".aac" };

juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

juce::Colour rgba (float r, float g, float b, float a = 1.0f) { return juce::Colour::fromFloatRGBA (r, g, b, a); }
const auto DIM = rgba (1, 1, 1, 0.45f), FG = rgba (1, 1, 1, 0.92f), DARK = rgba (0.08f, 0.08f, 0.08f);

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions ("Helvetica Neue", bold ? "Bold" : "Medium", size).withPointHeight (size));
}

float textWidth (const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth (f, s); }

// Draws text with its baseline at y (like mgraphics.move_to + show_text); returns its width.
float text (juce::Graphics& g, const juce::String& s, float x, float y, float size, juce::Colour c, bool bold = false)
{
    const auto f = font (size, bold);
    g.setFont (f);
    g.setColour (c);
    g.drawSingleLineText (s, juce::roundToInt (x), juce::roundToInt (y));
    return textWidth (f, s);
}

void rect (juce::Graphics& g, float x, float y, float w, float h, float r, juce::Colour c)
{
    g.setColour (c);
    g.fillRoundedRectangle (x, y, w, h, r);
}

// Colour each Camelot number by its position on the wheel.
juce::Colour camelotColor (const juce::String& cam, float alpha = 1.0f)
{
    const float h = (float) (cam.getIntValue() - 1) / 12.0f * 6.0f;
    const float s = 0.55f, l = cam.endsWith ("A") ? 0.5f : 0.6f;
    const float c = (1 - std::abs (2 * l - 1)) * s, x = c * (1 - std::abs (std::fmod (h, 2.0f) - 1)), m = l - c / 2;
    const float rgb[6][3] = { { c, x, 0 }, { x, c, 0 }, { 0, c, x }, { 0, x, c }, { x, 0, c }, { c, 0, x } };
    const auto& v = rgb[(int) std::floor (h) % 6];
    return rgba (v[0] + m, v[1] + m, v[2] + m, alpha);
}

struct Key { juce::String key, camelot; };

// Relative major/minor: same Camelot number, other letter.
Key relative (const Key& k)
{
    const bool minor = k.key.endsWith ("minor");
    const int tonic = NOTES.indexOf (k.key.upToFirstOccurrenceOf (" ", false, false));
    return { NOTES[(tonic + (minor ? 3 : 9)) % 12] + (minor ? " major" : " minor"),
             k.camelot.dropLastCharacters (1) + (minor ? "B" : "A") };
}

juce::String shortKey (const juce::String& k) { return k.replace (" major", "").replace (" minor", "m"); }

// Faint backing text, sized to ~85% of the panel width and centred in it.
void watermark (juce::Graphics& g, const juce::String& s, float W, float H)
{
    const float size = juce::jmin (100.0f * (W * 0.85f) / textWidth (font (100, true), s), H * 0.8f);
    const auto f = font (size, true);
    g.setFont (f);
    g.setColour (rgba (1, 1, 1, 0.035f));
    g.drawSingleLineText (s, juce::roundToInt ((W - textWidth (f, s)) / 2), juce::roundToInt (H / 2 + size * 0.36f));
}
} // namespace

WhatKeyEditor::WhatKeyEditor (WhatKeyProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (475, 169);
    proc.addChangeListener (this);
}

WhatKeyEditor::~WhatKeyEditor() { proc.removeChangeListener (this); }

void WhatKeyEditor::paint (juce::Graphics& g)
{
    const auto snap = proc.getSnapshot();
    g.fillAll (juce::Colour (0xff4f4f4f));
    paintDropZone (g, snap);

    g.setColour (juce::Colours::black);
    g.fillRect (panel);
    const auto inner = panel.reduced (1).toFloat();
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (inner.toNearestInt());
    g.setOrigin (inner.getPosition().toInt());
    paintPanel (g, snap, inner.getWidth(), inner.getHeight());
}

void WhatKeyEditor::paintDropZone (juce::Graphics& g, const WhatKeyProcessor::Snapshot& snap)
{
    const auto r = dropZone.toFloat();
    g.setColour (dragHover ? juce::Colour (0xff5d5d5d) : juce::Colour (0xff4f4f4f));
    g.fillRect (r);
    g.setColour (dragHover ? rgba (1, 1, 1, 0.6f) : juce::Colour (0xff151515));
    g.drawRect (r, 1.0f);

    const auto cy = r.getCentreY();
    g.setColour (rgba (1, 1, 1, 0.95f));
    const juce::String label ("Drop Something Here!");
    const float size = juce::jmin (11.0f, 11.0f * (r.getWidth() - 6) / textWidth (font (11, true), label)); // shrink to fit
    g.setFont (font (size, true));
    g.drawText (label, r.withY (cy - 10).withHeight (14), juce::Justification::centred, false);

    if (snap.fileName.isNotEmpty())
    {
        g.setColour (rgba (1, 1, 1, 0.75f));
        g.setFont (font (10).italicised());
        g.drawText (snap.fileName, r.withY (cy + 3).withHeight (13).reduced (2, 0), juce::Justification::centred, true);
    }
}

void WhatKeyEditor::paintPanel (juce::Graphics& g, const WhatKeyProcessor::Snapshot& snap, float W, float H)
{
    rect (g, 0, 0, W, H, 6, rgba (0.11f, 0.11f, 0.12f));
    watermark (g, "ALL OUT", W, H);
    button = {};

    if (snap.mode != "result")
    {
        const auto col = snap.mode == "error" ? rgba (1, 0.45f, 0.4f) : snap.mode == "busy" ? FG : DIM;
        text (g, "WhatKey", 12, 22, 12, DIM, true);
        g.setFont (font (12));
        g.setColour (col);
        g.drawText (snap.text, juce::Rectangle<float> (12, H / 2 - 8, W - 24, 18), juce::Justification::centredLeft, true);
        return;
    }

    const auto& d = snap.data;
    auto keyAt = [] (const juce::var& v) { return Key { v["key"].toString(), v["camelot"].toString() }; };
    const auto* keys = d["keys"].getArray();
    const auto* compatible = d["compatible"].getArray();
    if (keys == nullptr || keys->isEmpty()) return;

    const auto& topVar = keys->getReference (0);
    const auto top = keyAt (topVar);
    const auto agreeVar = d["segment_agreement"];
    const auto agree = agreeVar.isVoid() || agreeVar.isUndefined() ? juce::String ("n/a")
                                                                   : juce::String (juce::roundToInt ((double) agreeVar * 100)) + "%";
    const double tuning = d["tuning_cents"];
    const auto cents = (tuning >= 0 ? "+" : "") + juce::String (juce::roundToInt (tuning));

    // Header: file name + stats
    auto name = d["file"].toString();
    if (name.length() > 34) name = name.substring (0, 33) + U ("…");
    text (g, name, 12, 17, 10, DIM);
    const auto stats = juce::String (juce::roundToInt ((double) d["duration"])) + U ("s · ") + cents + U ("¢ · ") + agree + " stable";
    text (g, stats, W - 12 - textWidth (font (10), stats), 17, 10, DIM);

    // Headline key + Camelot badge + confidence
    const float kw = text (g, top.key, 12, 48, 24, FG, true);
    rect (g, 20 + kw, 30, 34, 20, 4, camelotColor (top.camelot));
    text (g, top.camelot, 27 + kw, 45, 12, DARK, true);
    const float pw = text (g, juce::String (juce::roundToInt ((double) topVar["prob"] * 100)) + "%", 62 + kw, 46, 13, DIM);
    if (proc.showRelatives)
    {
        const auto r = relative (top);
        const float rw = text (g, "rel. " + shortKey (r.key), 74 + kw + pw, 46, 13, DIM);
        text (g, r.camelot, 80 + kw + pw + rw, 46, 12, camelotColor (r.camelot));
    }

    // Runner-up bars (shifted right to make room for relatives when shown)
    const float barX = proc.showRelatives ? 186.0f : 108.0f, barW = W - barX - 48;
    for (int i = 1; i < keys->size(); ++i)
    {
        const auto& kv = keys->getReference (i);
        const auto k = keyAt (kv);
        const float prob = (float) (double) kv["prob"];
        const float y = 62.0f + (float) (i - 1) * 17.0f;
        text (g, k.key, 12, y + 11, 11, FG);
        text (g, k.camelot, 78, y + 11, 10, camelotColor (k.camelot));
        if (proc.showRelatives)
        {
            const auto r = relative (k);
            text (g, r.key, 108, y + 11, 10, DIM);
            text (g, r.camelot, 158, y + 11, 10, camelotColor (r.camelot, 0.7f));
        }
        rect (g, barX, y + 2, barW, 10, 3, rgba (1, 1, 1, 0.06f));
        rect (g, barX, y + 2, juce::jmax (3.0f, barW * prob), 10, 3, camelotColor (k.camelot, 0.85f));
        text (g, juce::String (prob * 100, 1) + "%", barX + barW + 6, y + 11, 10, DIM);
    }

    // Compatible keys
    float x = 12;
    const float y = H - 10;
    x += text (g, "Mixes with", x, y, 10, DIM) + 8;
    if (compatible != nullptr)
    {
        for (const auto& kv : *compatible)
        {
            const auto k = keyAt (kv);
            const auto label = shortKey (k.key) + " " + k.camelot;
            const float w = textWidth (font (10), label) + 10;
            rect (g, x, y - 11, w, 15, 4, camelotColor (k.camelot, 0.25f));
            text (g, label, x + 5, y, 10, FG);
            x += w + 5;
        }
    }

    // Relatives toggle, styled like the chips
    const bool on = proc.showRelatives;
    const float bw = textWidth (font (10), "Relatives") + 12;
    const juce::Rectangle<float> b (W - 12 - bw, y - 11, bw, 15);
    rect (g, b.getX(), b.getY(), bw, 15, 4, on ? rgba (1, 1, 1, 0.85f) : rgba (1, 1, 1, 0.08f));
    text (g, "Relatives", b.getX() + 6, y, 10, on ? DARK : DIM, on);
    button = b.translated ((float) panel.getX() + 1, (float) panel.getY() + 1);
}

void WhatKeyEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    if (button.contains (pos))
    {
        proc.showRelatives = ! proc.showRelatives;
        repaint();
    }
    else if (dropZone.toFloat().contains (pos))
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose an audio file", juce::File(), "*.wav;*.mp3;*.flac;*.aiff;*.aif;*.m4a;*.ogg;*.opus;*.aac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      proc.analyze (fc.getResult());
                              });
    }
}

void WhatKeyEditor::mouseMove (const juce::MouseEvent& e)
{
    const bool clickable = button.contains (e.position) || dropZone.toFloat().contains (e.position);
    setMouseCursor (clickable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

bool WhatKeyEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (AUDIO_EXTS.contains (juce::File (f).getFileExtension(), true))
            return true;
    return false;
}

void WhatKeyEditor::filesDropped (const juce::StringArray& files, int, int)
{
    setDragHover (false);
    for (auto& f : files)
    {
        if (AUDIO_EXTS.contains (juce::File (f).getFileExtension(), true))
        {
            proc.analyze (juce::File (f));
            return;
        }
    }
}
