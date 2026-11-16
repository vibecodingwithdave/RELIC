#include "PluginEditor.h"
#include "BinaryData.h"
using namespace ui;

namespace
{
    // The picture on the left: one is shown at a time and the arrows over it switch between them. focusX picks which part of a picture
    // survives the crop when it is wider than the frame (0 = left edge, 0.5 = centre, 1 = right edge).
    struct SceneAsset { const char* data; int size; float focusX; };
    const SceneAsset kScenes[] = {
        { BinaryData::scene_cards_jpg,    BinaryData::scene_cards_jpgSize,    0.5f },   // card table
        { BinaryData::scene_treant_jpg,   BinaryData::scene_treant_jpgSize,   0.5f },
        { BinaryData::scene_wanderer_jpg, BinaryData::scene_wanderer_jpgSize, 0.5f },
        { BinaryData::scene_troll_jpg,    BinaryData::scene_troll_jpgSize,    0.5f },
        { BinaryData::scene_orcs_jpg,     BinaryData::scene_orcs_jpgSize,     0.5f },
        { BinaryData::scene_knights_jpg,  BinaryData::scene_knights_jpgSize,  0.5f },
        { BinaryData::scene_dragon_jpg,   BinaryData::scene_dragon_jpgSize,   0.5f } };
    constexpr int kSceneCount = (int) (sizeof (kScenes) / sizeof (kScenes[0]));
}

void RelicEditor::setScene (int idx)
{
    sceneIdx = ((idx % kSceneCount) + kSceneCount) % kSceneCount;
    sceneImg = juce::ImageCache::getFromMemory (kScenes[sceneIdx].data, kScenes[sceneIdx].size);
    ui::clearScaledImageCache();                                     // drops the old picture's scaled copy
    proc.apvts.state.setProperty ("scene", sceneIdx, nullptr);      // saved with the project
    repaint();
}

static juce::AudioParameterChoice* choiceParam (juce::AudioProcessorValueTreeState& s, const char* id)
{ return dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)); }

ui::Knob* RelicEditor::addKnob (const char* id, const char* name)
{
    knobs.push_back (std::make_unique<ui::Knob> (proc.apvts, id, name));
    auto* k = knobs.back().get(); knobById[id] = k; addAndMakeVisible (k); k->setTooltip (help::forId (id)); return k;
}

void RelicEditor::showBrowser (bool show)
{
    if (show) { browser->refresh(); browser->setVisible (true); browser->toFront (false); }
    else browser->setVisible (false);
}

void RelicEditor::stepPreset (int delta)
{
    auto list = proc.scanPresets(); if (list.empty()) return;
    int cur = -1; for (int i = 0; i < (int) list.size(); ++i) if (list[(size_t) i].name == proc.getPresetName() && list[(size_t) i].bank == proc.getPresetBank()) { cur = i; break; }
    int next = cur < 0 ? (delta > 0 ? 0 : (int) list.size() - 1) : (cur + delta + (int) list.size()) % (int) list.size();
    proc.loadPresetFile (list[(size_t) next].file);
    if (browser->isVisible()) browser->refresh();
}

void RelicEditor::doSave()
{
    auto* aw = new juce::AlertWindow ("Save preset", "Name your preset. It is saved in your User bank.", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", proc.getPresetName() == "Init" ? juce::String() : proc.getPresetName());
    {
        juce::StringArray cats (presetCategories()); cats.add ("Other");
        aw->addComboBox ("category", cats, "Category");
        const int cur = presetCategories().indexOf (proc.getPresetCategory());
        aw->getComboBoxComponent ("category")->setSelectedItemIndex (cur >= 0 ? cur : cats.size() - 1, juce::dontSendNotification);
    }
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
    {
        if (r != 1) return;
        const auto name = aw->getTextEditorContents ("name").trim(); if (name.isEmpty()) return;
        auto f = RelicProcessor::presetsRoot().getChildFile ("User").getChildFile (juce::File::createLegalFileName (name) + ".xml");
        const int ci = aw->getComboBoxComponent ("category")->getSelectedItemIndex();
        const juce::String category = ci >= 0 && ci < presetCategories().size() ? presetCategories()[ci] : juce::String();   // "Other" is saved as no category
        auto doIt = [this, name, category] { proc.savePreset (name, category); if (browser->isVisible()) browser->refresh(); };
        if (f.existsAsFile())
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon).withTitle ("Replace preset?")
                .withMessage ("\"" + name + "\" already exists in your User presets.").withButton ("Replace").withButton ("Cancel").withAssociatedComponent (this),
                [doIt] (int res) { if (res == 1) doIt(); });
        else doIt();
    }), true);
}

ui::Stepper* RelicEditor::addStepper (const char* id, const char* name, std::vector<juce::Component*>& group)
{
    auto s = std::make_unique<ui::Stepper> (proc.apvts, id, name); auto* raw = s.get();
    raw->setTooltip (help::forId (id)); addAndMakeVisible (raw); group.push_back (raw); steppers[id] = std::move (s); return raw;
}

ui::StoneButton* RelicEditor::addNoteToggle (const char* id, const char* text, float textScale, const char* key)
{
    auto b = std::make_unique<ui::StoneButton> (text, true, true); b->textScale = textScale; auto* raw = b.get(); raw->setTooltip (help::forId (id));
    noteAtts.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (id), *b, nullptr));
    addAndMakeVisible (raw); noteToggles[key ? key : id] = std::move (b); return raw;
}

ui::Knob* RelicEditor::addKnobCopy (const char* id, const char* name)
{
    knobs.push_back (std::make_unique<ui::Knob> (proc.apvts, id, name));
    auto* k = knobs.back().get(); addAndMakeVisible (k); k->setTooltip (help::forId (id)); return k;
}

bool RelicEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a")) return true;
    return false;
}
void RelicEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a")) { proc.loadSampleFile (juce::File (f)); setTab (0); repaint(); return; }
}

ui::StoneButton* RelicEditor::placeholder (const juce::String& text, float x, float y, float w, float h)
{
    Ph ph; ph.b = std::make_unique<ui::StoneButton> (text, false, false); ph.r = { x, y, w, h };
    addAndMakeVisible (ph.b.get()); auto* raw = ph.b.get(); phs.push_back (std::move (ph)); return raw;
}

void RelicEditor::setChoice (const char* id, int index)
{
    if (auto* c = choiceParam (proc.apvts, id)) { c->beginChangeGesture(); *c = index; c->endChangeGesture(); }
}
void RelicEditor::cycleChoice (const char* id, int delta)
{
    if (auto* c = choiceParam (proc.apvts, id)) setChoice (id, (c->getIndex() + delta + c->choices.size()) % c->choices.size());
}
void RelicEditor::setAllEffects (bool on)
{
    for (auto id : { "trem_on", "delay_on", "comp_on", "crush_on", "pan_on", "chorus_on", "flanger_on", "reverb_on", "eq_on", "tape_on", "widener_on", "wah_on", "lofi_on", "ott_on" })
        if (auto* p = proc.apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (on ? 1.0f : 0.0f); p->endChangeGesture(); }
    if (! on) for (auto id : { "pdelay_on", "talk_on", "sgate_on" })        // the creative ones are only ever switched off by ALL ON / BYPASS
        if (auto* p = proc.apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (0.0f); p->endChangeGesture(); }
}

RelicEditor::RelicEditor (RelicProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    bgImg    = juce::ImageCache::getFromMemory (BinaryData::bg_plate_png, BinaryData::bg_plate_pngSize);
    logoImg  = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
    sceneIdx = juce::jlimit (0, kSceneCount - 1, (int) proc.apvts.state.getProperty ("scene", 0));
    sceneImg = juce::ImageCache::getFromMemory (kScenes[sceneIdx].data, kScenes[sceneIdx].size);
    frameImg = juce::ImageCache::getFromMemory (BinaryData::panel_frame_png, BinaryData::panel_frame_pngSize);
    {
        auto& sk = ui::skin();
        if (! sk.btn.isValid())
        {
            sk.btn         = juce::ImageCache::getFromMemory (BinaryData::btn_sheet_png, BinaryData::btn_sheet_pngSize);
            sk.pad         = juce::ImageCache::getFromMemory (BinaryData::pad_sheet_png, BinaryData::pad_sheet_pngSize);
            sk.knobBody    = juce::ImageCache::getFromMemory (BinaryData::knob_body_png, BinaryData::knob_body_pngSize);
            sk.knobPointer = juce::ImageCache::getFromMemory (BinaryData::knob_pointer_png, BinaryData::knob_pointer_pngSize);
            sk.screen      = juce::ImageCache::getFromMemory (BinaryData::screen_frame_png, BinaryData::screen_frame_pngSize);
            sk.iconsSidebar = juce::ImageCache::getFromMemory (BinaryData::icons_sidebar_png, BinaryData::icons_sidebar_pngSize);
            sk.iconsAction  = juce::ImageCache::getFromMemory (BinaryData::icons_action_png, BinaryData::icons_action_pngSize);
            sk.iconsEffect  = juce::ImageCache::getFromMemory (BinaryData::icons_effect_png, BinaryData::icons_effect_pngSize);
            sk.keyWhiteUp   = juce::ImageCache::getFromMemory (BinaryData::key_white_up_png, BinaryData::key_white_up_pngSize);
            sk.keyWhiteDown = juce::ImageCache::getFromMemory (BinaryData::key_white_down_png, BinaryData::key_white_down_pngSize);
            sk.keyBlackUp   = juce::ImageCache::getFromMemory (BinaryData::key_black_up_png, BinaryData::key_black_up_pngSize);
            sk.keyBlackDown = juce::ImageCache::getFromMemory (BinaryData::key_black_down_png, BinaryData::key_black_down_pngSize);
            sk.meterSegs    = juce::ImageCache::getFromMemory (BinaryData::meter_segs_png, BinaryData::meter_segs_pngSize);
            sk.fontAtlas    = juce::ImageCache::getFromMemory (BinaryData::font_atlas_png, BinaryData::font_atlas_pngSize);
            ui::buildFontLevels();
        }
    }

    // ---- tabs ----
    const char* tabNames[11] = { "SAMPLE", "OSC", "FX", "FX 2", "FX 3", "ARP", "CHORD", "MOD", "OSC 2", "ENV", "FX 4" };
    for (int i = 0; i < 11; ++i)
    {
        const bool live = true;
        tabs[i] = std::make_unique<ui::StoneButton> (tabNames[i], live, false);
        tabs[i]->textScale = 0.26f; tabs[i]->onClick = [this, i] { setTab (i); };
        addAndMakeVisible (*tabs[i]);
    }

    // ---- OSC tab ----
    wave = std::make_unique<ui::WaveDisplay> (proc); env = std::make_unique<ui::EnvDisplay> (proc); meter = std::make_unique<ui::Meter> (proc);
    addAndMakeVisible (*wave); addAndMakeVisible (*env); addAndMakeVisible (*meter);
    oscGroup = { wave.get(), env.get() };
    const std::pair<const char*, const char*> oscKnobs[] = { { "pulse_width", "Width" }, { "osc_level", "Level" }, { "unison_voices", "Voices" },
        { "unison_detune", "Detune" }, { "unison_spread", "Spread" }, { "voices", "Poly" },
        { "attack_ms", "Attack" }, { "decay_ms", "Decay" }, { "sustain", "Sustain" }, { "release_ms", "Release" } };
    for (auto& k : oscKnobs) oscGroup.push_back (addKnob (k.first, k.second));

    // ---- CHORDS and ARP tabs ----
    chordDisp = std::make_unique<ui::ChordDisplay> (proc); addAndMakeVisible (*chordDisp); chordGroup.push_back (chordDisp.get());
    arpDisp   = std::make_unique<ui::ArpDisplay> (proc);   addAndMakeVisible (*arpDisp);   arpGroup.push_back (arpDisp.get());
    rhythmLane = std::make_unique<ui::RhythmLane> (proc); addAndMakeVisible (*rhythmLane); arpGroup.push_back (rhythmLane.get());
    addStepper ("chord_key", "Key", chordGroup); addStepper ("chord_scale", "Scale", chordGroup); addStepper ("chord_type", "Chord", chordGroup);
    addStepper ("chord_voicing", "Voicing", chordGroup); addStepper ("chord_inversion", "Inversion", chordGroup);
    addStepper ("chord_mode", "Mode", chordGroup); addStepper ("chord_shape", "Shape", chordGroup); chordGroup.push_back (addNoteToggle ("voice_lead", "VOICE LEAD", 0.34f)); chordGroup.push_back (addNoteToggle ("chord_lock", "SCALE LOCK", 0.34f)); chordGroup.push_back (addKnob ("chord_strum_ms", "Strum"));
    addStepper ("arp_div", "Rate", arpGroup); addStepper ("arp_mode", "Pattern", arpGroup); addStepper ("arp_octaves", "Octaves", arpGroup); addStepper ("arp_order", "Order", arpGroup); addStepper ("arp_grouping", "Grouping", arpGroup); addStepper ("arp_wshape", "Wave shape", arpGroup);
    arpGroup.push_back (addNoteToggle ("arp_hold", "HOLD", 0.42f)); arpGroup.push_back (addKnob ("arp_gate", "Gate")); arpGroup.push_back (addKnob ("arp_swing", "Swing")); arpGroup.push_back (addKnob ("arp_human", "Human"));
    arpGroup.push_back (addKnob ("arp_chance", "Chance")); arpGroup.push_back (addKnob ("arp_spray", "Spray"));
    arpGroup.push_back (addNoteToggle ("arp_run", "GO", 0.42f));
    arpGroup.push_back (addNoteToggle ("arp_two_hand", "2-HAND", 0.36f));
    arpGroup.push_back (addNoteToggle ("arp_rhythm_on", "RHYTHM", 0.36f));
    addStepper ("arp_rhythm_length", "Lane length", arpGroup);
    addNoteToggle ("chord_on", "CHORD", 0.4f); addNoteToggle ("arp_on", "ARP", 0.4f);

    // ---- SAMPLER tab ----
    // SLOT A / SLOT B choose which sample layer the tab edits. Each layer has its own controls and, when switched on, its own envelope.
    sampDisp = std::make_unique<ui::SampleDisplay> (proc); addAndMakeVisible (*sampDisp); sampGroup.push_back (sampDisp.get());
    {
        auto mk = [this] (const char* text, const char* paramId, std::vector<juce::Component*>* set)
        {
            auto b = std::make_unique<ui::StoneButton> (text, true, paramId != nullptr); b->textScale = 0.4f;
            if (paramId) { sampAtts.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (paramId), *b, nullptr)); b->setTooltip (help::forId (paramId)); }
            addAndMakeVisible (*b); sampGroup.push_back (b.get()); if (set) set->push_back (b.get()); sampButtons.push_back (std::move (b));
        };
        mk ("LOAD", nullptr, nullptr);                                                                                               // 0: loads into the slot being edited
        mk ("LOOP", "sample_loop", &sampSetA); mk ("REVERSE", "sample_reverse", &sampSetA); mk ("NORM", "sample_normalize", &sampSetA);   // 1-3: slot A
        mk ("LOOP", "sampleb_loop", &sampSetB); mk ("REVERSE", "sampleb_reverse", &sampSetB); mk ("NORM", "sampleb_normalize", &sampSetB); // 4-6: slot B
        mk ("OWN ENV", "sample_env_own", &sampSetA); mk ("OWN ENV", "sampleb_env_own", &sampSetB);                                    // 7-8
        mk ("B LAYER", "sampleb_indep", &sampSetB);                                                                                   // 9
        sampButtons[0]->onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> (sampSlot == 0 ? "Choose a sample" : "Choose a sample for slot B", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    if (sampSlot == 0) proc.loadSampleFile (f);
                    else { proc.loadSampleFile (f, false, true); if (auto* q = proc.apvts.getParameter ("sample_morph")) if (q->getValue() < 0.01f) q->setValueNotifyingHost (0.5f); }
                }
                sampDisp->repaint();
            });
        };
        const char* sn[2] = { "SLOT A", "SLOT B" };
        for (int i = 0; i < 2; ++i)
        {
            slotBtn[i] = std::make_unique<ui::StoneButton> (sn[i], true, false); slotBtn[i]->textScale = 0.4f; slotBtn[i]->setTooltip (help::forName (sn[i]));
            slotBtn[i]->onClick = [this, i] { sampSlot = i; updateSamplerView(); };
            addAndMakeVisible (*slotBtn[i]); sampGroup.push_back (slotBtn[i].get());
        }
        slotBtn[0]->setToggleState (true, juce::dontSendNotification);
    }
    {
        auto knob = [this] (std::vector<ui::Knob*>& list, std::vector<juce::Component*>& set, const char* id, const char* name)
        { auto* kn = addKnobCopy (id, name); list.push_back (kn); set.push_back (kn); sampGroup.push_back (kn); };
        for (auto k : { std::make_pair ("sample_root", "Root"), { "sample_octave", "Octave" }, { "sample_tune_ct", "Tune" }, { "sample_gain", "Gain" }, { "sample_start", "Start" }, { "sample_end", "End" }, { "loop_start", "Loop in" }, { "loop_end", "Loop out" } })
            knob (sampKnobsA, sampSetA, k.first, k.second);
        for (auto k : { std::make_pair ("sampleb_root", "Root"), { "sampleb_octave", "Octave" }, { "sampleb_tune_ct", "Tune" }, { "sampleb_gain", "Gain" }, { "sampleb_start", "Start" }, { "sampleb_end", "End" }, { "sampleb_loop_start", "Loop in" }, { "sampleb_loop_end", "Loop out" } })
            knob (sampKnobsB, sampSetB, k.first, k.second);
        for (auto k : { std::make_pair ("attack_ms", "Attack"), { "decay_ms", "Decay" }, { "sustain", "Sustain" }, { "release_ms", "Release" } }) knob (envKnobsVoice, sampEnvVoice, k.first, k.second);
        for (auto k : { std::make_pair ("sample_attack_ms", "Attack"), { "sample_decay_ms", "Decay" }, { "sample_sustain", "Sustain" }, { "sample_release_ms", "Release" } }) knob (envKnobsA, sampEnvA, k.first, k.second);
        for (auto k : { std::make_pair ("sampleb_attack_ms", "Attack"), { "sampleb_decay_ms", "Decay" }, { "sampleb_sustain", "Sustain" }, { "sampleb_release_ms", "Release" } }) knob (envKnobsB, sampEnvB, k.first, k.second);
    }
    mixSlider = std::make_unique<ui::MixSlider> (proc.apvts); addAndMakeVisible (*mixSlider);
    abSlider = std::make_unique<ui::MixSlider> (proc.apvts, "sample_morph", "A", "B"); addAndMakeVisible (*abSlider);
    loadB = std::make_unique<ui::StoneButton> ("LOAD B", true, false); loadB->textScale = 0.38f; addAndMakeVisible (*loadB);
    loadB->onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a sample for slot B", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        { auto f = fc.getResult(); if (f.existsAsFile()) { proc.loadSampleFile (f, false, true); if (auto* q = proc.apvts.getParameter ("sample_morph")) if (q->getValue() < 0.01f) q->setValueNotifyingHost (0.5f); } });
    };
    sampGroup.push_back (addNoteToggle ("chop_on", "CHOP", 0.42f));
    addStepper ("chop_mode", "Slice by", sampGroup); addStepper ("chop_count", "Slices", sampGroup); addStepper ("chop_grid", "Grid", sampGroup); addStepper ("chop_pitch", "Pitch", sampGroup);
    sampSetA.push_back (addStepper ("loop_xfade", "Loop fade", sampGroup)); sampSetB.push_back (addStepper ("sampleb_xfade", "Loop fade", sampGroup));

    // ---- master and filter ----
    addKnob ("master", "Master");
    addKnob ("cutoff_hz", "Cutoff"); addKnob ("resonance", "Reso");
    filterDisp = std::make_unique<ui::FilterDisplay> (proc); addAndMakeVisible (*filterDisp);
    const char* ft[4] = { "LP", "HP", "BP", "N" };
    for (int i = 0; i < 4; ++i)
    {
        ftype[i] = std::make_unique<ui::StoneButton> (ft[i], true, false);
        ftype[i]->onClick = [this, i] { setChoice ("filter_type", i); };
        addAndMakeVisible (*ftype[i]);
    }
    filterOn = std::make_unique<ui::StoneButton> ("FILTER", true, true); filterOn->textScale = 0.5f; addAndMakeVisible (*filterOn);
    filterOnAtt = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter ("filter_on"), *filterOn, nullptr);

    // ---- effect pads (bottom left) ----
    struct PadDef { const char* label; const char* id; };
    const PadDef defs[] = { { "TREMOLO", "trem_on" }, { "DELAY", "delay_on" }, { "COMP", "comp_on" }, { "CRUSH", "crush_on" }, { "PANNER", "pan_on" },
                         { "CHORUS", "chorus_on" }, { "FLANGER", "flanger_on" }, { "REVERB", "reverb_on" } };   // text only: the pads are small, so no icons
    for (auto& d : defs)
    {
        Pad pd; pd.id = d.id; pd.btn = std::make_unique<ui::StoneButton> (d.label, true, true); pd.btn->textScale = 0.3f; pd.btn->pad = true; pd.btn->setTooltip (help::forId (d.id));
        pd.att = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (d.id), *pd.btn, nullptr);
        addAndMakeVisible (*pd.btn); pads.push_back (std::move (pd));
    }

    // ---- waveform selector row, poly, effect shortcuts ----
    waveLeft  = std::make_unique<ui::StoneButton> ("<", true, false); waveLeft->onClick  = [this] { cycleChoice ("waveform", -1); };
    waveRight = std::make_unique<ui::StoneButton> (">", true, false); waveRight->onClick = [this] { cycleChoice ("waveform", +1); };
    waveName  = std::make_unique<ui::StoneButton> ("SAW", false, false);
    polyBtn   = std::make_unique<ui::StoneButton> ("POLY", true, false);
    polyBtn->onClick = [this] { auto* v = proc.apvts.getParameter ("voices"); const bool poly = proc.apvts.getRawParameterValue ("voices")->load() > 1.5f;
                                v->beginChangeGesture(); v->setValueNotifyingHost (v->convertTo0to1 (poly ? 1.0f : 8.0f)); v->endChangeGesture(); };
    allOn     = std::make_unique<ui::StoneButton> ("ALL ON", true, false);  allOn->onClick = [this] { setAllEffects (true); };
    bypassFx  = std::make_unique<ui::StoneButton> ("BYPASS FX", true, false); bypassFx->onClick = [this] { setAllEffects (false); };
    for (auto* b : { waveLeft.get(), waveRight.get(), waveName.get(), polyBtn.get(), allOn.get(), bypassFx.get() }) addAndMakeVisible (b);
    waveName->setEnabled (true); waveName->setInterceptsMouseClicks (false, false);

    // ---- FX tab ----
    for (auto k : { std::make_pair ("trem_depth", "Depth"), { "trem_rate_hz", "Rate" }, { "delay_time_ms", "Time" }, { "delay_feedback", "Fdbk" }, { "delay_mix", "Mix" },
                    { "comp_threshold_db", "Thresh" }, { "comp_ratio", "Ratio" }, { "comp_attack_ms", "Attack" }, { "comp_release_ms", "Release" }, { "comp_makeup_db", "Makeup" },
                    { "crush_bits", "Bits" }, { "crush_downsample", "Down" }, { "crush_mix", "Mix" }, { "pan_rate_hz", "Rate" }, { "pan_depth", "Depth" } })
        fxGroup.push_back (addKnob (k.first, k.second));
    for (auto id : { "trem_on", "delay_on", "comp_on", "crush_on", "pan_on", "delay_pingpong" })
    {
        const bool pp = juce::String (id) == "delay_pingpong";
        auto b = std::make_unique<ui::StoneButton> (pp ? "PING-PONG" : "ON", true, true); b->textScale = pp ? 0.36f : 0.45f; b->setTooltip (help::forId (id));
        fxAtts.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (id), *b, nullptr));
        addAndMakeVisible (*b); fxGroup.push_back (b.get()); fxToggles.push_back (std::move (b));
    }
    delayMode = std::make_unique<ui::StoneButton> ("CLEAN", true, false); delayMode->textScale = 0.4f;
    delayMode->onClick = [this] { cycleChoice ("delay_mode", +1); }; addAndMakeVisible (*delayMode); fxGroup.push_back (delayMode.get());
    panShape = std::make_unique<ui::StoneButton> ("SINE", true, false); panShape->textScale = 0.4f;
    panShape->onClick = [this] { cycleChoice ("pan_shape", +1); }; addAndMakeVisible (*panShape); fxGroup.push_back (panShape.get());

    // ---- FX 2 tab: chorus, flanger, reverb ----
    for (auto k : { std::make_pair ("chorus_rate_hz", "Rate"), { "chorus_depth", "Depth" }, { "chorus_mix", "Mix" },
                    { "flanger_rate_hz", "Rate" }, { "flanger_depth", "Depth" }, { "flanger_feedback", "Fdbk" }, { "flanger_mix", "Mix" },
                    { "reverb_size", "Size" }, { "reverb_damp", "Damp" }, { "reverb_predelay_ms", "Pre" }, { "reverb_mix", "Mix" } })
        fx2Group.push_back (addKnob (k.first, k.second));
    for (auto id : { "chorus_on", "flanger_on", "reverb_on" }) fx2Group.push_back (addNoteToggle (id, "ON", 0.45f));

    // ---- FX 3 tab: EQ, tape, widener, wah, lo-fi, OTT ----
    for (auto k : { std::make_pair ("eq_low_db", "Low"), { "eq_mid_hz", "Mid Hz" }, { "eq_mid_db", "Mid" }, { "eq_high_db", "High" },
                    { "tape_drive", "Drive" }, { "tape_tone", "Tone" }, { "tape_wobble", "Wow" }, { "widener_width", "Width" },
                    { "wah_rate_hz", "Rate" }, { "wah_depth", "Depth" }, { "wah_reso", "Reso" }, { "wah_mix", "Mix" },
                    { "lofi_tone_hz", "Tone" }, { "lofi_noise", "Noise" }, { "lofi_wobble", "Wow" }, { "lofi_mix", "Mix" },
                    { "ott_depth", "Depth" }, { "ott_time", "Time" }, { "ott_mix", "Mix" } })
        fx3Group.push_back (addKnob (k.first, k.second));
    for (auto id : { "eq_on", "tape_on", "widener_on", "wah_on", "lofi_on", "ott_on" }) fx3Group.push_back (addNoteToggle (id, "ON", 0.45f));
    wahMode = std::make_unique<ui::StoneButton> ("LFO", true, false); wahMode->textScale = 0.4f; wahMode->onClick = [this] { cycleChoice ("wah_mode", +1); };
    addAndMakeVisible (*wahMode); fx3Group.push_back (wahMode.get());
    addNoteToggle ("eq_on", "EQ", 0.42f, "eq_on#main"); addNoteToggle ("tape_on", "TAPE", 0.38f, "tape_on#main"); addNoteToggle ("lead_on", "LEAD", 0.38f, "lead_on");

    // ---- OSC 2 / OSC 3 tab ----
    for (const char* o : { "osc2", "osc3" })
    {
        const juce::String s (o);
        osc23Group.push_back (addNoteToggle ((s + "_on").toRawUTF8(), "ON", 0.45f));
        addStepper ((s + "_wave").toRawUTF8(), "Wave", osc23Group); addStepper ((s + "_oct").toRawUTF8(), "Octave", osc23Group); addStepper ((s + "_semi").toRawUTF8(), "Semitone", osc23Group);
        osc23Group.push_back (addKnob ((s + "_level").toRawUTF8(), "Level")); osc23Group.push_back (addKnob ((s + "_fine").toRawUTF8(), "Fine"));
    }

    // ---- FX 4 tab: talk box, spectral gate, pitch delay ----
    for (auto k : { std::make_pair ("talk_vowel", "Vowel"), { "talk_reso", "Reso" }, { "talk_rate_hz", "Rate" }, { "talk_depth", "Sweep" }, { "talk_mix", "Mix" },
                    { "sgate_threshold_db", "Thresh" }, { "sgate_depth", "Depth" }, { "sgate_release_ms", "Release" }, { "sgate_mix", "Mix" },
                    { "pdelay_time_ms", "Time" }, { "pdelay_feedback", "Fdbk" }, { "pdelay_semi", "Pitch" }, { "pdelay_mix", "Mix" } })
        fx4Group.push_back (addKnob (k.first, k.second));
    for (auto id : { "talk_on", "sgate_on", "pdelay_on" }) fx4Group.push_back (addNoteToggle (id, "ON", 0.45f));

    // ---- TEXTURE tab (opened from the Textures icon in the sidebar) ----
    for (auto k : { std::make_pair ("tex_level", "Level"), { "tex_pos", "Position" }, { "tex_spread", "Spread" }, { "tex_size_ms", "Grain" }, { "tex_density", "Density" }, { "tex_jitter_ct", "Jitter" } })
        { auto* kn = addKnob (k.first, k.second); texGroup.push_back (kn); texGrains.push_back (kn); }
    {
        const char* vn[2] = { "GRAINS", "SHAPE" };
        for (int i = 0; i < 2; ++i)
        {
            texViewBtn[i] = std::make_unique<ui::StoneButton> (vn[i], true, false); texViewBtn[i]->textScale = 0.4f; texViewBtn[i]->setTooltip (help::forName (vn[i]));
            texViewBtn[i]->onClick = [this, i] { texView = i; updateTextureView(); repaint(); }; addAndMakeVisible (*texViewBtn[i]); texGroup.push_back (texViewBtn[i].get());
        }
        texGroup.push_back (addNoteToggle ("tex_indep", "OWN FILE", 0.38f));
        loadTex = std::make_unique<ui::StoneButton> ("LOAD TEXTURE", true, false); loadTex->textScale = 0.3f; loadTex->setTooltip (help::forName ("LOAD TEXTURE")); addAndMakeVisible (*loadTex); texGroup.push_back (loadTex.get());
        loadTex->onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a file for the texture", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            { auto f = fc.getResult(); if (f.existsAsFile() && proc.loadTextureFile (f)) { if (auto* q = proc.apvts.getParameter ("tex_indep")) q->setValueNotifyingHost (1.0f); repaint(); } });
        };
        auto add = [this] (juce::Component* c, bool ownOnly) { texGroup.push_back (c); texShape.push_back (c); if (ownOnly) texOwnOnly.push_back (c); };
        add (addStepper ("tex_root", "Root", texGroup), true); add (addStepper ("tex_octave", "Octave", texGroup), true);
        add (addKnob ("tex_tune_ct", "Tune"), true); add (addNoteToggle ("tex_normalize", "NORMALIZE", 0.3f), true);
        add (addNoteToggle ("tex_env_own", "OWN ENVELOPE", 0.28f), true);
        for (auto k : { std::make_pair ("tex_attack_ms", "Attack"), { "tex_decay_ms", "Decay" }, { "tex_sustain", "Sustain" }, { "tex_release_ms", "Release" } }) add (addKnob (k.first, k.second), true);
        add (addNoteToggle ("tex_filter_on", "FILTER", 0.36f), false); add (addStepper ("tex_filter_type", "Type", texGroup), false);
        add (addKnob ("tex_cutoff_hz", "Cutoff"), false); add (addKnob ("tex_reso", "Reso"), false); add (addStepper ("tex_route", "Route", texGroup), false);
    }

    // ---- XY pad overlay (double-click the picture to open it) ----
    xyPad = std::make_unique<ui::XyPad> (proc); xyPad->onDoubleClick = [this] { showXy (false); }; addChildComponent (*xyPad);
    for (auto k : { std::make_pair ("macro_1", "Macro 1"), { "macro_2", "Macro 2" } }) { auto* kn = addKnob (k.first, k.second); kn->setVisible (false); xyGroup.push_back (kn); }

    // ---- ENV tab: two modulation envelopes (matrix sources Env 1 / Env 2) ----
    for (int n = 1; n <= 2; ++n)
        for (auto k : { std::make_pair ("_attack_ms", "Attack"), { "_decay_ms", "Decay" }, { "_sustain", "Sustain" }, { "_release_ms", "Release" } })
            envGroup.push_back (addKnob ((juce::String ("menv") + juce::String (n) + k.first).toRawUTF8(), k.second));

    // ---- MOD tab: two LFOs and the matrix ----
    for (int n = 1; n <= 2; ++n)
    {
        const juce::String t = "lfo" + juce::String (n);
        addStepper ((t + "_shape").toRawUTF8(), "Shape", modGroup); addStepper ((t + "_sync").toRawUTF8(), "Sync", modGroup);
        modGroup.push_back (addKnob ((t + "_rate_hz").toRawUTF8(), "Rate"));
    }
    for (int n = 1; n <= 4; ++n)
    {
        const juce::String t = "mod" + juce::String (n);
        addStepper ((t + "_src").toRawUTF8(), "Source", modGroup); addStepper ((t + "_dst").toRawUTF8(), "Target", modGroup);
        amountSliders.push_back (std::make_unique<ui::AmountSlider> (proc.apvts, t + "_amt", "AMOUNT")); amountSliders.back()->setTooltip (help::forId (t + "_amt")); addAndMakeVisible (*amountSliders.back()); modGroup.push_back (amountSliders.back().get());
    }
    addNoteToggle ("vib_on", "VIB", 0.42f); addNoteToggle ("porta_on", "PORTA", 0.38f);
    for (auto id : { "lfo1_keysync", "lfo2_keysync" }) modGroup.push_back (addNoteToggle (id, "KEY SYNC", 0.36f));
    addKnob ("vib_rate_hz", "V.Rate"); addKnob ("vib_depth_ct", "V.Depth"); addKnob ("porta_ms", "Glide");

    // ---- sidebar: the eight icon buttons, and the CRAFT / UNDO / REDO / CHAOS buttons over the picture ----
    const char* side[8] = { "Presets", "Chords", "One-shots", "Textures", "Expand", "History", "Save", "Init" };
    for (int i = 0; i < 8; ++i)
    {
        const bool live = true;
        auto b = std::make_unique<ui::IconButton> (side[i], i, live);
        if (i == 0) b->onClick = [this] { showBrowser (! browser->isVisible()); };
        if (i == 1) b->onClick = [this] { setTab (6); };
        if (i == 3) b->onClick = [this] { setTab (11); };
        if (i == 2) b->onClick = [this]
        {
            auto dir = RelicProcessor::presetsRoot().getSiblingFile ("Samples"); dir.createDirectory();
            auto files = dir.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a"); files.sort();
            juce::PopupMenu m; m.addSectionHeader ("ONE-SHOTS  (" + dir.getFullPathName() + ")");
            for (int k = 0; k < files.size() && k < 400; ++k) m.addItem (1000 + k, files[k].getRelativePathFrom (dir));
            if (files.isEmpty()) m.addItem (1, "(empty: put audio files in that folder)", false, false);
            m.addSeparator(); m.addItem (2, "Open the folder");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, files, dir] (int r) { if (r == 2) dir.revealToUser(); else if (r >= 1000 && r - 1000 < files.size()) { proc.loadSampleFile (files[r - 1000]); setTab (0); } });
        };
        if (i == 4) b->onClick = [this] { expanded = ! expanded; setSize (expanded ? 1400 : 1100, (int) ((expanded ? 1400 : 1100) * DH / DW)); };
        if (i == 5) b->onClick = [this]
        {
            juce::PopupMenu m; m.addSectionHeader ("HISTORY"); m.addItem (1, "Undo   (" + juce::String (proc.undoCount()) + " steps back)", proc.canUndo()); m.addItem (2, "Redo   (" + juce::String (proc.redoCount()) + " steps forward)", proc.canRedo());
            m.addSeparator(); m.addItem (3, "Undo everything since loading", proc.canUndo());
            m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r) { if (r == 1) proc.undo(); else if (r == 2) proc.redo(); else if (r == 3) { int n = 0; while (proc.undo() && n++ < 200) {} } });
        };
        if (i == 6) b->onClick = [this] { doSave(); };
        if (i == 7) b->onClick = [this]
        {
            // Init wipes the whole sound, so ask first (History can still step back through the changes).
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon).withTitle ("Reset everything?")
                .withMessage ("Init sets every control back to its default and removes the loaded samples. Anything you have not saved is lost, though History can step back through it.")
                .withButton ("Reset").withButton ("Cancel").withAssociatedComponent (this),
                [this] (int res) { if (res == 1) { proc.initPreset(); if (browser->isVisible()) browser->refresh(); } });
        };
        addAndMakeVisible (*b); sideBtns.emplace_back (std::move (b), juce::Rectangle<float> (29.0f, 98.0f + i * 73.0f, 66.0f, 66.0f));
    }
    craftBtn = placeholder ("CRAFT", 156, 560, 320, 54); craftBtn->icon = ui::iconCell (ui::skin().iconsAction, 0, true);
    undoBtn = placeholder ("UNDO", 141, 624, 112, 40); undoBtn->textScale = 0.3f; undoBtn->icon = ui::iconCell (ui::skin().iconsAction, 1, true);
    redoBtn = placeholder ("REDO", 264, 624, 112, 40); redoBtn->textScale = 0.3f; redoBtn->icon = ui::iconCell (ui::skin().iconsAction, 2, true);
    chaosBtn = placeholder ("CHAOS", 387, 624, 112, 40); chaosBtn->textScale = 0.3f; chaosBtn->icon = ui::iconCell (ui::skin().iconsAction, 3, true);
    lockFxBtn = placeholder ("LOCK FX", 35, 740, 115, 34); fxAB = placeholder ("EFFECTS A", 406, 740, 115, 34);
    for (auto* b : { craftBtn, undoBtn, redoBtn, chaosBtn, lockFxBtn, fxAB }) b->setEnabled (true);
    fxAB->onClick = [this] { proc.switchFxSlot(); }; fxAB->textScale = 0.38f;
    lockFxBtn->setClickingTogglesState (true); lockFxBtn->textScale = 0.34f;
    craftBtn->onClick = [this] { proc.randomizeSound (false); };
    chaosBtn->onClick = [this] { proc.randomizeSound (true); };
    undoBtn->onClick = [this] { proc.undo(); };
    redoBtn->onClick = [this] { proc.redo(); };
    lockFxBtn->onClick = [this] { proc.lockFx = lockFxBtn->getToggleState(); };
    splitBtn = placeholder ("SPLIT", 565, 840, 64, 32); splitBtn->setEnabled (true); splitBtn->setClickingTogglesState (true); splitBtn->textScale = 0.34f;
    noteAtts.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter ("split_on"), *splitBtn, nullptr));
    splitRc = std::make_unique<RightClick>();
    splitRc->f = [this]
    {
        juce::PopupMenu m; m.addSectionHeader ("SPLIT POINT  (below = sample only, from here up = synth only)");
        for (int n = 36; n <= 84; n += 6) m.addItem (n, juce::MidiMessage::getMidiNoteName (n, true, true, 3));
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r) { if (r > 0) if (auto* q = proc.apvts.getParameter ("split_note")) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 ((float) r)); q->endChangeGesture(); } });
    };
    splitBtn->addMouseListener (splitRc.get(), false);
    auto* octUp = placeholder ("+", 1215, 840, 60, 40); auto* octDn = placeholder ("-", 1215, 925, 60, 30);
    for (auto* b : { octUp, octDn }) { b->setEnabled (true); b->textScale = 0.6f; }
    auto shiftOct = [this] (int d)
    {
        const int k = juce::jlimit (-3, 4, octShift + d); if (k == octShift) return;
        octShift = k; keyboard.setAvailableRange (36 + 12 * k, 71 + 12 * k); keyboard.setLowestVisibleKey (36 + 12 * k);
        keyboard.setKeyWidth ((float) keyboard.getWidth() / 21.0f); proc.keyboardState.allNotesOff (0);
    };
    octUp->onClick = [shiftOct] { shiftOct (+1); }; octDn->onClick = [shiftOct] { shiftOct (-1); };

    // ---- presets: bar arrows, save button, browser ----
    saveBtn = std::make_unique<ui::StoneButton> ("SAVE", true, false); saveBtn->textScale = 0.4f; saveBtn->onClick = [this] { doSave(); }; addAndMakeVisible (*saveBtn);
    prevPreset = std::make_unique<ui::StoneButton> ("<", true, false); prevPreset->onClick = [this] { stepPreset (-1); }; addAndMakeVisible (*prevPreset);
    nextPreset = std::make_unique<ui::StoneButton> (">", true, false); nextPreset->onClick = [this] { stepPreset (+1); }; addAndMakeVisible (*nextPreset);
    scenePrev = std::make_unique<ui::StoneButton> ("<", true, false); scenePrev->onClick = [this] { setScene (sceneIdx - 1); }; addAndMakeVisible (*scenePrev);
    sceneNext = std::make_unique<ui::StoneButton> (">", true, false); sceneNext->onClick = [this] { setScene (sceneIdx + 1); }; addAndMakeVisible (*sceneNext);
    barClick = std::make_unique<ui::ClickArea>(); barClick->onClick = [this] { showBrowser (! browser->isVisible()); }; addAndMakeVisible (*barClick);
    browser = std::make_unique<ui::PresetBrowser> (proc); browser->onClose = [this] { showBrowser (false); };
    browser->onSave = [this] { doSave(); }; addChildComponent (*browser);

    // ---- keyboard ----
    keyboard.setAvailableRange (36, 71);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, parchment);
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff17121c));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, goldDim);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, lime.withAlpha (0.55f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, gold.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (keyboard);

    setResizeLimits (742, 530, 2226, 1590);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) DW / DH);
    setSize (1100, (int) (1100 * DH / DW));
    applyHelp();
    setTab (1);
    startTimerHz (15);
    timerCallback();
}

// Hover help for everything that is not created through addKnob / addStepper / addNoteToggle (those attach their own).
void RelicEditor::applyHelp()
{
    auto tip = [] (juce::SettableTooltipClient* c, const juce::String& text) { if (c != nullptr && text.isNotEmpty()) c->setTooltip (text); };
    const char* tabNames[11] = { "SAMPLE", "OSC", "FX", "FX 2", "FX 3", "ARP", "CHORD", "MOD", "OSC 2", "ENV", "FX 4" };
    for (int i = 0; i < 11; ++i) tip (tabs[i].get(), help::forName (tabNames[i]));
    for (auto& sb : sideBtns) tip (sb.first.get(), help::forName (sb.first->getName()));
    tip (craftBtn, help::forName ("CRAFT")); tip (undoBtn, help::forName ("UNDO")); tip (redoBtn, help::forName ("REDO")); tip (chaosBtn, help::forName ("CHAOS"));
    tip (scenePrev.get(), help::forName ("scene_prev")); tip (sceneNext.get(), help::forName ("scene_next"));
    tip (saveBtn.get(), help::forName ("SAVE")); tip (prevPreset.get(), help::forName ("preset_prev")); tip (nextPreset.get(), help::forName ("preset_next")); tip (barClick.get(), help::forName ("preset_bar"));
    tip (lockFxBtn, help::forName ("LOCK FX")); tip (bypassFx.get(), help::forName ("BYPASS FX")); tip (allOn.get(), help::forName ("ALL ON")); tip (fxAB, help::forName ("EFFECTS A"));
    tip (polyBtn.get(), help::forName ("POLY")); tip (waveLeft.get(), help::forName ("wave_prev")); tip (waveRight.get(), help::forName ("wave_next"));
    tip (waveName.get(), help::forId ("waveform"));
    const char* ftNames[4] = { "LP", "HP", "BP", "N" };
    for (int i = 0; i < 4; ++i) tip (ftype[i].get(), help::forName (ftNames[i]));
    tip (filterOn.get(), help::forId ("filter_on")); tip (filterDisp.get(), help::forName ("filter_display"));
    tip (delayMode.get(), help::forId ("delay_mode")); tip (panShape.get(), help::forId ("pan_shape")); tip (wahMode.get(), help::forId ("wah_mode"));
    if (! sampButtons.empty()) tip (sampButtons[0].get(), help::forName ("LOAD"));
    for (size_t i = 1; i < sampButtons.size(); ++i)
    {
        static const char* ids[] = { "", "sample_loop", "sample_reverse", "sample_normalize" };
        if (i < 4) tip (sampButtons[i].get(), help::forId (ids[i]));
    }
    tip (loadB.get(), help::forName ("LOAD B")); tip (mixSlider.get(), help::forId ("synth_mix")); tip (abSlider.get(), help::forId ("sample_morph"));
    tip (splitBtn, help::forName ("SPLIT"));
    for (auto& ph : phs) if (ph.b->getButtonText() == "+" || ph.b->getButtonText() == "-") tip (ph.b.get(), help::forName (ph.b->getButtonText()));
    tip (wave.get(), help::forName ("wave_display")); tip (env.get(), help::forName ("env_display")); tip (sampDisp.get(), help::forName ("sample_display"));
    tip (chordDisp.get(), help::forName ("chord_display")); tip (arpDisp.get(), help::forName ("arp_display")); tip (xyPad.get(), help::forName ("xy_pad")); tip (meter.get(), help::forName ("meter"));
}

RelicEditor::~RelicEditor() { setLookAndFeel (nullptr); }

void RelicEditor::showXy (bool show)
{
    xyShown = show; xyPad->setVisible (show); for (auto* c : xyGroup) c->setVisible (show);
    if (show) { xyPad->toFront (false); for (auto* c : xyGroup) c->toFront (false); }
    repaint();
}
void RelicEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const float s = scale(); if (s <= 0.0f) return;
    if (! xyShown && juce::Rectangle<float> (124, 112, 384, 426).contains (e.position / s)) showXy (true);
}

// Shows the controls of the sample slot being edited. The envelope knobs edit the voice envelope until the slot is given an envelope of its own.
void RelicEditor::updateSamplerView()
{
    if (currentTab != 0 || sampDisp == nullptr) return;
    const bool b = sampSlot == 1;
    const bool ownA = proc.apvts.getRawParameterValue ("sample_env_own")->load() > 0.5f, ownB = proc.apvts.getRawParameterValue ("sampleb_env_own")->load() > 0.5f;
    const bool indep = proc.apvts.getRawParameterValue ("sampleb_indep")->load() > 0.5f, ownSel = b ? ownB : ownA;
    auto show = [] (std::vector<juce::Component*>& v, bool on) { for (auto* c : v) c->setVisible (on); };
    show (sampSetA, ! b); show (sampSetB, b);
    show (sampEnvVoice, ! ownSel); show (sampEnvA, ! b && ownA); show (sampEnvB, b && ownB);
    // slot B's own settings only count once B is a layer of its own; until then they are shown dimmed
    for (auto* c : sampSetB) c->setAlpha (c == sampButtons[9].get() || indep ? 1.0f : 0.45f);
    for (auto* c : sampEnvB) c->setAlpha (indep ? 1.0f : 0.45f);
    for (int i = 0; i < 2; ++i) slotBtn[i]->setToggleState (i == sampSlot, juce::dontSendNotification);
    sampDisp->setSlot (sampSlot);
}

void RelicEditor::updateTextureView()
{
    if (currentTab != 11 || texViewBtn[0] == nullptr) return;
    for (auto* c : texGrains) c->setVisible (texView == 0);
    for (auto* c : texShape) c->setVisible (texView == 1);
    const bool own = proc.apvts.getRawParameterValue ("tex_indep")->load() > 0.5f;
    for (auto* c : texOwnOnly) c->setAlpha (own ? 1.0f : 0.45f);   // these only count once the texture plays its own file
    for (int i = 0; i < 2; ++i) texViewBtn[i]->setToggleState (i == texView, juce::dontSendNotification);
}

void RelicEditor::setTab (int t)
{
    currentTab = t;
    for (int i = 0; i < 11; ++i) tabs[i]->setToggleState (i == t, juce::dontSendNotification);
    for (auto* c : oscGroup) c->setVisible (t == 1);
    for (auto* c : sampGroup) c->setVisible (t == 0);
    meter->setVisible (t == 1);
    for (auto* c : fxGroup)  c->setVisible (t == 2);
    for (auto* c : fx2Group) c->setVisible (t == 3);
    for (auto* c : fx3Group) c->setVisible (t == 4);
    for (auto* c : arpGroup) c->setVisible (t == 5);
    for (auto* c : chordGroup) c->setVisible (t == 6);
    for (auto* c : modGroup) c->setVisible (t == 7);
    for (auto* c : osc23Group) c->setVisible (t == 8);
    for (auto* c : envGroup) c->setVisible (t == 9);
    for (auto* c : fx4Group) c->setVisible (t == 10);
    for (auto* c : texGroup) c->setVisible (t == 11);
    updateTextureView();
    updateSamplerView();
    repaint();
}

void RelicEditor::timerCallback()
{
    noteToggles["arp_run"]->setButtonText (proc.apvts.getRawParameterValue ("arp_run")->load() > 0.5f ? "GO" : "STOP");
    fxAB->setButtonText (proc.fxSlotActive() == 0 ? "EFFECTS A" : "EFFECTS B"); fxAB->setToggleState (proc.fxSlotActive() == 1, juce::dontSendNotification);
    splitBtn->setButtonText (proc.apvts.getRawParameterValue ("split_on")->load() > 0.5f ? "SPLIT " + juce::MidiMessage::getMidiNoteName ((int) proc.apvts.getRawParameterValue ("split_note")->load(), true, true, 3) : juce::String ("SPLIT"));
    updateSamplerView(); updateTextureView();
    proc.historyTick(); undoBtn->setEnabled (proc.canUndo()); redoBtn->setEnabled (proc.canRedo()); lockFxBtn->setToggleState (proc.lockFx.load(), juce::dontSendNotification);
    if (proc.getPresetName() != lastPresetName) { lastPresetName = proc.getPresetName(); repaint (R (600, 690, 590, 52)); }
    if (auto* c = choiceParam (proc.apvts, "waveform")) waveName->setButtonText (c->getCurrentChoiceName().toUpperCase());
    const int ftIdx = (int) proc.apvts.getRawParameterValue ("filter_type")->load();
    for (int i = 0; i < 4; ++i) ftype[i]->setToggleState (i == ftIdx, juce::dontSendNotification);
    polyBtn->setToggleState (proc.apvts.getRawParameterValue ("voices")->load() > 1.5f, juce::dontSendNotification);
    if (auto* c = choiceParam (proc.apvts, "delay_mode")) delayMode->setButtonText (c->getCurrentChoiceName().toUpperCase());
    if (auto* c = choiceParam (proc.apvts, "pan_shape")) panShape->setButtonText (c->getCurrentChoiceName().toUpperCase());
    if (auto* c = choiceParam (proc.apvts, "wah_mode")) wahMode->setButtonText (c->getCurrentChoiceName() == "Envelope" ? "ENV" : "LFO");
}

juce::Rectangle<int> RelicEditor::R (float x, float y, float w, float h) const
{
    const float s = scale(); return { (int) (x * s), (int) (y * s), (int) (w * s), (int) (h * s) };
}

void RelicEditor::paint (juce::Graphics& g)
{
    g.fillAll (stone);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.addTransform (juce::AffineTransform::scale (scale()));
    if (bgImg.isValid()) ui::drawScaledCached (g, bgImg, bgImg.getBounds(), { 0.0f, 0.0f, DW, DH });
    auto frame = [&] (float x, float y, float w, float h, juce::Colour = juce::Colours::transparentBlack)
    {
        if (frameImg.isValid()) ui::nineSlice (g, frameImg, frameImg.getBounds(), { (int) x, (int) y, (int) w, (int) h }, 250, (int) juce::jmin (30.0f, w / 3, h / 3));
        else { g.setColour (stone); g.fillRoundedRectangle (x, y, w, h, 14.0f); g.setColour (goldDim); g.drawRoundedRectangle (x + 1, y + 1, w - 2, h - 2, 14.0f, 3.0f); }
    };
    // banner: logo plus small texts
    g.setColour (parchment); ui::drawText (g, "McBABE! Labs", juce::Rectangle<float> (60, 24, 400, 44), 36, juce::Justification::left);
    ui::drawText (g, "v0.1", juce::Rectangle<float> (1020, 30, 380, 36), 26, juce::Justification::right);
    if (logoImg.isValid())
    {
        const int lh = 108, lw = (int) (lh * (float) logoImg.getWidth() / logoImg.getHeight());
        ui::drawScaledCached (g, logoImg, logoImg.getBounds(), { (float) ((int) (DW / 2) - lw / 2), -6.0f, (float) lw, (float) lh });
    }
    // left: scene art in its frame
    frame (112, 100, 408, 582);
    if (sceneImg.isValid())
    {
        g.saveState();
        juce::Path clip; clip.addRoundedRectangle (124.0f, 112.0f, 384.0f, 558.0f, 6.0f); g.reduceClipRegion (clip);
        const float sc = juce::jmax (384.0f / sceneImg.getWidth(), 558.0f / sceneImg.getHeight());
        const int dw = (int) (sceneImg.getWidth() * sc), dh = (int) (sceneImg.getHeight() * sc);
        ui::drawScaledCached (g, sceneImg, sceneImg.getBounds(), { (float) (124 - (int) ((dw - 384) * kScenes[sceneIdx].focusX)), (float) (112 - (dh - 558) / 2), (float) dw, (float) dh });
        // darken the bottom of the picture so CRAFT, UNDO, REDO and CHAOS read clearly over it
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.0f), 0.0f, 520.0f, juce::Colours::black.withAlpha (0.62f), 0.0f, 670.0f, false));
        g.fillRect (124.0f, 520.0f, 384.0f, 150.0f);
        g.restoreState();
    }
    frame (12, 92, 100, 590);
    // centre panel
    frame (540, 92, 742, 560);
    if (currentTab == 11)
    {
        g.setColour (display); g.fillRoundedRectangle (556, 170, 716, 462, 8.0f); g.setColour (goldDim); g.drawRoundedRectangle (556, 170, 716, 462, 8.0f, 2.0f);
        g.setColour (parchment); ui::drawText (g, "TEXTURE", juce::Rectangle<float> (556, 176, 716, 28), 22, juce::Justification::centred);
        g.setColour (lime.withAlpha (0.7f)); ui::drawText (g, (proc.apvts.getRawParameterValue ("tex_indep")->load() > 0.5f ? (proc.getSampleT() ? "A CLOUD OF GRAINS FROM THE TEXTURE FILE" : "OWN FILE IS ON: LOAD A TEXTURE FILE") : (proc.getSample() ? "A CLOUD OF GRAINS FROM THE LOADED SAMPLE, PLAYED BY THE KEYS" : "LOAD A SAMPLE ON THE SAMPLER TAB FIRST")), juce::Rectangle<float> (556, 214, 716, 22), 14, juce::Justification::centred);
    }
    if (currentTab == 10)
    {
        const char* titles[3] = { "TALK BOX", "SPECTRAL GATE", "PITCH DELAY" };
        for (int i = 0; i < 3; ++i)
        {
            const float x0 = 556 + i * 241.0f;
            g.setColour (display); g.fillRoundedRectangle (x0, 170, 232, 462, 8.0f); g.setColour (goldDim); g.drawRoundedRectangle (x0, 170, 232, 462, 8.0f, 2.0f);
            g.setColour (parchment); ui::drawText (g, titles[i], juce::Rectangle<float> (x0, 176, 232, 26), 22, juce::Justification::centred);
        }
    }
    if (currentTab == 9)
    {
        for (int i = 0; i < 2; ++i)
        {
            const float x0 = 556 + i * 366.0f;
            g.setColour (display); g.fillRoundedRectangle (x0, 170, 352, 462, 8.0f); g.setColour (goldDim); g.drawRoundedRectangle (x0, 170, 352, 462, 8.0f, 2.0f);
            g.setColour (parchment); ui::drawText (g, i == 0 ? "ENVELOPE 1" : "ENVELOPE 2", juce::Rectangle<float> (x0, 176, 352, 28), 22, juce::Justification::centred);
            g.setColour (lime.withAlpha (0.7f)); ui::drawText (g, "SOURCE: ENV " + juce::String (i + 1) + "  IN THE MOD TAB", juce::Rectangle<float> (x0, 214, 352, 20), 14, juce::Justification::centred);
        }
    }
    if (currentTab == 8)
    {
        for (int i = 0; i < 2; ++i)
        {
            const float x0 = 556 + i * 366.0f;
            g.setColour (display); g.fillRoundedRectangle (x0, 170, 352, 462, 8.0f); g.setColour (goldDim); g.drawRoundedRectangle (x0, 170, 352, 462, 8.0f, 2.0f);
            g.setColour (parchment); ui::drawText (g, i == 0 ? "OSCILLATOR 2" : "OSCILLATOR 3", juce::Rectangle<float> (x0, 176, 352, 28), 22, juce::Justification::centred);
        }
    }
    if (currentTab == 7)
    {
        for (int i = 0; i < 2; ++i)
        {
            const float x0 = 548 + i * 366.0f;
            g.setColour (display); g.fillRoundedRectangle (x0, 156, 358, 196, 8.0f); g.setColour (goldDim); g.drawRoundedRectangle (x0, 156, 358, 196, 8.0f, 2.0f);
            g.setColour (parchment); ui::drawText (g, i == 0 ? "LFO 1" : "LFO 2", juce::Rectangle<float> (x0, 160, 358, 28), 22, juce::Justification::centred);
        }
        g.setColour (display); g.fillRoundedRectangle (548, 362, 724, 274, 8.0f); g.setColour (goldDim); g.drawRoundedRectangle (548, 362, 724, 274, 8.0f, 2.0f);
        g.setColour (parchment); ui::drawText (g, "MODULATION MATRIX", juce::Rectangle<float> (548, 364, 724, 28), 22, juce::Justification::centred);
    }
    if (currentTab == 4)
    {
        const char* titles[6] = { "EQ", "TAPE", "WIDEN", "WAH", "LO-FI", "OTT" };
        for (int i = 0; i < 6; ++i)
        {
            const float x0 = 552 + i * 120.0f;
            g.setColour (display); g.fillRoundedRectangle (x0, 170, 116, 462, 8.0f); g.setColour (goldDim); g.drawRoundedRectangle (x0, 170, 116, 462, 8.0f, 2.0f);
            g.setColour (parchment); ui::drawText (g, titles[i], juce::Rectangle<float> (x0, 176, 116, 26), 22, juce::Justification::centred);
        }
    }
    if (currentTab == 3)
    {
        const char* titles[3] = { "CHORUS", "FLANGER", "REVERB" };
        for (int i = 0; i < 3; ++i)
        {
            const float x0 = 556 + i * 241.0f;
            g.setColour (display); g.fillRoundedRectangle (x0, 170, 232, 462, 8.0f); g.setColour (goldDim); g.drawRoundedRectangle (x0, 170, 232, 462, 8.0f, 2.0f);
            g.setColour (parchment); ui::drawText (g, titles[i], juce::Rectangle<float> (x0, 176, 232, 26), 22, juce::Justification::centred);
        }
    }
    if (currentTab == 2)
    {
        const char* titles[5] = { "TREMOLO", "DELAY", "COMPRESSOR", "CRUSHER", "PANNER" };
        for (int i = 0; i < 5; ++i)
        {
            const float x0 = 556 + i * 145.0f;
            g.setColour (display); g.fillRoundedRectangle (x0, 170, 138, 462, 8.0f); g.setColour (goldDim); g.drawRoundedRectangle (x0, 170, 138, 462, 8.0f, 2.0f);
            g.setColour (parchment); ui::drawText (g, titles[i], juce::Rectangle<float> (x0, 176, 138, 26), i == 2 ? 17.0f : 22.0f, juce::Justification::centred);
        }
    }
    // right column
    frame (1290, 70, 184, 120); g.setColour (parchment); ui::drawText (g, "MASTER", juce::Rectangle<float> (1290, 76, 184, 34), 28, juce::Justification::centred);
    frame (1290, 380, 184, 276); g.setColour (parchment); ui::drawText (g, "FILTER", juce::Rectangle<float> (1290, 386, 184, 34), 28, juce::Justification::centred);
    frame (1290, 690, 184, 260);
    // bottom left: effect pads frame; centre: preset bar
    frame (12, 684, 520, 280);
    frame (545, 684, 735, 280);
    g.setColour (juce::Colour (0xff120f16)); g.fillRoundedRectangle (610, 694, 570, 44, 8.0f);
    { const auto nm = proc.getPresetName().toUpperCase(); g.setColour (gold); ui::drawText (g, nm, juce::Rectangle<float> (618, 696, 554, 40), juce::jlimit (20.0f, 34.0f, 34.0f * 20.0f / (float) juce::jmax (1, nm.length())), juce::Justification::centred); }
}

void RelicEditor::resized()
{
    for (int i = 0; i < 11; ++i) tabs[i]->setBounds (R (548 + i * 66.0f, 102, 62, 44));

    // OSC tab
    wave->setBounds (R (548, 156, 724, 232)); env->setBounds (R (548, 394, 360, 82)); meter->setBounds (R (1176, 490, 92, 150));
    const char* oscIds[6] = { "pulse_width", "osc_level", "unison_voices", "unison_detune", "unison_spread", "voices" };
    for (int i = 0; i < 6; ++i) K (oscIds[i])->setBounds (R (928 + (i % 3) * 82.0f, 396 + (i / 3) * 124.0f, 76, 118));
    const char* adsr[4] = { "attack_ms", "decay_ms", "sustain", "release_ms" };
    for (int i = 0; i < 4; ++i) K (adsr[i])->setBounds (R (556 + i * 92.0f, 486, 86, 156));

    // SAMPLER tab
    sampDisp->setBounds (R (548, 156, 724, 158));
    // Left: slot selector and the buttons of the slot being edited (the A and B sets share the same places). Right: a 6 x 2 knob grid.
    slotBtn[0]->setBounds (R (548, 394, 112, 38)); slotBtn[1]->setBounds (R (668, 394, 112, 38));
    sampButtons[0]->setBounds (R (548, 438, 112, 38));
    for (int k = 0; k < 2; ++k)
    {
        const size_t o = k == 0 ? 1 : 4;
        sampButtons[o]->setBounds (R (668, 438, 112, 38)); sampButtons[o + 1]->setBounds (R (548, 482, 112, 38)); sampButtons[o + 2]->setBounds (R (668, 482, 112, 38));
        sampButtons[(size_t) (7 + k)]->setBounds (R (548, 526, 112, 38));
    }
    sampButtons[9]->setBounds (R (668, 526, 112, 38));
    auto cell = [] (int col, int row) { return juce::Rectangle<float> (790.0f + col * 78.0f, 396.0f + row * 124.0f, 74.0f, 118.0f); };
    for (auto* list : { &sampKnobsA, &sampKnobsB })
        for (size_t i = 0; i < list->size(); ++i) { const auto c = i < 6 ? cell ((int) i, 0) : cell ((int) i - 6, 1); (*list)[i]->setBounds (R (c.getX(), c.getY(), c.getWidth(), c.getHeight())); }
    for (auto* list : { &envKnobsVoice, &envKnobsA, &envKnobsB })
        for (size_t i = 0; i < list->size(); ++i) { const auto c = cell (2 + (int) i, 1); (*list)[i]->setBounds (R (c.getX(), c.getY(), c.getWidth(), c.getHeight())); }
    mixSlider->setBounds (R (610, 740, 570, 46));
    loadB->setBounds (R (870, 786, 92, 36)); abSlider->setBounds (R (970, 778, 220, 46));
    noteToggles["chop_on"]->setBounds (R (548, 340, 110, 44));
    { const char* cs[5] = { "chop_mode", "chop_count", "chop_grid", "chop_pitch", "loop_xfade" }; for (int i = 0; i < 5; ++i) steppers[cs[i]]->setBounds (R (668 + i * 121.0f, 322, 116, 66)); steppers["sampleb_xfade"]->setBounds (R (668 + 4 * 121.0f, 322, 116, 66)); }

    // Knob rows start at the same height in every effect panel, with roomier spacing when a panel has only two rows.
    auto rowY = [] (int rows, int row, float shift = 0.0f) { const float pitch = rows >= 3 ? 124.0f : 150.0f; return 268.0f + shift + (float) row * pitch; };

    // FX tab
    struct Slot { int panel, slot; const char* knob; };
    const Slot slots[] = { { 0, 0, "trem_depth" }, { 0, 1, "trem_rate_hz" }, { 1, 0, "delay_time_ms" }, { 1, 1, "delay_feedback" }, { 1, 2, "delay_mix" },
        { 2, 0, "comp_threshold_db" }, { 2, 1, "comp_ratio" }, { 2, 2, "comp_attack_ms" }, { 2, 3, "comp_release_ms" }, { 2, 4, "comp_makeup_db" },
        { 3, 0, "crush_bits" }, { 3, 1, "crush_downsample" }, { 3, 2, "crush_mix" }, { 4, 0, "pan_rate_hz" }, { 4, 1, "pan_depth" } };
    const int fxCount[5] = { 2, 3, 5, 3, 2 }; const float fxShift[5] = { 0.0f, -12.0f, 0.0f, 0.0f, 0.0f };
    for (auto& s : slots) { const int rows = (fxCount[s.panel] + 1) / 2; K (s.knob)->setBounds (R (556 + s.panel * 145.0f + 4 + (s.slot % 2) * 66.0f, rowY (rows, s.slot / 2, fxShift[s.panel]), 64, 120)); }
    for (int i = 0; i < 5; ++i) fxToggles[(size_t) i]->setBounds (R (556 + i * 145.0f + 10, 208, 118, 32));
    fxToggles[5]->setBounds (R (556 + 145.0f + 14, 540, 116, 36)); delayMode->setBounds (R (556 + 145.0f + 14, 582, 116, 36));
    panShape->setBounds (R (556 + 4 * 145.0f + 14, 430, 116, 40));

    // FX 4 tab
    {
        const char* ids[3][5] = { { "talk_vowel", "talk_reso", "talk_rate_hz", "talk_depth", "talk_mix" }, { "sgate_threshold_db", "sgate_depth", "sgate_release_ms", "sgate_mix", nullptr }, { "pdelay_time_ms", "pdelay_feedback", "pdelay_semi", "pdelay_mix", nullptr } };
        const char* on[3] = { "talk_on", "sgate_on", "pdelay_on" };
        for (int pnl = 0; pnl < 3; ++pnl)
        {
            const float x0 = 556 + pnl * 241.0f; noteToggles[on[pnl]]->setBounds (R (x0 + 57, 208, 118, 32));
            int n = 0; for (int k = 0; k < 5; ++k) if (ids[pnl][k]) ++n;
            for (int k = 0; k < 5; ++k) if (ids[pnl][k]) K (ids[pnl][k])->setBounds (R (x0 + 8 + (k % 3) * 74.0f, rowY ((n + 2) / 3, k / 3), 72, 120));
        }
    }

    // XY overlay over the picture
    xyPad->setBounds (R (124, 112, 384, 426)); K ("macro_1")->setBounds (R (168, 412, 120, 120)); K ("macro_2")->setBounds (R (346, 412, 120, 120));

    // TEXTURE tab
    texViewBtn[0]->setBounds (R (566, 246, 130, 38)); texViewBtn[1]->setBounds (R (704, 246, 130, 38));
    noteToggles["tex_indep"]->setBounds (R (860, 246, 170, 38)); loadTex->setBounds (R (1040, 246, 220, 38));
    { const char* ids[6] = { "tex_level", "tex_pos", "tex_spread", "tex_size_ms", "tex_density", "tex_jitter_ct" };
      for (int i = 0; i < 6; ++i) K (ids[i])->setBounds (R (580 + (i % 3) * 230.0f, 300 + (i / 3) * 166.0f, 170, 156)); }
    steppers["tex_root"]->setBounds (R (566, 298, 170, 70)); steppers["tex_octave"]->setBounds (R (744, 298, 170, 70));
    noteToggles["tex_normalize"]->setBounds (R (930, 314, 150, 38)); K ("tex_tune_ct")->setBounds (R (1110, 292, 100, 116));
    noteToggles["tex_env_own"]->setBounds (R (566, 440, 150, 38));
    { const char* ids[4] = { "tex_attack_ms", "tex_decay_ms", "tex_sustain", "tex_release_ms" };
      for (int i = 0; i < 4; ++i) K (ids[i])->setBounds (R (730 + i * 130.0f, 416, 100, 108)); }
    noteToggles["tex_filter_on"]->setBounds (R (566, 566, 110, 38)); steppers["tex_filter_type"]->setBounds (R (684, 548, 150, 70));
    K ("tex_cutoff_hz")->setBounds (R (846, 530, 100, 100)); K ("tex_reso")->setBounds (R (954, 530, 100, 100)); steppers["tex_route"]->setBounds (R (1070, 548, 190, 70));

    // ENV tab
    for (int n = 1; n <= 2; ++n)
        for (int k = 0; k < 4; ++k)
        {
            static const char* sfx[4] = { "_attack_ms", "_decay_ms", "_sustain", "_release_ms" };
            K ((juce::String ("menv") + juce::String (n) + sfx[k]).toRawUTF8())->setBounds (R (556 + (n - 1) * 366.0f + 24 + (k % 2) * 172.0f, 262 + (k / 2) * 190.0f, 130, 170));
        }

    // OSC 2 / 3 tab
    for (int i = 0; i < 2; ++i)
    {
        const juce::String s = i == 0 ? "osc2" : "osc3"; const float x0 = 556 + i * 366.0f;
        noteToggles[s + "_on"]->setBounds (R (x0 + 126, 214, 100, 38));
        steppers[s + "_wave"]->setBounds (R (x0 + 76, 266, 200, 70));
        steppers[s + "_oct"]->setBounds (R (x0 + 12, 350, 160, 70)); steppers[s + "_semi"]->setBounds (R (x0 + 180, 350, 160, 70));
        K ((s + "_level").toRawUTF8())->setBounds (R (x0 + 60, 440, 100, 176)); K ((s + "_fine").toRawUTF8())->setBounds (R (x0 + 192, 440, 100, 176));
    }

    // FX 3 tab
    {
        const char* ids[6][4] = { { "eq_low_db", "eq_mid_hz", "eq_mid_db", "eq_high_db" }, { "tape_drive", "tape_tone", "tape_wobble", nullptr }, { "widener_width", nullptr, nullptr, nullptr },
                                  { "wah_rate_hz", "wah_depth", "wah_reso", "wah_mix" }, { "lofi_tone_hz", "lofi_noise", "lofi_wobble", "lofi_mix" }, { "ott_depth", "ott_time", "ott_mix", nullptr } };
        const char* on[6] = { "eq_on", "tape_on", "widener_on", "wah_on", "lofi_on", "ott_on" };
        for (int pnl = 0; pnl < 6; ++pnl)
        {
            const float x0 = 552 + pnl * 120.0f; noteToggles[on[pnl]]->setBounds (R (x0 + 8, 208, 100, 32));
            int n = 0; for (int k = 0; k < 4; ++k) if (ids[pnl][k]) ++n;
            for (int k = 0; k < 4; ++k) if (ids[pnl][k]) K (ids[pnl][k])->setBounds (R (x0 + 4 + (k % 2) * 56.0f, rowY ((n + 1) / 2, k / 2, 0.0f), 54, 120));
        }
        wahMode->setBounds (R (552 + 3 * 120.0f + 8, 582, 100, 36));
    }
    noteToggles["eq_on#main"]->setBounds (R (1300, 200, 165, 90)); noteToggles["tape_on#main"]->setBounds (R (1300, 300, 80, 34)); noteToggles["lead_on"]->setBounds (R (1386, 300, 80, 34));

    // FX 2 tab
    const char* fx2Ids[3][4] = { { "chorus_rate_hz", "chorus_depth", "chorus_mix", nullptr }, { "flanger_rate_hz", "flanger_depth", "flanger_feedback", "flanger_mix" }, { "reverb_size", "reverb_damp", "reverb_predelay_ms", "reverb_mix" } };
    const char* fx2On[3] = { "chorus_on", "flanger_on", "reverb_on" };
    for (int pnl = 0; pnl < 3; ++pnl)
    {
        const float x0 = 556 + pnl * 241.0f;
        noteToggles[fx2On[pnl]]->setBounds (R (x0 + 57, 208, 118, 32));
        int n = 0; for (int k = 0; k < 4; ++k) if (fx2Ids[pnl][k]) ++n;
        for (int k = 0; k < 4; ++k) if (fx2Ids[pnl][k]) K (fx2Ids[pnl][k])->setBounds (R (x0 + 8 + (k % 3) * 74.0f, rowY ((n + 2) / 3, k / 3), 72, 120));
    }

    // MOD tab
    for (int n = 1; n <= 2; ++n)
    {
        const juce::String t = "lfo" + juce::String (n); const float x0 = 548 + (n - 1) * 366.0f;
        steppers[(t + "_shape").toRawUTF8()]->setBounds (R (x0 + 10, 190, 160, 70)); steppers[(t + "_sync").toRawUTF8()]->setBounds (R (x0 + 188, 190, 160, 70));
        K ((t + "_rate_hz").toRawUTF8())->setBounds (R (x0 + 10, 264, 92, 84));
        noteToggles[t + "_keysync"]->setBounds (R (x0 + 150, 286, 150, 40));
    }
    for (int n = 1; n <= 4; ++n)
    {
        const juce::String t = "mod" + juce::String (n); const float y = 394 + (n - 1) * 60.0f;
        steppers[(t + "_src").toRawUTF8()]->setBounds (R (558, y, 200, 56)); steppers[(t + "_dst").toRawUTF8()]->setBounds (R (766, y, 220, 56));
        amountSliders[(size_t) (n - 1)]->setBounds (R (996, y + 4, 266, 50));
    }
    noteToggles["vib_on"]->setBounds (R (1300, 700, 80, 44)); noteToggles["porta_on"]->setBounds (R (1388, 700, 80, 44));
    K ("vib_rate_hz")->setBounds (R (1298, 752, 84, 100)); K ("vib_depth_ct")->setBounds (R (1386, 752, 84, 100)); K ("porta_ms")->setBounds (R (1342, 846, 84, 92));

    // master / filter
    K ("master")->setBounds (R (1316, 106, 132, 72));
    filterDisp->setBounds (R (1302, 416, 160, 80));
    K ("cutoff_hz")->setBounds (R (1298, 500, 80, 72)); K ("resonance")->setBounds (R (1384, 500, 80, 72));
    for (int i = 0; i < 4; ++i) ftype[i]->setBounds (R (1300 + i * 43.0f, 576, 40, 30));
    filterOn->setBounds (R (1302, 610, 160, 26));

    // effect pads (3 x 2) and bottom rows
    for (size_t i = 0; i < pads.size(); ++i) pads[i].btn->setBounds (R (26 + (i % 4) * 124.0f, 790 + (i / 4) * 88.0f, 118, 80));
    waveLeft->setBounds (R (300, 692, 50, 40)); waveName->setBounds (R (352, 692, 130, 40)); waveRight->setBounds (R (484, 692, 36, 40));
    polyBtn->setBounds (R (150, 692, 140, 40));
    bypassFx->setBounds (R (157, 740, 125, 34)); allOn->setBounds (R (289, 740, 110, 34));

    for (auto& sb : sideBtns) sb.first->setBounds (R (sb.second.getX(), sb.second.getY(), sb.second.getWidth(), sb.second.getHeight()));

    saveBtn->setBounds (R (35, 692, 100, 40));
    scenePrev->setBounds (R (132, 120, 36, 36)); sceneNext->setBounds (R (464, 120, 36, 36));
    prevPreset->setBounds (R (566, 694, 40, 44)); nextPreset->setBounds (R (1184, 694, 40, 44)); barClick->setBounds (R (610, 694, 570, 44));
    browser->setBounds (R (548, 150, 724, 494));

    // CHORDS and ARP tabs
    chordDisp->setBounds (R (548, 156, 724, 170)); arpDisp->setBounds (R (548, 156, 724, 96));
    rhythmLane->setBounds (R (548, 258, 724, 76));
    const float colX[4] = { 548, 728, 908, 1088 };
    steppers["chord_key"]->setBounds (R (colX[0], 340, 172, 70)); steppers["chord_scale"]->setBounds (R (colX[1], 340, 172, 70));
    steppers["chord_type"]->setBounds (R (colX[2], 340, 172, 70)); steppers["chord_voicing"]->setBounds (R (colX[3], 340, 172, 70));
    steppers["chord_inversion"]->setBounds (R (colX[0], 430, 172, 70)); noteToggles["chord_lock"]->setBounds (R (colX[1], 450, 172, 44));
    K ("chord_strum_ms")->setBounds (R (colX[2], 424, 90, 124)); steppers["chord_mode"]->setBounds (R (colX[3], 430, 172, 70)); steppers["chord_shape"]->setBounds (R (colX[0], 552, 172, 70)); noteToggles["voice_lead"]->setBounds (R (colX[1], 572, 172, 44));
    steppers["arp_div"]->setBounds (R (colX[0], 340, 172, 70)); steppers["arp_order"]->setBounds (R (colX[3], 340, 172, 70)); steppers["arp_grouping"]->setBounds (R (colX[0], 552, 172, 70)); steppers["arp_wshape"]->setBounds (R (colX[1], 552, 172, 70)); steppers["arp_mode"]->setBounds (R (colX[1], 340, 172, 70)); steppers["arp_octaves"]->setBounds (R (colX[2], 340, 172, 70));
    noteToggles["arp_hold"]->setBounds (R (colX[0], 450, 172, 44));
    noteToggles["arp_run"]->setBounds (R (colX[3], 450, 172, 44));
    noteToggles["arp_two_hand"]->setBounds (R (colX[0], 500, 172, 44));
    noteToggles["arp_rhythm_on"]->setBounds (R (colX[2], 572, 172, 44));
    steppers["arp_rhythm_length"]->setBounds (R (colX[3], 552, 172, 70));
    { const char* ak[5] = { "arp_gate", "arp_swing", "arp_human", "arp_chance", "arp_spray" }; for (int i = 0; i < 5; ++i) K (ak[i])->setBounds (R (colX[1] + 72.0f * (float) i, 424, 72, 124)); }
    noteToggles["chord_on"]->setBounds (R (640, 785, 100, 36)); noteToggles["arp_on"]->setBounds (R (748, 785, 100, 36));

    // placeholders
    int reverbIdx = 8 + 0;   // the first placeholder is Reverb, created before the sidebar items
    for (size_t i = 0; i < phs.size(); ++i)
    {
        auto& p = phs[i];
        p.b->setBounds (R (p.r.getX(), p.r.getY(), p.r.getWidth(), p.r.getHeight()));
    }
    (void) reverbIdx;
    const int kw = R (572, 0, 0, 0).getX();
    keyboard.setBounds (R (636, 832, 570, 118));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 21.0f);
    (void) kw;
}
