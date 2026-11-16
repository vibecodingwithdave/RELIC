#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

using APVTS = juce::AudioProcessorValueTreeState;
using F = juce::AudioParameterFloat;
using R = juce::NormalisableRange<float>;

APVTS::ParameterLayout RelicProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    auto id = [] (const juce::String& s) { return juce::ParameterID { s, 1 }; };
    auto flt = [&] (const juce::String& i, const juce::String& n, float lo, float hi, float def, float skew = 1.0f)
        { v.push_back (std::make_unique<F> (id (i), n, R (lo, hi, 0.0f, skew), def)); };
    auto toggle = [&] (const juce::String& i, const juce::String& n, bool def)
        { v.push_back (std::make_unique<juce::AudioParameterBool> (id (i), n, def)); };
    auto choice = [&] (const juce::String& i, const juce::String& n, juce::StringArray c, int def)
        { v.push_back (std::make_unique<juce::AudioParameterChoice> (id (i), n, c, def)); };
    auto integer = [&] (const juce::String& i, const juce::String& n, int lo, int hi, int def)
        { v.push_back (std::make_unique<juce::AudioParameterInt> (id (i), n, lo, hi, def)); };

    // Oscillator
    choice ("waveform", "Waveform", { "Sine", "Saw", "Square", "Triangle", "Wavetable", "Pluck", "Bell" }, 1);
    flt ("pulse_width", "Pulse Width", 0.05f, 0.95f, 0.5f);
    flt ("osc_level", "Osc Level", 0.0f, 1.0f, 0.5f);
    integer ("unison_voices", "Unison Voices", 1, 7, 1);
    flt ("unison_detune", "Unison Detune", 0.0f, 50.0f, 12.0f);
    flt ("unison_spread", "Unison Spread", 0.0f, 1.0f, 0.5f);
    integer ("voices", "Voices", 1, 16, 8);
    for (const char* n : { "osc2", "osc3" })
    {
        const juce::String s (n), t = s == "osc2" ? "Osc 2 " : "Osc 3 ";
        toggle (s + "_on", t + "On", false);
        choice (s + "_wave", t + "Wave", { "Sine", "Saw", "Square", "Triangle", "Wavetable" }, s == "osc2" ? 1 : 2);
        flt (s + "_level", t + "Level", 0.0f, 1.0f, 0.5f);
        integer (s + "_oct", t + "Octave", -2, 2, 0);
        integer (s + "_semi", t + "Semi", -12, 12, 0);
        flt (s + "_fine", t + "Fine", -50.0f, 50.0f, s == "osc2" ? 7.0f : -7.0f);
    }
    // Modulation envelopes (matrix sources "Env 1" and "Env 2")
    for (int n = 1; n <= 2; ++n)
    {
        const juce::String s = "menv" + juce::String (n), t = "Mod Env " + juce::String (n) + " ";
        flt (s + "_attack_ms", t + "Attack Ms", 0.0f, 5000.0f, 2.0f, 0.3f);
        flt (s + "_decay_ms", t + "Decay Ms", 0.0f, 5000.0f, 500.0f, 0.3f);
        flt (s + "_sustain", t + "Sustain", 0.0f, 1.0f, 0.0f);
        flt (s + "_release_ms", t + "Release Ms", 0.0f, 5000.0f, 300.0f, 0.3f);
    }
    // Amp envelope
    flt ("attack_ms", "Attack Ms", 0.0f, 5000.0f, 5.0f, 0.3f);
    flt ("decay_ms", "Decay Ms", 0.0f, 5000.0f, 200.0f, 0.3f);
    flt ("sustain", "Sustain", 0.0f, 1.0f, 0.8f);
    flt ("release_ms", "Release Ms", 0.0f, 5000.0f, 200.0f, 0.3f);
    // Filter
    toggle ("filter_on", "Filter On", true);
    choice ("filter_type", "Filter Type", { "LP", "HP", "BP", "Notch" }, 0);
    choice ("filter_model", "Filter Model", { "Clean", "OBX", "SP-12", "MPC60" }, 0);
    choice ("filter_source", "Filter Source", { "All", "Synth", "Sample" }, 0);
    flt ("cutoff_hz", "Cutoff Hz", 20.0f, 20000.0f, 18000.0f, 0.3f);
    flt ("resonance", "Resonance", 0.0f, 1.0f, 0.0f);
    // Tremolo
    toggle ("trem_on", "Tremolo On", false);
    flt ("trem_depth", "Tremolo Depth", 0.0f, 1.0f, 0.5f);
    flt ("trem_rate_hz", "Tremolo Rate Hz", 0.1f, 20.0f, 4.0f, 0.5f);
    // Delay
    toggle ("delay_on", "Delay On", false);
    flt ("delay_time_ms", "Delay Time Ms", 1.0f, 2000.0f, 375.0f, 0.5f);
    flt ("delay_feedback", "Delay Feedback", 0.0f, 0.95f, 0.4f);
    flt ("delay_mix", "Delay Mix", 0.0f, 1.0f, 0.3f);
    choice ("delay_mode", "Delay Mode", { "Clean", "Tape", "Wash" }, 0);
    // Pitch delay
    toggle ("pdelay_on", "Pitch Delay On", false);
    flt ("pdelay_time_ms", "Pitch Delay Time Ms", 50.0f, 1500.0f, 400.0f, 0.5f);
    flt ("pdelay_feedback", "Pitch Delay Feedback", 0.0f, 0.9f, 0.5f);
    integer ("pdelay_semi", "Pitch Delay Semi", -12, 12, 7);
    flt ("pdelay_mix", "Pitch Delay Mix", 0.0f, 1.0f, 0.4f);
    // Talk box
    toggle ("talk_on", "Talk On", false);
    flt ("talk_vowel", "Talk Vowel", 0.0f, 1.0f, 0.0f);
    flt ("talk_reso", "Talk Reso", 0.0f, 1.0f, 0.6f);
    flt ("talk_rate_hz", "Talk Rate Hz", 0.0f, 8.0f, 0.0f);
    flt ("talk_depth", "Talk Depth", 0.0f, 1.0f, 0.5f);
    flt ("talk_mix", "Talk Mix", 0.0f, 1.0f, 1.0f);
    // Spectral gate
    toggle ("sgate_on", "Spectral Gate On", false);
    flt ("sgate_threshold_db", "Spectral Gate Threshold Db", -80.0f, -10.0f, -40.0f);
    flt ("sgate_depth", "Spectral Gate Depth", 0.0f, 1.0f, 1.0f);
    flt ("sgate_release_ms", "Spectral Gate Release Ms", 5.0f, 500.0f, 60.0f, 0.5f);
    flt ("sgate_mix", "Spectral Gate Mix", 0.0f, 1.0f, 1.0f);
    toggle ("delay_pingpong", "Delay Ping Pong", true);
    // Compressor
    toggle ("comp_on", "Comp On", false);
    flt ("comp_threshold_db", "Comp Threshold Db", -60.0f, 0.0f, -12.0f);
    flt ("comp_ratio", "Comp Ratio", 1.0f, 20.0f, 3.0f, 0.5f);
    flt ("comp_attack_ms", "Comp Attack Ms", 0.1f, 200.0f, 10.0f, 0.4f);
    flt ("comp_release_ms", "Comp Release Ms", 5.0f, 1000.0f, 120.0f, 0.4f);
    flt ("comp_makeup_db", "Comp Makeup Db", -12.0f, 24.0f, 0.0f);
    // Bitcrusher
    toggle ("crush_on", "Crush On", false);
    flt ("crush_bits", "Crush Bits", 1.0f, 16.0f, 8.0f);
    flt ("crush_downsample", "Crush Downsample", 1.0f, 50.0f, 1.0f, 0.5f);
    flt ("crush_mix", "Crush Mix", 0.0f, 1.0f, 1.0f);
    // Chorus, flanger, reverb
    toggle ("chorus_on", "Chorus On", false);
    flt ("chorus_rate_hz", "Chorus Rate Hz", 0.05f, 5.0f, 0.8f, 0.5f);
    flt ("chorus_depth", "Chorus Depth", 0.0f, 1.0f, 0.5f);
    flt ("chorus_mix", "Chorus Mix", 0.0f, 1.0f, 0.5f);
    toggle ("flanger_on", "Flanger On", false);
    flt ("flanger_rate_hz", "Flanger Rate Hz", 0.05f, 5.0f, 0.3f, 0.5f);
    flt ("flanger_depth", "Flanger Depth", 0.0f, 1.0f, 0.7f);
    flt ("flanger_feedback", "Flanger Feedback", -0.95f, 0.95f, 0.5f);
    flt ("flanger_mix", "Flanger Mix", 0.0f, 1.0f, 0.5f);
    toggle ("reverb_on", "Reverb On", false);
    flt ("reverb_size", "Reverb Size", 0.0f, 1.0f, 0.5f);
    flt ("reverb_damp", "Reverb Damping", 0.0f, 1.0f, 0.4f);
    flt ("reverb_predelay_ms", "Reverb Predelay Ms", 0.0f, 100.0f, 10.0f);
    flt ("reverb_mix", "Reverb Mix", 0.0f, 1.0f, 0.3f);
    // EQ, tape, widener, wah, lo-fi, OTT, lead mode
    toggle ("eq_on", "EQ On", false);
    flt ("eq_low_db", "EQ Low Db", -15.0f, 15.0f, 0.0f); flt ("eq_mid_hz", "EQ Mid Hz", 200.0f, 8000.0f, 1000.0f, 0.4f);
    flt ("eq_mid_db", "EQ Mid Db", -15.0f, 15.0f, 0.0f); flt ("eq_high_db", "EQ High Db", -15.0f, 15.0f, 0.0f);
    toggle ("tape_on", "Tape On", false);
    flt ("tape_drive", "Tape Drive", 0.0f, 1.0f, 0.4f); flt ("tape_tone", "Tape Tone", 0.0f, 1.0f, 0.7f); flt ("tape_wobble", "Tape Wobble", 0.0f, 1.0f, 0.2f);
    toggle ("widener_on", "Widener On", false);
    flt ("widener_width", "Widener Width", 0.0f, 2.0f, 1.4f);
    toggle ("wah_on", "Wah On", false);
    choice ("wah_mode", "Wah Mode", { "LFO", "Envelope" }, 0);
    flt ("wah_rate_hz", "Wah Rate Hz", 0.1f, 8.0f, 1.2f, 0.5f); flt ("wah_depth", "Wah Depth", 0.0f, 1.0f, 0.8f);
    flt ("wah_reso", "Wah Reso", 0.0f, 1.0f, 0.5f); flt ("wah_mix", "Wah Mix", 0.0f, 1.0f, 1.0f);
    toggle ("lofi_on", "Lofi On", false);
    flt ("lofi_tone_hz", "Lofi Tone Hz", 1000.0f, 12000.0f, 4500.0f, 0.5f); flt ("lofi_noise", "Lofi Noise", 0.0f, 1.0f, 0.3f);
    flt ("lofi_wobble", "Lofi Wobble", 0.0f, 1.0f, 0.3f); flt ("lofi_mix", "Lofi Mix", 0.0f, 1.0f, 1.0f);
    toggle ("ott_on", "OTT On", false);
    flt ("ott_depth", "OTT Depth", 0.0f, 1.0f, 0.5f); flt ("ott_time", "OTT Time", 0.0f, 1.0f, 0.5f); flt ("ott_mix", "OTT Mix", 0.0f, 1.0f, 1.0f);
    toggle ("lead_on", "Lead On", false);
    // LFOs, modulation matrix, vibrato, portamento
    for (int n = 1; n <= 2; ++n)
    {
        const juce::String t = "lfo" + juce::String (n);
        choice (t + "_shape", "LFO " + juce::String (n) + " Shape", { "Sine", "Triangle", "Saw", "Square", "Random" }, 0);
        flt (t + "_rate_hz", "LFO " + juce::String (n) + " Rate Hz", 0.05f, 20.0f, n == 1 ? 2.0f : 0.5f, 0.5f);
        choice (t + "_sync", "LFO " + juce::String (n) + " Sync", { "Free", "1/1", "1/2", "1/4", "1/8", "1/16" }, 0);
    }
    for (int n = 1; n <= 8; ++n)       // slots 5 to 8, the extra sources and targets, Curve and Range are newer; the order of the older choices never changes
    {
        const juce::String t = "mod" + juce::String (n);
        choice (t + "_src", "Mod " + juce::String (n) + " Source", { "None", "LFO 1", "LFO 2", "Velocity", "Mod Wheel", "Env 1", "Env 2", "XY X", "XY Y", "Macro 1", "Macro 2", "Key Track", "Random" }, 0);
        choice (t + "_dst", "Mod " + juce::String (n) + " Destination", { "None", "Cutoff", "Resonance", "Pitch", "Level", "Pulse Width", "Pan", "Texture Level", "Texture Position", "Sample Level" }, 0);
        flt (t + "_amt", "Mod " + juce::String (n) + " Amount", -1.0f, 1.0f, 0.0f);
        flt (t + "_curve", "Mod " + juce::String (n) + " Curve", -1.0f, 1.0f, 0.0f);
        choice (t + "_range", "Mod " + juce::String (n) + " Range", { "Full", "Positive", "Negative" }, 0);
    }
    toggle ("vib_on", "Vibrato On", false);
    flt ("vib_rate_hz", "Vibrato Rate Hz", 0.5f, 10.0f, 5.5f);
    flt ("vib_depth_ct", "Vibrato Depth Ct", 0.0f, 100.0f, 25.0f);
    toggle ("porta_on", "Portamento On", false);
    flt ("porta_ms", "Portamento Ms", 1.0f, 2000.0f, 120.0f, 0.4f);
    // Auto-panner
    toggle ("pan_on", "Pan On", false);
    flt ("pan_rate_hz", "Pan Rate Hz", 0.05f, 12.0f, 1.0f, 0.5f);
    flt ("pan_depth", "Pan Depth", 0.0f, 1.0f, 0.7f);
    choice ("pan_shape", "Pan Shape", { "Sine", "Triangle", "Square" }, 0);
    // Sampler
    flt ("synth_mix", "Synth Mix", 0.0f, 1.0f, 1.0f);               // 0 = all sample, 1 = all synth
    flt ("sample_gain", "Sample Gain", 0.0f, 2.0f, 1.0f);
    integer ("sample_root", "Sample Root", 0, 127, 60);
    integer ("sample_octave", "Sample Octave", -2, 2, 0);
    toggle ("sample_loop", "Sample Loop", false);
    toggle ("sample_reverse", "Sample Reverse", false);
    toggle ("sample_normalize", "Sample Normalize", true);
    flt ("sample_start", "Sample Start", 0.0f, 1.0f, 0.0f);
    flt ("sample_end", "Sample End", 0.0f, 1.0f, 1.0f);                         // playback end, independent of the loop end
    flt ("sample_tune_ct", "Sample Tune Ct", -100.0f, 100.0f, 0.0f);            // fine tuning of the sample, in cents
    flt ("sample_morph", "Sample Morph", 0.0f, 1.0f, 0.0f);                     // 0 = slot A, 1 = slot B
    // Slot B as an independent layer. Off by default: B then only morphs with A and uses A's settings, as before.
    // On, B has its own gain, root, tuning, start/end, loop and reverse, and the A/B slider balances the layers (both at full level in the middle).
    toggle ("sampleb_indep", "Sample B Independent", false);
    flt ("sampleb_gain", "Sample B Gain", 0.0f, 2.0f, 1.0f);
    integer ("sampleb_root", "Sample B Root", 0, 127, 60);
    integer ("sampleb_octave", "Sample B Octave", -2, 2, 0);
    flt ("sampleb_tune_ct", "Sample B Tune Ct", -100.0f, 100.0f, 0.0f);
    toggle ("sampleb_loop", "Sample B Loop", false);
    toggle ("sampleb_reverse", "Sample B Reverse", false);
    toggle ("sampleb_normalize", "Sample B Normalize", true);
    flt ("sampleb_start", "Sample B Start", 0.0f, 1.0f, 0.0f);
    flt ("sampleb_end", "Sample B End", 0.0f, 1.0f, 1.0f);
    flt ("sampleb_loop_start", "Sample B Loop Start", 0.0f, 1.0f, 0.0f);
    flt ("sampleb_loop_end", "Sample B Loop End", 0.0f, 1.0f, 1.0f);
    choice ("sampleb_xfade", "Sample B Xfade", { "Off", "5 ms", "10 ms", "20 ms", "50 ms", "100 ms", "200 ms", "500 ms" }, 0);
    // Own envelope for each sample layer. Off by default: the layer then follows the voice envelope only, as before. On, the layer is also shaped by its own
    // ADSR (the voice envelope still gates the whole voice, so keep its release at least as long as the longest layer release).
    toggle ("sample_env_own", "Sample Env Own", false);
    flt ("sample_attack_ms", "Sample Attack Ms", 0.0f, 5000.0f, 5.0f, 0.3f);
    flt ("sample_decay_ms", "Sample Decay Ms", 0.0f, 5000.0f, 200.0f, 0.3f);
    flt ("sample_sustain", "Sample Sustain", 0.0f, 1.0f, 0.8f);
    flt ("sample_release_ms", "Sample Release Ms", 0.0f, 5000.0f, 200.0f, 0.3f);
    toggle ("sampleb_env_own", "Sample B Env Own", false);
    flt ("sampleb_attack_ms", "Sample B Attack Ms", 0.0f, 5000.0f, 5.0f, 0.3f);
    flt ("sampleb_decay_ms", "Sample B Decay Ms", 0.0f, 5000.0f, 200.0f, 0.3f);
    flt ("sampleb_sustain", "Sample B Sustain", 0.0f, 1.0f, 0.8f);
    flt ("sampleb_release_ms", "Sample B Release Ms", 0.0f, 5000.0f, 200.0f, 0.3f);
    // Texture layer (granular cloud from the loaded sample)
    flt ("tex_level", "Texture Level", 0.0f, 1.0f, 0.0f);
    flt ("tex_pos", "Texture Position", 0.0f, 1.0f, 0.5f);
    flt ("tex_spread", "Texture Spread", 0.0f, 1.0f, 0.2f);
    flt ("tex_size_ms", "Texture Size Ms", 20.0f, 500.0f, 120.0f, 0.5f);
    flt ("tex_density", "Texture Density", 2.0f, 80.0f, 20.0f, 0.5f);
    flt ("tex_jitter_ct", "Texture Jitter Ct", 0.0f, 100.0f, 0.0f);
    // The texture's own file, pitch and envelope. Off by default: the grains then come from sample A and follow its root, as before.
    toggle ("tex_indep", "Texture Own File", false);
    integer ("tex_root", "Texture Root", 0, 127, 60);
    integer ("tex_octave", "Texture Octave", -2, 2, 0);
    flt ("tex_tune_ct", "Texture Tune Ct", -100.0f, 100.0f, 0.0f);
    toggle ("tex_normalize", "Texture Normalize", true);
    toggle ("tex_env_own", "Texture Env Own", false);
    flt ("tex_attack_ms", "Texture Attack Ms", 0.0f, 5000.0f, 5.0f, 0.3f);
    flt ("tex_decay_ms", "Texture Decay Ms", 0.0f, 5000.0f, 200.0f, 0.3f);
    flt ("tex_sustain", "Texture Sustain", 0.0f, 1.0f, 0.8f);
    flt ("tex_release_ms", "Texture Release Ms", 0.0f, 5000.0f, 200.0f, 0.3f);
    // Filtering and routing of the texture. Off / "With sample" by default: the texture is then filtered together with the samples, as before.
    toggle ("tex_filter_on", "Texture Filter On", false);
    choice ("tex_filter_type", "Texture Filter Type", { "LP", "HP", "BP", "Notch" }, 0);
    flt ("tex_cutoff_hz", "Texture Cutoff Hz", 20.0f, 20000.0f, 8000.0f, 0.3f);
    flt ("tex_reso", "Texture Reso", 0.0f, 1.0f, 0.1f);
    choice ("tex_route", "Texture Route", { "With sample", "Dry" }, 0);          // Dry: the texture skips the voice filter
    flt ("loop_start", "Loop Start", 0.0f, 1.0f, 0.0f);
    flt ("loop_end", "Loop End", 0.0f, 1.0f, 1.0f);
    choice ("loop_xfade", "Loop Xfade", { "Off", "5 ms", "10 ms", "20 ms", "50 ms", "100 ms", "200 ms", "500 ms" }, 0);
    toggle ("lfo1_keysync", "LFO 1 Key Sync", false);
    toggle ("lfo2_keysync", "LFO 2 Key Sync", false);
    // XY pad and macros (matrix sources "XY X", "XY Y", "Macro 1", "Macro 2")
    flt ("xy_x", "XY X", 0.0f, 1.0f, 0.5f); flt ("xy_y", "XY Y", 0.0f, 1.0f, 0.5f);
    flt ("macro_1", "Macro 1", 0.0f, 1.0f, 0.0f); flt ("macro_2", "Macro 2", 0.0f, 1.0f, 0.0f);
    // Keyboard split: below the split note plays the sample only, from the split note up plays the synth only
    toggle ("split_on", "Split On", false);
    integer ("split_note", "Split Note", 24, 96, 60);
    // Chop sampler
    toggle ("chop_on", "Chop On", false);
    choice ("chop_mode", "Chop Mode", { "Even", "Onset", "Grid" }, 0);
    integer ("chop_count", "Chop Count", 2, 16, 8);
    choice ("chop_grid", "Chop Grid", { "1/4", "1/8", "1/16", "1/32" }, 2);
    integer ("chop_pitch", "Chop Pitch", -12, 12, 0);
    // Chords
    toggle ("chord_on", "Chord On", false);
    choice ("chord_key", "Chord Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    choice ("chord_scale", "Chord Scale", { "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian", "Harmonic Minor", "Melodic Minor" }, 0);
    choice ("chord_type", "Chord Type", { "Triad", "Sus2", "Sus4", "7th", "9th", "Add9", "Power", "11th", "13th" }, 0);
    choice ("chord_mode", "Chord Mode", { "Scale", "Fixed", "Custom" }, 0);
    { static const int triad[7] = { 0x91, 0x89, 0x89, 0x91, 0x91, 0x89, 0x49 };       // Custom mode: one shape per scale degree, as a bit mask of semitones above the root
      for (int i = 1; i <= 7; ++i) integer ("chord_deg_" + juce::String (i) + "_shape", "Chord Degree " + juce::String (i) + " Shape", 0, (1 << 22) - 1, triad[i - 1]); }
    choice ("chord_shape", "Chord Shape", { "MAJ", "MIN", "MAJ7", "MIN7", "DOM7", "MAJ9", "MIN9", "DOM9", "MAJ11", "MIN11", "MAJ13", "SUS2", "SUS4", "ADD9", "6", "MIN6", "DIM", "DIM7", "AUG", "9SUS4", "13SUS4", "7#5", "7#5#9", "7b9" }, 2);
    toggle ("voice_lead", "Voice Lead", false);
    integer ("chord_inversion", "Chord Inversion", 0, 3, 0);
    choice ("chord_voicing", "Chord Voicing", { "Close", "Open", "Drop 2", "Spread" }, 0);
    toggle ("chord_lock", "Chord Lock", true);
    flt ("chord_strum_ms", "Chord Strum Ms", 0.0f, 200.0f, 0.0f);
    // Arpeggiator
    toggle ("arp_on", "Arp On", false);
    toggle ("arp_hold", "Arp Hold", false);
    toggle ("arp_run", "Arp Run", true);
    choice ("arp_div", "Arp Division", { "1/1", "1/2", "1/2D", "1/2T", "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T", "1/16", "1/16D", "1/16T", "1/32" }, 10);
    choice ("arp_order", "Arp Order", { "Pitch", "As Played", "Converge", "Diverge", "Random" }, 0);
    choice ("arp_mode", "Arp Mode", { "Up", "Down", "Up-Down", "Down-Up", "Random", "Played", "Chord", "Waterfall", "Custom" }, 0);
    choice ("arp_wshape", "Arp Waterfall Shape", { "Cascade", "Spiral", "Geyser", "Twin", "Tide", "Climb" }, 0);
    choice ("arp_grouping", "Arp Grouping", { "Pattern", "5-5-6", "7-7-2", "3-3-3-3-4", "4-4-4-4" }, 0);
    integer ("arp_octaves", "Arp Octaves", 1, 4, 1);
    flt ("arp_gate", "Arp Gate", 0.05f, 1.0f, 0.5f);
    flt ("arp_swing", "Arp Swing", 0.0f, 1.0f, 0.0f);
    flt ("arp_human", "Arp Human", 0.0f, 1.0f, 0.0f);
    toggle ("arp_two_hand", "Arp Two Hand", false);
    flt ("arp_chance", "Arp Chance", 0.0f, 1.0f, 1.0f);
    flt ("arp_spray", "Arp Spray", 0.0f, 1.0f, 0.0f);
    toggle ("arp_rhythm_on", "Arp Rhythm On", false);
    integer ("arp_rhythm_length", "Arp Rhythm Length", 1, 16, 16);
    for (int i = 1; i <= 16; ++i)
    {
        const auto id = "arp_rhythm_" + juce::String (i);
        choice (id + "_type", "Arp Rhythm " + juce::String (i) + " Type", { "Hit", "Rest", "Tie" }, 0);
        flt (id + "_velocity", "Arp Rhythm " + juce::String (i) + " Velocity", 0.0f, 1.0f, 1.0f);
    }
    integer ("arp_pat_length", "Arp Pattern Length", 1, 64, 16);
    for (int i = 1; i <= 64; ++i) integer ("arp_pat_" + juce::String (i), "Arp Pattern Step " + juce::String (i), -12, 12, (i - 1) % 8);   // Custom mode: note number of each step
    toggle ("wt_use_file", "Wavetable From File", false);   // on: oscillators set to Wavetable play the loaded file instead of the built-in bank
    // Master
    flt ("master", "Master", 0.0f, 2.0f, 0.8f);
    return { v.begin(), v.end() };
}

std::shared_ptr<SampleData> RelicProcessor::decodeSample (const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples < 2) { sampleError = "Couldn't read that file"; return nullptr; }
    if (r->lengthInSamples > (juce::int64) (r->sampleRate * 600.0)) { sampleError = "That sample is longer than 10 minutes"; return nullptr; }
    const int n = (int) r->lengthInSamples;
    juce::AudioBuffer<float> buf (juce::jmax (1, (int) r->numChannels), n);
    r->read (&buf, 0, n, 0, true, true);
    auto d = std::make_shared<SampleData>();
    d->L.assign (buf.getReadPointer (0), buf.getReadPointer (0) + n);
    d->R = buf.getNumChannels() > 1 ? std::vector<float> (buf.getReadPointer (1), buf.getReadPointer (1) + n) : d->L;
    d->rate = r->sampleRate; d->name = f.getFileNameWithoutExtension();
    float pk = 1.0e-6f; for (int i = 0; i < n; ++i) pk = std::max ({ pk, std::abs (d->L[(size_t) i]), std::abs (d->R[(size_t) i]) });
    d->peak = pk;
    d->peaksMin.assign (1024, 0.0f); d->peaksMax.assign (1024, 0.0f);
    for (int b = 0; b < 1024; ++b)
    {
        const int a = (int) ((juce::int64) n * b / 1024), z = std::max (a + 1, (int) ((juce::int64) n * (b + 1) / 1024));
        float lo = 1.0f, hi = -1.0f;
        for (int i = a; i < z && i < n; ++i) { const float v = 0.5f * (d->L[(size_t) i] + d->R[(size_t) i]); lo = std::min (lo, v); hi = std::max (hi, v); }
        d->peaksMin[(size_t) b] = lo; d->peaksMax[(size_t) b] = hi;
    }
    // onset detection: spectral-flux-like energy rise on 5 ms hops, strongest peaks first (>= 60 ms apart)
    {
        const int hop = std::max (16, (int) (d->rate * 0.005)); const int nb = n / hop;
        std::vector<float> env ((size_t) std::max (0, nb), 0.0f), flux ((size_t) std::max (0, nb), 0.0f);
        for (int b = 0; b < nb; ++b) { double e = 0; for (int i = b * hop; i < (b + 1) * hop; ++i) { const float v = 0.5f * (d->L[(size_t) i] + d->R[(size_t) i]); e += (double) v * v; } env[(size_t) b] = (float) std::sqrt (e / hop); }
        for (int b = 3; b < nb; ++b) { float prev = 0; for (int k = 1; k <= 3; ++k) prev += env[(size_t) (b - k)]; prev /= 3.0f; flux[(size_t) b] = std::max (0.0f, env[(size_t) b] - prev) / (prev + 0.02f * pk + 1e-6f); }
        std::vector<int> pk_;
        for (int b = 4; b + 1 < nb; ++b) if (flux[(size_t) b] > 0.5f && flux[(size_t) b] >= flux[(size_t) b - 1] && flux[(size_t) b] > flux[(size_t) b + 1]) pk_.push_back (b);
        std::sort (pk_.begin(), pk_.end(), [&] (int a, int b) { return flux[(size_t) a] > flux[(size_t) b]; });
        std::vector<int> kept; const int minGap = std::max (1, (int) (0.06 / 0.005));
        for (int b : pk_) { bool ok = true; for (int k : kept) if (std::abs (k - b) < minGap) { ok = false; break; } if (ok) kept.push_back (b); if (kept.size() >= 15) break; }
        // the onset is where the rise began: step back to the local minimum of the envelope (at most 3 hops)
        for (int b : kept) { int s0 = b; while (s0 > 0 && b - s0 < 3 && env[(size_t) (s0 - 1)] < env[(size_t) s0]) --s0; d->onsets.push_back ((float) (s0 * hop) / (float) n); }
    }
    return d;
}

bool RelicProcessor::loadSampleFile (const juce::File& f, bool setMixToSample, bool slotB)
{
    auto d = decodeSample (f); if (d == nullptr) return false;
    if (slotB)
    {
        { const juce::SpinLock::ScopedLockType l (sampleLock); sampleB = d; }
        sampleError = {}; apvts.state.setProperty ("samplePathB", f.getFullPathName(), nullptr); return true;
    }
    const bool first = (getSample() == nullptr);
    { const juce::SpinLock::ScopedLockType l (sampleLock); sample = d; }
    sampleError = {};
    apvts.state.setProperty ("samplePath", f.getFullPathName(), nullptr);
    if (first && setMixToSample) if (auto* p = apvts.getParameter ("synth_mix")) p->setValueNotifyingHost (0.0f);   // hear the sample straight away
    return true;
}

void RelicProcessor::clearSampleB()
{
    { const juce::SpinLock::ScopedLockType l (sampleLock); sampleB.reset(); }
    apvts.state.removeProperty ("samplePathB", nullptr);
}

bool RelicProcessor::loadTextureFile (const juce::File& f)
{
    auto d = decodeSample (f); if (d == nullptr) return false;
    { const juce::SpinLock::ScopedLockType l (sampleLock); sampleT = d; }
    sampleError = {}; apvts.state.setProperty ("samplePathT", f.getFullPathName(), nullptr); return true;
}
void RelicProcessor::clearTexture()
{
    { const juce::SpinLock::ScopedLockType l (sampleLock); sampleT.reset(); }
    apvts.state.removeProperty ("samplePathT", nullptr);
}

// ---- external wavetables ----
juce::File RelicProcessor::wavetablesRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Relic").getChildFile ("Wavetables");
}

void RelicProcessor::rebuildWavetable()
{
    std::shared_ptr<const WtRaw> raw; { const juce::SpinLock::ScopedLockType l (sampleLock); raw = wtRaw; }
    if (raw == nullptr) return;
    auto bank = rf::WaveTables::buildUser (raw->data, raw->frameLen, sr, raw->name);
    const juce::SpinLock::ScopedLockType l (sampleLock); wtUser = bank;
}

bool RelicProcessor::loadWavetableFile (const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples < 2) { sampleError = "Couldn't read that wavetable"; return false; }
    if (r->lengthInSamples > (1 << 24)) { sampleError = "That wavetable file is too long (over 16 million samples)"; return false; }
    const int n = (int) r->lengthInSamples;
    juce::AudioBuffer<float> buf (juce::jmax (1, (int) r->numChannels), n);
    r->read (&buf, 0, n, 0, true, true);
    auto raw = std::make_shared<WtRaw>(); raw->data.assign ((size_t) n, 0.0f);
    for (int ch = 0; ch < buf.getNumChannels(); ++ch) { const float* p = buf.getReadPointer (ch); for (int i = 0; i < n; ++i) raw->data[(size_t) i] += p[i] / (float) buf.getNumChannels(); }
    int hint = 0;
    if (f.hasFileExtension ("wav")) { juce::MemoryBlock mb; if (f.loadFileAsData (mb) && mb.getSize() > 0) hint = rf::WaveTables::parseFrameLenHint ((const unsigned char*) mb.getData(), mb.getSize()); }
    raw->frameLen = rf::WaveTables::detectFrameLen ((size_t) n, hint); raw->name = f.getFileNameWithoutExtension().toStdString();
    auto bank = rf::WaveTables::buildUser (raw->data, raw->frameLen, sr, raw->name);
    if (bank == nullptr) { sampleError = "That file has no usable wavetable frames"; return false; }
    { const juce::SpinLock::ScopedLockType l (sampleLock); wtRaw = raw; wtUser = bank; }
    sampleError = {}; apvts.state.setProperty ("wavetablePath", f.getFullPathName(), nullptr); return true;
}

void RelicProcessor::clearWavetable()
{
    { const juce::SpinLock::ScopedLockType l (sampleLock); wtRaw.reset(); wtUser.reset(); }
    apvts.state.removeProperty ("wavetablePath", nullptr);
}

std::vector<juce::File> RelicProcessor::scanWavetables (const juce::File& folder)
{
    std::vector<juce::File> out;
    if (! folder.isDirectory()) return out;
    for (const auto& e : juce::RangedDirectoryIterator (folder, true, "*.wav;*.aif;*.aiff;*.flac", juce::File::findFiles))
        if (! e.getFile().getFileName().startsWith (".")) out.push_back (e.getFile());
    std::sort (out.begin(), out.end(), [] (const juce::File& a, const juce::File& b) { return a.getFullPathName().compareNatural (b.getFullPathName()) < 0; });
    return out;
}

bool RelicProcessor::stepWavetable (int dir)
{
    const auto cur = getWavetableFile();
    const auto files = scanWavetables (cur.existsAsFile() ? cur.getParentDirectory() : wavetablesRoot());
    if (files.empty()) { sampleError = "No wavetable files in that folder"; return false; }
    int idx = -1; for (size_t i = 0; i < files.size(); ++i) if (files[i] == cur) { idx = (int) i; break; }
    const int n = (int) files.size();
    // try forward until one loads, so a file that cannot be read does not stop the stepping
    for (int tries = 0; tries < n; ++tries)
    {
        idx = idx < 0 ? (dir > 0 ? 0 : n - 1) : ((idx + dir) % n + n) % n;
        if (loadWavetableFile (files[(size_t) idx])) return true;
    }
    return false;
}

void RelicProcessor::clearSample()
{
    { const juce::SpinLock::ScopedLockType l (sampleLock); sample.reset(); }
    apvts.state.removeProperty ("samplePath", nullptr);
}


// ---------------------------------------------------------------- presets
juce::File RelicProcessor::presetsRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Relic").getChildFile ("Presets");
}

static void installFactoryPresets()
{
    // The stamp follows the bundled presets' contents, so adding or editing a factory preset reinstalls them.
    juce::int64 h = 1469598103934665603LL;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (! juce::String (BinaryData::originalFilenames[i]).endsWithIgnoreCase (".xml")) continue;
        int size = 0; const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        for (int k = 0; k < size; ++k) h = (h ^ (juce::uint8) data[k]) * 1099511628211LL;
    }
    const juce::String version = "v2-" + juce::String::toHexString (h);
    auto dir = RelicProcessor::presetsRoot().getChildFile ("Factory");
    auto stamp = dir.getChildFile (".version");
    if (stamp.existsAsFile() && stamp.loadFileAsString().trim() == version) return;
    dir.createDirectory();
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String orig = BinaryData::originalFilenames[i];
        if (! orig.endsWithIgnoreCase (".xml")) continue;
        int size = 0; const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        if (data == nullptr) continue;
        dir.getChildFile (orig.replaceCharacter ('_', ' ')).replaceWithData (data, (size_t) size);
    }
    stamp.replaceWithText (version);
}

// The category sits in the first attributes of the file, so only the start of it is read (the library can hold hundreds of presets).
static juce::String readCategory (const juce::File& f)
{
    juce::FileInputStream in (f); if (! in.openedOk()) return {};
    char buf[2048] = {}; const int n = in.read (buf, (int) sizeof (buf) - 1); if (n <= 0) return {};
    const auto head = juce::String::fromUTF8 (buf, n); const int stateAt = head.indexOf ("<STATE"); if (stateAt < 0) return {};
    const auto fromState = head.substring (stateAt); const int tagEnd = fromState.indexOfChar ('>');        // skips the <?xml ...?> line
    const auto tag = tagEnd > 0 ? fromState.substring (0, tagEnd) : fromState;
    const int k = tag.indexOf (" category=\""); if (k < 0) return {};
    const auto rest = tag.substring (k + 11);
    return rest.upToFirstOccurrenceOf ("\"", false, false).replace ("&amp;", "&").trim();
}

std::vector<PresetEntry> RelicProcessor::scanPresets() const
{
    std::vector<PresetEntry> out; const auto root = presetsRoot();
    for (const auto& f : root.findChildFiles (juce::File::findFiles, true, "*.xml"))
    {
        if (f.getFileName().startsWith (".")) continue;
        PresetEntry e; e.file = f; e.name = f.getFileNameWithoutExtension();
        e.bank = f.getParentDirectory().getRelativePathFrom (root).replaceCharacter ('\\', '/');
        e.category = readCategory (f);
        out.push_back (e);
    }
    auto rank = [] (const juce::String& b) { return b == "Factory" ? 0 : (b == "User" ? 1 : 2); };
    std::sort (out.begin(), out.end(), [&] (const PresetEntry& a, const PresetEntry& b)
    { if (rank (a.bank) != rank (b.bank)) return rank (a.bank) < rank (b.bank); if (a.bank != b.bank) return a.bank.compareNatural (b.bank) < 0; return a.name.compareNatural (b.name) < 0; });
    return out;
}

static void resetAllParameters (juce::AudioProcessor& proc)
{
    for (auto* prm : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->paramID != "bypass") { r->beginChangeGesture(); r->setValueNotifyingHost (r->getDefaultValue()); r->endChangeGesture(); }
}

bool RelicProcessor::loadPresetFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("STATE")) return false;
    resetAllParameters (*this);
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        if (auto* prm = apvts.getParameter (e->getStringAttribute ("id")))
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            { r->beginChangeGesture(); r->setValueNotifyingHost (r->convertTo0to1 ((float) e->getDoubleAttribute ("value"))); r->endChangeGesture(); }
    const auto samplePath = xml->getStringAttribute ("samplePath");
    juce::File sf (samplePath);
    if (samplePath.isNotEmpty() && sf.existsAsFile()) loadSampleFile (sf, false);
    else { clearSample(); if (samplePath.isNotEmpty()) sampleError = "Sample not found: " + sf.getFileName(); }
    { const auto pb = xml->getStringAttribute ("samplePathB"); juce::File fb (pb); if (pb.isNotEmpty() && fb.existsAsFile()) loadSampleFile (fb, false, true); else clearSampleB(); }
    { const auto pt = xml->getStringAttribute ("samplePathT"); juce::File ft (pt); if (pt.isNotEmpty() && ft.existsAsFile()) loadTextureFile (ft); else clearTexture(); }
    { const auto pw = xml->getStringAttribute ("wavetablePath"); juce::File fw (pw); if (pw.isNotEmpty() && fw.existsAsFile()) loadWavetableFile (fw); else { clearWavetable(); if (pw.isNotEmpty()) sampleError = "Wavetable not found: " + fw.getFileName(); } }
    apvts.state.setProperty ("presetName", xml->getStringAttribute ("presetName", f.getFileNameWithoutExtension()), nullptr);
    apvts.state.setProperty ("presetCategory", xml->getStringAttribute ("category"), nullptr);
    apvts.state.setProperty ("presetBank", f.getParentDirectory().getRelativePathFrom (presetsRoot()).replaceCharacter ('\\', '/'), nullptr);
    return true;
}

void RelicProcessor::initPreset()
{
    resetAllParameters (*this); clearSample(); clearSampleB(); clearTexture(); clearWavetable(); sampleError = {};
    apvts.state.setProperty ("presetName", "Init", nullptr); apvts.state.setProperty ("presetBank", "", nullptr); apvts.state.setProperty ("presetCategory", "", nullptr);
}

juce::File RelicProcessor::savePreset (const juce::String& name, const juce::String& category)
{
    auto dir = presetsRoot().getChildFile ("User"); dir.createDirectory();
    auto safe = juce::File::createLegalFileName (name.trim()); if (safe.isEmpty()) safe = "Untitled";
    auto file = dir.getChildFile (safe + ".xml");
    apvts.state.setProperty ("presetName", name.trim(), nullptr); apvts.state.setProperty ("presetBank", "User", nullptr); apvts.state.setProperty ("presetCategory", category, nullptr);
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("presetName", name.trim());
        if (category.isNotEmpty()) xml->setAttribute ("category", category); else xml->removeAttribute ("category");
        file.replaceWithText (xml->toString());
    }
    return file;
}

RelicProcessor::RelicProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < 16; ++i)
    {
        const auto id = "arp_rhythm_" + juce::String (i + 1);
        rhythmTypes[(size_t) i] = apvts.getRawParameterValue (id + "_type");
        rhythmVelocities[(size_t) i] = apvts.getRawParameterValue (id + "_velocity");
    }
    for (int i = 0; i < 7; ++i) degShapes[(size_t) i] = apvts.getRawParameterValue ("chord_deg_" + juce::String (i + 1) + "_shape");
    for (int i = 0; i < 64; ++i) patSteps[(size_t) i] = apvts.getRawParameterValue ("arp_pat_" + juce::String (i + 1));
    installFactoryPresets();
}

bool RelicProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RelicProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    tables.prepare (sr);
    rebuildWavetable();
    delay.prepare (sr); chorus.prepare (sr); flanger.prepare (sr); reverb.prepare (sr);
    pdelay.prepare (sr); talk.prepare (sr); sgate.prepare (sr); eq.prepare (sr); tape.prepare (sr); wah.prepare (sr); lofi.prepare (sr); ott.prepare (sr); leadHeld.reserve (64);
    for (auto& vo : voices) vo = Voice();
    sampleCounter = 0; outEvts.clear(); engine.reset (0, outEvts); outEvts.clear(); inEvts.reserve (256); outEvts.reserve (512);
}

float RelicProcessor::oscSample (int shape, double phase, double hz, float pw) const
{
    const int band = rf::WaveTables::bandFor (hz);
    switch (shape)
    {
        case 0: return (float) std::sin (2.0 * rf::kPi * phase);
        case 4: return curWt != nullptr ? tables.lookupUserMorph (*curWt, (pw - 0.05) / 0.9, band, phase) : tables.lookupMorph ((pw - 0.05) / 0.9, band, phase);
        case 1: return tables.lookup (rf::WaveTables::Saw, band, phase) * 0.847f;
        case 2:
        {
            double ph2 = phase + pw; if (ph2 >= 1.0) ph2 -= 1.0;
            const float d = tables.lookup (rf::WaveTables::Saw, band, phase) - tables.lookup (rf::WaveTables::Saw, band, ph2);
            return d / (2.0f * std::max (pw, 1.0f - pw)) * 0.707f;
        }
        default: return tables.lookup (rf::WaveTables::Triangle, band, phase) * 0.951f;
    }
}

rf::ChordArpSettings RelicProcessor::readChordArp() const
{
    rf::ChordArpSettings c;
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    c.chordOn = v ("chord_on") > 0.5f; c.key = (int) v ("chord_key"); c.scale = (int) v ("chord_scale"); c.type = (int) v ("chord_type");
    c.inversion = (int) v ("chord_inversion"); c.voicing = (int) v ("chord_voicing"); c.lock = v ("chord_lock") > 0.5f; c.strumMs = v ("chord_strum_ms");
    c.arpOn = v ("arp_on") > 0.5f; c.hold = v ("arp_hold") > 0.5f; c.rate = (int) v ("arp_div"); c.order = (int) v ("arp_order"); c.grouping = (int) v ("arp_grouping"); c.chordMode = (int) v ("chord_mode"); c.shape = (int) v ("chord_shape"); c.voiceLead = v ("voice_lead") > 0.5f; c.wshape = (int) v ("arp_wshape"); c.mode = (int) v ("arp_mode");
    c.run = v ("arp_run") > 0.5f; c.twoHand = v ("arp_two_hand") > 0.5f; c.chance = v ("arp_chance"); c.spray = v ("arp_spray");
    c.rhythmOn = v ("arp_rhythm_on") > 0.5f; c.rhythmLength = (int) v ("arp_rhythm_length");
    for (int i = 0; i < 16; ++i)
    {
        c.rhythm[(size_t) i].type = (int) rhythmTypes[(size_t) i]->load();
        c.rhythm[(size_t) i].velocity = rhythmVelocities[(size_t) i]->load();
    }
    for (int i = 0; i < 7; ++i) c.degShape[(size_t) i] = (int) degShapes[(size_t) i]->load();
    c.patLength = (int) v ("arp_pat_length"); for (int i = 0; i < 64; ++i) c.pattern[(size_t) i] = (int) patSteps[(size_t) i]->load();
    c.octaves = (int) v ("arp_octaves"); c.gate = v ("arp_gate"); c.swing = v ("arp_swing"); c.human = v ("arp_human");
    return c;
}

// Lead mode: one voice, last note wins, overlapping notes glide or jump without restarting the envelope.
void RelicProcessor::leadRetarget (int note, float vel)
{
    auto& v = voices[0];
    v.glide = p ("porta_on")->load() > 0.5f ? (float) v.note + v.glide - (float) note : 0.0f;
    v.note = note; v.baseHz = 440.0 * std::pow (2.0, (note - 69) / 12.0); v.velocity = vel; lastNote = note;
}

int RelicProcessor::chopSlices (const SampleData& s, double bpm, double* out) const
{
    const int mode = (int) p ("chop_mode")->load(); int count = juce::jlimit (2, 16, (int) p ("chop_count")->load());
    out[0] = 0.0;
    if (mode == 0) { for (int i = 1; i <= count; ++i) out[i] = (double) i / count; }
    else if (mode == 1)
    {
        double b[16]; int m = 0;
        for (size_t i = 0; i < s.onsets.size() && m < count - 1; ++i) if (s.onsets[i] > 0.002f) b[m++] = s.onsets[i];
        std::sort (b, b + m); for (int i = 0; i < m; ++i) out[i + 1] = b[i];
        count = m + 1; out[count] = 1.0;
    }
    else
    {
        static const double beats[4] = { 1.0, 0.5, 0.25, 0.125 };
        const double len = beats[juce::jlimit (0, 3, (int) p ("chop_grid")->load())] * 60.0 / std::max (20.0, bpm) * s.rate / (double) std::max (1, s.frames());
        int m = 0; while (m < count && (m + 1) * len < 1.0 - 1e-9) { ++m; out[m] = m * len; }
        count = m + 1; out[count] = 1.0;
    }
    return count;
}

void RelicProcessor::noteOn (int note, float vel)
{
    { bool any = false; for (auto& v : voices) if (v.active && v.env.stage != rf::Adsr::Release) { any = true; break; }
      if (! any) { if (p ("lfo1_keysync")->load() > 0.5f) lfo[0].reset(); if (p ("lfo2_keysync")->load() > 0.5f) lfo[1].reset(); } }
    if (p ("chop_on")->load() > 0.5f && audioSample != nullptr && audioSample->frames() > 1 && note >= kChopBase && note < kChopBase + 16)
    {
        double b[18]; const int cnt = chopSlices (*audioSample, hostBpm, b); const int pad = note - kChopBase;
        if (pad >= cnt) return;                                  // a pad with no slice stays silent
        {
            const int allowed = juce::jlimit (1, MaxVoices, (int) p ("voices")->load()); int idx = -1;
            for (int i = 0; i < allowed; ++i) if (voices[i].active && voices[i].note == note) { idx = i; break; }
            if (idx < 0) for (int i = 0; i < allowed; ++i) if (! voices[i].active) { idx = i; break; }
            if (idx < 0) { unsigned long long oldest = ~0ull; for (int i = 0; i < allowed; ++i) if (voices[i].age < oldest) { oldest = voices[i].age; idx = i; } }
            auto& vo = voices[idx]; const double nF = (double) audioSample->frames();
            vo.active = true; vo.note = note; vo.velocity = vel; vo.age = ++counter; vo.glide = 0.0f; vo.chop = true; vo.sampleActive = true; vo.sampleActiveB = false;
            vo.sliceA = std::floor (b[pad] * nF); vo.sliceB = std::min (nF - 1.0, std::floor (b[pad + 1] * nF));
            if (vo.sliceB < vo.sliceA + 2.0) vo.sliceB = std::min (nF - 1.0, vo.sliceA + 2.0);
            vo.samplePos = p ("sample_reverse")->load() > 0.5f ? vo.sliceB - 1.0 : vo.sliceA;
            vo.fl.reset(); vo.fr.reset(); vo.fl2.reset(); vo.fr2.reset(); vo.mpL = vo.mpR = 0.0f; vo.envA.level = vo.envB.level = 0.0f; vo.envA.noteOn(); vo.envB.noteOn(); vo.env.noteOn(); return;
        }
    }

    const bool lead = p ("lead_on")->load() > 0.5f;
    if (lead)
    {
        leadHeld.erase (std::remove (leadHeld.begin(), leadHeld.end(), note), leadHeld.end()); leadHeld.push_back (note);
        for (int i = 1; i < MaxVoices; ++i) if (voices[i].active && ! voices[i].chop && voices[i].env.stage != rf::Adsr::Release) voices[i].env.noteOff();
        if (voices[0].active && ! voices[0].chop && voices[0].env.stage != rf::Adsr::Release) { leadRetarget (note, vel); return; }
    }
    else leadHeld.clear();
    const int allowed = lead ? 1 : juce::jlimit (1, MaxVoices, (int) p ("voices")->load());
    int idx = -1;
    for (int i = 0; i < allowed; ++i) if (voices[i].active && voices[i].note == note) { idx = i; break; }
    if (idx < 0) for (int i = 0; i < allowed; ++i) if (! voices[i].active) { idx = i; break; }
    if (idx < 0) { unsigned long long oldest = ~0ull; for (int i = 0; i < allowed; ++i) if (voices[i].age < oldest) { oldest = voices[i].age; idx = i; } }
    auto& vo = voices[idx];
    vo.zone = p ("split_on")->load() > 0.5f ? (note < (int) p ("split_note")->load() ? 1 : 2) : 0;
    vo.active = true; vo.chop = false; vo.note = note; vo.velocity = vel; vo.age = ++counter;
    vo.glide = (p ("porta_on")->load() > 0.5f && lastNote >= 0) ? (float) (lastNote - note) : 0.0f; lastNote = note;
    vo.baseHz = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    unsigned int seed = (unsigned int) (note * 2654435761u + counter * 40503u);
    const bool random = p ("unison_voices")->load() > 1.5f;
    for (int u = 0; u < 7; ++u) { seed = seed * 1664525u + 1013904223u; vo.phase[u] = random ? (seed >> 8) / 16777216.0 : 0.0; }
    vo.phaseX[0] = vo.phaseX[1] = 0.0;
    vo.fl.reset(); vo.fr.reset(); vo.fl2.reset(); vo.fr2.reset(); vo.mpL = vo.mpR = 0.0f;
    if (audioSample != nullptr && audioSample->frames() > 1)
    {
        const double n = (double) audioSample->frames(); const auto rg = sampleRegion (n, "sample_start", "sample_end");
        vo.samplePos = p ("sample_reverse")->load() > 0.5f ? rg.endF : std::min ((rg.trimmed ? rg.endF : n) - 2.0, rg.startRaw);
        vo.sampleActive = true;
    }
    else vo.sampleActive = false;
    if (p ("sampleb_indep")->load() > 0.5f && audioSampleB != nullptr && audioSampleB->frames() > 1)
    {
        const double nb = (double) audioSampleB->frames(); const auto rb = sampleRegion (nb, "sampleb_start", "sampleb_end");
        vo.samplePosB = p ("sampleb_reverse")->load() > 0.5f ? rb.endF : std::min ((rb.trimmed ? rb.endF : nb) - 2.0, rb.startRaw);
        vo.sampleActiveB = true;
    }
    else vo.sampleActiveB = false;
    for (auto& g : vo.grains) g.on = false;
    vo.grainTimer = 0.0f; vo.gseed = (unsigned int) (counter * 2654435761u + (unsigned int) note * 40503u + 1u);
    vo.modRand = (float) (((vo.gseed * 1664525u + 1013904223u) >> 8) & 0xFFFFFFu) / 8388608.0f - 1.0f;   // the Random source: one value per note, -1 to 1
    { vo.nseed = vo.gseed * 1664525u + 1013904223u; const double D = std::min (8000.0, sr / vo.baseHz); vo.ksLp = 0.0f; vo.ksPos = 0;
      for (int i = 0; i < 8192; ++i) { vo.nseed = vo.nseed * 1664525u + 1013904223u; vo.ks[i] = i >= 8192 - (int) D ? ((vo.nseed >> 8) / 8388608.0f - 1.0f) * 0.7f : 0.0f; }   // the burst sits just behind the write position
      for (int k = 0; k < 6; ++k) { vo.bellAmp[k] = 1.0f; vo.phase[k] = 0.0; } }
    vo.menv[0].level = vo.menv[1].level = 0.0f; vo.menv[0].noteOn(); vo.menv[1].noteOn();
    vo.envA.level = vo.envB.level = 0.0f; vo.envA.noteOn(); vo.envB.noteOn();
    vo.envT.level = 0.0f; vo.envT.noteOn(); vo.ftl.reset(); vo.ftr.reset();
    vo.env.noteOn();
}

void RelicProcessor::noteOff (int note)
{
    bool anyChop = false;
    for (auto& vo : voices) if (vo.active && vo.chop && vo.note == note) { anyChop = true; if (vo.env.stage != rf::Adsr::Release) vo.env.noteOff(); }
    if (anyChop) return;
    if (p ("lead_on")->load() > 0.5f)
    {
        leadHeld.erase (std::remove (leadHeld.begin(), leadHeld.end(), note), leadHeld.end());
        auto& v = voices[0];
        if (! v.active || v.note != note) return;
        if (! leadHeld.empty()) { leadRetarget (leadHeld.back(), v.velocity); return; }
        if (v.env.stage != rf::Adsr::Release) v.env.noteOff();
        return;
    }
    for (auto& vo : voices) if (vo.active && vo.note == note && vo.env.stage != rf::Adsr::Release) vo.env.noteOff();
}

RelicProcessor::SampleRegion RelicProcessor::sampleRegion (double nF, const char* startId, const char* endId) const
{
    SampleRegion r; r.startRaw = (double) p (startId)->load() * nF; r.startF = std::floor (r.startRaw);
    r.trimmed = p (endId)->load() < 0.9999f;
    r.endF = r.trimmed ? std::min (nF - 1.0, std::max (r.startF + 2.0, std::floor ((double) p (endId)->load() * nF))) : nF - 1.0;
    r.revFloor = r.trimmed ? r.startF : 0.0;
    return r;
}

void RelicProcessor::renderSegment (float* L, float* R, int n)
{
    if (n <= 0) return;
    constexpr int kChunk = 512;
    if (n > kChunk) { renderSegment (L, R, kChunk); renderSegment (L + kChunk, R + kChunk, n - kChunk); return; }
    const int shape = (int) p ("waveform")->load();
    const float pw0 = p ("pulse_width")->load(), level = p ("osc_level")->load();
    const int uni = juce::jlimit (1, 7, (int) p ("unison_voices")->load());
    const float detune = p ("unison_detune")->load(), spread = p ("unison_spread")->load();
    const float aMs = p ("attack_ms")->load(), dMs = p ("decay_ms")->load(), sus = p ("sustain")->load(), rMs = p ("release_ms")->load();
    const bool fOn = p ("filter_on")->load() > 0.5f;
    const int fType = (int) p ("filter_type")->load(), fModel = (int) p ("filter_model")->load(), fSrc = (int) p ("filter_source")->load();
    const float mpA = std::exp (-2.0f * (float) rf::kPi * 11000.0f / (float) sr);
    const float cutoff = p ("cutoff_hz")->load(), reso0 = p ("resonance")->load(), q = rf::resonanceToQ (reso0);
    const float norm = 1.0f / std::sqrt ((float) uni);
    const auto* smp = audioSample.get();
    const bool haveS = smp != nullptr && smp->frames() > 1;
    const auto* smpB = audioSampleB.get(); const float morph = p ("sample_morph")->load();
    const bool indepSel = p ("sampleb_indep")->load() > 0.5f;
    const bool indepB = indepSel && smpB != nullptr && smpB->frames() > 1;           // slot B plays as its own layer
    const float mix = p ("synth_mix")->load();
    const auto* smpTx = audioSampleT.get(); const bool indepT = p ("tex_indep")->load() > 0.5f && smpTx != nullptr && smpTx->frames() > 1;   // the texture plays from its own file
    const float synthGain = (haveS || indepB || indepT) ? mix : 1.0f;
    float sampleGainFull = 0.0f;
    float sampleGain = 0.0f, rootSemis = 0.0f; double nF = 0, loopA = 0, loopB = 0; bool loopOn = false, rev = false; double rateRatio = 1.0, xfF = 0.0; float chopGain = 0.0f, chopRate = 1.0f;
    SampleRegion rgA; double tuneMulA = 1.0;
    if (haveS)
    {
        nF = (double) smp->frames();
        sampleGainFull = p ("sample_gain")->load() * (p ("sample_normalize")->load() > 0.5f ? 1.0f / smp->peak : 1.0f); sampleGain = (1.0f - mix) * sampleGainFull;
        rootSemis = (float) ((int) p ("sample_root")->load() - 12 * (int) p ("sample_octave")->load());
        loopOn = p ("sample_loop")->load() > 0.5f; rev = p ("sample_reverse")->load() > 0.5f;
        rgA = sampleRegion (nF, "sample_start", "sample_end"); tuneMulA = std::exp2 ((double) p ("sample_tune_ct")->load() / 1200.0);
        loopA = std::floor ((double) p ("loop_start")->load() * nF);
        if (rgA.trimmed && loopA > rgA.endF - 2.0) loopA = rgA.startF;                      // a loop that starts after the End point falls back to the whole played region
        loopB = std::max (loopA + 2.0, std::floor ((double) p ("loop_end")->load() * nF));
        loopB = std::min (loopB, rgA.endF);
        rateRatio = smp->rate / sr;
        { static const double xms[8] = { 0, 5, 10, 20, 50, 100, 200, 500 }; xfF = xms[juce::jlimit (0, 7, (int) p ("loop_xfade")->load())] * 0.001 * smp->rate; }
        chopGain = p ("sample_gain")->load() * (p ("sample_normalize")->load() > 0.5f ? 1.0f / smp->peak : 1.0f);
        chopRate = std::exp2 ((float) (int) p ("chop_pitch")->load() / 12.0f);
    }
    auto cubic = [] (const std::vector<float>& c, double pos, int n) -> float
    {
        const int i = (int) pos; const float f = (float) (pos - i);
        auto at = [&] (int k) { return c[(size_t) juce::jlimit (0, n - 1, k)]; };
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        return y1 + 0.5f * f * (y2 - y0 + f * (2.f * y0 - 5.f * y1 + 4.f * y2 - y3 + f * (3.f * (y1 - y2) + y3 - y0)));
    };

    const bool haveB = haveS && ! indepSel && smpB != nullptr && smpB->frames() > 1 && morph > 0.0f;     // the classic A/B morph
    const double rB = haveB ? smpB->rate / smp->rate : 1.0; const float gB = haveB ? (p ("sample_normalize")->load() > 0.5f ? smp->peak / smpB->peak : 1.0f) : 1.0f;
    // Independent slot B: its own region, loop, tuning and gain. In this mode the A/B slider balances the two layers, both at full level in the middle.
    const float wA = indepB && haveS ? std::min (1.0f, 2.0f * (1.0f - morph)) : 1.0f, wB = indepB ? (haveS ? std::min (1.0f, 2.0f * morph) : 1.0f) : 0.0f;
    double nB = 0, loopAB = 0, loopBB = 0, rateB = 1.0, xfBF = 0.0, tuneMulB = 1.0; bool loopOnB = false, revB = false; SampleRegion rgB; float gainBFull = 0.0f, gainBMix = 0.0f, rootB = 0.0f;
    if (indepB)
    {
        nB = (double) smpB->frames(); rgB = sampleRegion (nB, "sampleb_start", "sampleb_end");
        gainBFull = p ("sampleb_gain")->load() * (p ("sampleb_normalize")->load() > 0.5f ? 1.0f / smpB->peak : 1.0f); gainBMix = (1.0f - mix) * gainBFull;
        rootB = (float) ((int) p ("sampleb_root")->load() - 12 * (int) p ("sampleb_octave")->load());
        loopOnB = p ("sampleb_loop")->load() > 0.5f; revB = p ("sampleb_reverse")->load() > 0.5f;
        loopAB = std::floor ((double) p ("sampleb_loop_start")->load() * nB);
        if (rgB.trimmed && loopAB > rgB.endF - 2.0) loopAB = rgB.startF;
        loopBB = std::max (loopAB + 2.0, std::floor ((double) p ("sampleb_loop_end")->load() * nB)); loopBB = std::min (loopBB, rgB.endF);
        rateB = smpB->rate / sr; tuneMulB = std::exp2 ((double) p ("sampleb_tune_ct")->load() / 1200.0);
        { static const double xms[8] = { 0, 5, 10, 20, 50, 100, 200, 500 }; xfBF = xms[juce::jlimit (0, 7, (int) p ("sampleb_xfade")->load())] * 0.001 * smpB->rate; }
    }
    const bool ownA = p ("sample_env_own")->load() > 0.5f, ownB = indepB && p ("sampleb_env_own")->load() > 0.5f;
    const float envAp[4] = { p ("sample_attack_ms")->load(), p ("sample_decay_ms")->load(), p ("sample_sustain")->load(), p ("sample_release_ms")->load() };
    const float envBp[4] = { p ("sampleb_attack_ms")->load(), p ("sampleb_decay_ms")->load(), p ("sampleb_sustain")->load(), p ("sampleb_release_ms")->load() };
    // The texture's grains come from sample A (and follow its root) unless the texture has its own file.
    const bool texIndep = p ("tex_indep")->load() > 0.5f; const auto* smpT = audioSampleT.get();
    const SampleData* tsmp = texIndep ? smpT : smp; const bool tsOk = tsmp != nullptr && tsmp->frames() > 1;
    const float texLevel = p ("tex_level")->load(); const bool texOn = tsOk && texLevel > 0.0f;
    const double nTex = tsOk ? (double) tsmp->frames() : 0.0, texRate = tsOk ? tsmp->rate / sr : 1.0;
    const float texRoot = texIndep ? (float) ((int) p ("tex_root")->load() - 12 * (int) p ("tex_octave")->load()) : rootSemis;
    const double texTuneMul = texIndep ? std::exp2 ((double) p ("tex_tune_ct")->load() / 1200.0) : 1.0;
    const bool texNorm = (texIndep ? p ("tex_normalize")->load() : p ("sample_normalize")->load()) > 0.5f;
    const bool ownT = texIndep && p ("tex_env_own")->load() > 0.5f;
    const float envTp[4] = { p ("tex_attack_ms")->load(), p ("tex_decay_ms")->load(), p ("tex_sustain")->load(), p ("tex_release_ms")->load() };
    const bool texFiltOn = p ("tex_filter_on")->load() > 0.5f, texDry = (int) p ("tex_route")->load() == 1; const int texFiltType = (int) p ("tex_filter_type")->load();
    const bool texSep = texFiltOn || texDry;     // the texture is mixed separately only when it is filtered or routed dry
    const float texCut = p ("tex_cutoff_hz")->load(), texQ = rf::resonanceToQ (p ("tex_reso")->load());
    const float texPos = p ("tex_pos")->load(), texSpread = p ("tex_spread")->load(), texSize = p ("tex_size_ms")->load(), texDens = p ("tex_density")->load(), texJit = p ("tex_jitter_ct")->load();
    const float texGain = texLevel * (texNorm && tsOk ? 1.0f / tsmp->peak : 1.0f) * 0.7f / std::sqrt (std::max (1.0f, texDens * texSize * 0.001f));
    const float pwN = (pw0 - 0.05f) / 0.9f;
    float bellDk[6]; for (int k = 0; k < 6; ++k) bellDk[k] = std::exp (-1.0f / (((0.4f + 5.0f * pwN) / (1.0f + 1.2f * (float) k)) * (float) sr));
    const float normEff = shape >= 5 ? 1.0f : norm;

    // ---- modulation: LFOs for this chunk, then the active matrix slots ----
    struct Slot { int src, dst, range; float amt, curveP; } slots[8]; int nSlots = 0; bool fMod = false;
    for (int k = 0; k < 8; ++k)
    {
        const juce::String t = "mod" + juce::String (k + 1);
        const int sc = (int) p ((t + "_src").toRawUTF8())->load(), ds = (int) p ((t + "_dst").toRawUTF8())->load(); const float am = p ((t + "_amt").toRawUTF8())->load();
        if (sc > 0 && ds > 0 && am != 0.0f)
        {
            const float cv = p ((t + "_curve").toRawUTF8())->load();       // bend: above 0 starts slowly, below 0 starts fast
            slots[nSlots++] = { sc, ds, (int) p ((t + "_range").toRawUTF8())->load(), am, cv == 0.0f ? 1.0f : std::exp2 (2.0f * cv) }; fMod |= (ds == 1 || ds == 2);
        }
    }
    const float ctl[4] = { p ("xy_x")->load(), p ("xy_y")->load(), p ("macro_1")->load(), p ("macro_2")->load() };
    bool useEnv[2] = { false, false };
    for (int k = 0; k < nSlots; ++k) { if (slots[k].src == 5) useEnv[0] = true; if (slots[k].src == 6) useEnv[1] = true; }
    float me[2][4];   // attack, decay, sustain, release of each modulation envelope
    for (int k = 0; k < 2; ++k) { const juce::String s = "menv" + juce::String (k + 1); me[k][0] = p ((s + "_attack_ms").toRawUTF8())->load(); me[k][1] = p ((s + "_decay_ms").toRawUTF8())->load(); me[k][2] = p ((s + "_sustain").toRawUTF8())->load(); me[k][3] = p ((s + "_release_ms").toRawUTF8())->load(); }
    float lfoBuf[2][kChunk], vibBuf[kChunk];
    {
        const int sh1 = (int) p ("lfo1_shape")->load(), sh2 = (int) p ("lfo2_shape")->load();
        auto hzOf = [&] (const char* rate, const char* sync) { const double b = rf::lfoSyncBeats ((int) p (sync)->load()); return b > 0.0 ? hostBpm / 60.0 / b : (double) p (rate)->load(); };
        const double h1 = hzOf ("lfo1_rate_hz", "lfo1_sync"), h2 = hzOf ("lfo2_rate_hz", "lfo2_sync"), hv = p ("vib_rate_hz")->load();
        for (int i = 0; i < n; ++i) { lfoBuf[0][i] = lfo[0].next (h1, sr, sh1); lfoBuf[1][i] = lfo[1].next (h2, sr, sh2); vibBuf[i] = vibLfo.next (hv, sr, rf::ModLfo::Sine); }
    }
    const float wheel = modWheel.load(), bendSemis = pitchBend.load() * 2.0f;
    const bool vibOn = p ("vib_on")->load() > 0.5f; const float vibSemis = p ("vib_depth_ct")->load() * 0.01f;
    const bool portaOn = p ("porta_on")->load() > 0.5f; const float glideCoef = std::exp (-1.0f / (std::max (1.0f, p ("porta_ms")->load()) * 0.001f * (float) sr));

    struct Xo { bool on; int shape; float level; double mult; } xo[2];
    for (int k = 0; k < 2; ++k)
    {
        const juce::String s = k == 0 ? "osc2" : "osc3";
        xo[k].on = p ((s + "_on").toRawUTF8())->load() > 0.5f; xo[k].shape = (int) p ((s + "_wave").toRawUTF8())->load(); xo[k].level = p ((s + "_level").toRawUTF8())->load();
        xo[k].mult = std::pow (2.0, p ((s + "_oct").toRawUTF8())->load() + p ((s + "_semi").toRawUTF8())->load() / 12.0 + p ((s + "_fine").toRawUTF8())->load() / 1200.0);
    }
    float gl[7], gr[7], ratio[7];
    for (int u = 0; u < uni; ++u)
    {
        const float pos = uni == 1 ? 0.0f : ((float) u / (float) (uni - 1)) * 2.0f - 1.0f;   // -1..+1
        ratio[u] = std::pow (2.0f, pos * detune / 1200.0f);
        const float a = (pos * spread + 1.0f) * (float) (rf::kPi / 4.0);
        gl[u] = std::cos (a) * 1.41421356f; gr[u] = std::sin (a) * 1.41421356f;
    }

    for (auto& vo : voices)
    {
        if (! vo.active) continue;
        const float vSynth = vo.zone == 1 ? 0.0f : (vo.zone == 2 ? 1.0f : synthGain), vSamp = vo.zone == 1 ? sampleGainFull : (vo.zone == 2 ? 0.0f : sampleGain);   // keyboard split
        const float vSampB = vo.zone == 1 ? gainBFull : (vo.zone == 2 ? 0.0f : gainBMix);
        auto setupF = [&] (float c_, float q_) { vo.fl.setup (c_, q_, sr); vo.fr.setup (c_, q_, sr); if (fModel == 2) { vo.fl2.setup (c_, q_, sr); vo.fr2.setup (c_, q_, sr); } };
        if (fOn) setupF (cutoff, q);
        if (texFiltOn) { vo.ftl.setup (texCut, texQ, sr); vo.ftr.setup (texCut, texQ, sr); }
        for (int i = 0; i < n; ++i)
        {
            float mc = 0, mr = 0, mp = 0, ml = 0, mw = 0, mpn = 0, mtl = 0, mtp = 0, msl = 0, ev[2] = { 0.0f, 0.0f };
            for (int k = 0; k < 2; ++k) if (useEnv[k])
            {
                if (vo.env.stage == rf::Adsr::Release && vo.menv[k].stage != rf::Adsr::Release && vo.menv[k].stage != rf::Adsr::Idle) vo.menv[k].noteOff();
                ev[k] = vo.menv[k].next (me[k][0], me[k][1], me[k][2], me[k][3], sr);
            }
            float ea = 1.0f, eb = 1.0f;       // own envelopes of the sample layers (1 when off)
            if (ownA) { if (vo.env.stage == rf::Adsr::Release && vo.envA.stage != rf::Adsr::Release && vo.envA.stage != rf::Adsr::Idle) vo.envA.noteOff(); ea = vo.envA.next (envAp[0], envAp[1], envAp[2], envAp[3], sr); }
            if (ownB) { if (vo.env.stage == rf::Adsr::Release && vo.envB.stage != rf::Adsr::Release && vo.envB.stage != rf::Adsr::Idle) vo.envB.noteOff(); eb = vo.envB.next (envBp[0], envBp[1], envBp[2], envBp[3], sr); }
            float et = 1.0f;
            if (ownT) { if (vo.env.stage == rf::Adsr::Release && vo.envT.stage != rf::Adsr::Release && vo.envT.stage != rf::Adsr::Idle) vo.envT.noteOff(); et = vo.envT.next (envTp[0], envTp[1], envTp[2], envTp[3], sr); }
            for (int k = 0; k < nSlots; ++k)
            {
                const int sc = slots[k].src;
                float x = sc == 1 ? lfoBuf[0][i] : sc == 2 ? lfoBuf[1][i] : sc == 3 ? vo.velocity : sc == 4 ? wheel : sc <= 6 ? ev[sc - 5] : sc <= 10 ? ctl[sc - 7]
                        : sc == 11 ? juce::jlimit (-1.0f, 1.0f, ((float) vo.note - 60.0f) / 24.0f) : vo.modRand;
                if (slots[k].range == 1) x = std::max (0.0f, x); else if (slots[k].range == 2) x = std::min (0.0f, x);
                if (slots[k].curveP != 1.0f) x = std::copysign (std::pow (std::abs (x), slots[k].curveP), x);
                const float v = slots[k].amt * x;
                switch (slots[k].dst) { case 1: mc += v; break; case 2: mr += v; break; case 3: mp += v; break; case 4: ml += v; break; case 5: mw += v; break;
                                        case 6: mpn += v; break; case 7: mtl += v; break; case 8: mtp += v; break; default: msl += v; break; }
            }
            if (fOn && fMod && (i & 7) == 0)
            {
                const float c = juce::jlimit (20.0f, 20000.0f, cutoff * std::exp2 (5.0f * mc)), qq = rf::resonanceToQ (juce::jlimit (0.0f, 1.0f, reso0 + 0.5f * mr));
                setupF (c, qq);
            }
            float semis = mp * 12.0f + bendSemis + (vibOn ? vibBuf[i] * vibSemis : 0.0f);
            if (vo.glide != 0.0f) { semis += vo.glide; vo.glide *= glideCoef; if (std::abs (vo.glide) < 1e-3f) vo.glide = 0.0f; }
            const double pf = semis != 0.0f ? (double) std::exp2 (semis / 12.0f) : 1.0;
            const float pw = juce::jlimit (0.05f, 0.95f, pw0 + 0.45f * mw), lvl = juce::jlimit (0.0f, 2.0f, 1.0f + ml);
            float l = 0.0f, r = 0.0f, pl = 0.0f, pr = 0.0f;       // pl/pr: the sample's share, kept apart so the filter can treat it separately
            float tl = 0.0f, tr = 0.0f, dtl = 0.0f, dtr = 0.0f;   // the texture when it is filtered or routed dry (tl/tr), and its dry share (dtl/dtr)
            if (shape >= 5)
            {
                const double hz = vo.baseHz * pf; float s = 0.0f;
                if (shape == 5)          // plucked string: a noise burst circulating in a damped delay line
                {
                    const float b = 0.02f + 0.6f * (1.0f - (pw - 0.05f) / 0.9f);
                    const double D = std::min (8000.0, std::max (2.0, sr / hz - 0.5 - (double) (b / (1.0f - b))));
                    double rp = (double) vo.ksPos - D; if (rp < 0.0) rp += 8192.0;
                    const int i0 = (int) rp & 8191, i1 = (i0 + 1) & 8191; const float fr = (float) (rp - std::floor (rp));
                    const float y = vo.ks[i0] * (1.0f - fr) + vo.ks[i1] * fr;
                    vo.ksLp = y * (1.0f - b) + vo.ksLp * b; vo.ks[vo.ksPos] = vo.ksLp * 0.9985f; vo.ksPos = (vo.ksPos + 1) & 8191;
                    s = y * 2.4f;     // the string loses level quickly, so it needs more gain than the other sources
                }
                else                     // bell: six decaying inharmonic partials
                {
                    static const float rat[6] = { 1.0f, 2.76f, 5.4f, 8.93f, 13.34f, 18.64f }, wg[6] = { 1.0f, 0.6f, 0.4f, 0.25f, 0.15f, 0.1f };
                    for (int k = 0; k < 6; ++k)
                    {
                        const double fk = hz * rat[k]; if (fk > 0.45 * sr) continue;
                        s += (float) std::sin (2.0 * rf::kPi * vo.phase[k]) * wg[k] * vo.bellAmp[k];
                        vo.phase[k] += fk / sr; vo.phase[k] -= std::floor (vo.phase[k]); vo.bellAmp[k] *= bellDk[k];
                    }
                    s *= 0.35f;
                }
                l += s; r += s;
            }
            else
            for (int u = 0; u < uni; ++u)
            {
                const double hz = vo.baseHz * ratio[u] * pf;
                const float s = oscSample (shape, vo.phase[u], hz, pw);
                vo.phase[u] += hz / sr; vo.phase[u] -= std::floor (vo.phase[u]);
                l += s * gl[u]; r += s * gr[u];
            }
            l *= normEff * level * lvl * vo.velocity * vSynth; r *= normEff * level * lvl * vo.velocity * vSynth;
            for (int k = 0; k < 2; ++k) if (xo[k].on)
            {
                const double hz = vo.baseHz * xo[k].mult * pf;
                const float s = oscSample (xo[k].shape, vo.phaseX[k], hz, pw) * xo[k].level * level * lvl * vo.velocity * vSynth;
                vo.phaseX[k] += hz / sr; vo.phaseX[k] -= std::floor (vo.phaseX[k]);
                l += s; r += s;
            }
            if (vo.chop) { l = r = 0.0f; }
            if (haveS && vo.sampleActive && vo.chop)
            {
                const int nn = smp->frames();
                const float g = chopGain * vo.velocity * lvl * ea;
                float cl = cubic (smp->L, vo.samplePos, nn), cr = cubic (smp->R, vo.samplePos, nn);
                const double cxf = loopOn && ! rev ? std::min (xfF, 0.5 * (vo.sliceB - vo.sliceA)) : 0.0;
                if (cxf > 0.0 && vo.samplePos >= vo.sliceB - cxf)
                {
                    const double u = (vo.samplePos - (vo.sliceB - cxf)) / cxf, p2 = vo.sliceA + u * cxf; const float gi = (float) std::sin (u * 1.5707963), go = (float) std::cos (u * 1.5707963);
                    cl = cl * go + cubic (smp->L, p2, nn) * gi; cr = cr * go + cubic (smp->R, p2, nn) * gi;
                }
                pl += cl * g; pr += cr * g;
                const double step = rateRatio * chopRate * pf;
                if (! rev)
                {
                    vo.samplePos += step;
                    if (vo.samplePos >= vo.sliceB) { if (loopOn) vo.samplePos = vo.sliceA + cxf + std::fmod (vo.samplePos - vo.sliceB, std::max (1.0, vo.sliceB - vo.sliceA - cxf)); else { vo.sampleActive = false; if (vo.env.stage != rf::Adsr::Release) vo.env.noteOff(); } }
                }
                else
                {
                    vo.samplePos -= step;
                    if (vo.samplePos <= vo.sliceA) { if (loopOn) vo.samplePos = vo.sliceB - std::fmod (vo.sliceA - vo.samplePos, vo.sliceB - vo.sliceA); else { vo.sampleActive = false; if (vo.env.stage != rf::Adsr::Release) vo.env.noteOff(); } }
                }
            }
            else if (haveS && vo.sampleActive)
            {
                const int nn = smp->frames();
                float sl = cubic (smp->L, vo.samplePos, nn), sr_ = cubic (smp->R, vo.samplePos, nn);
                const double lxf = loopOn && ! rev ? std::min (xfF, 0.5 * (loopB - loopA)) : 0.0;
                if (lxf > 0.0 && vo.samplePos >= loopB - lxf)
                {
                    const double u = (vo.samplePos - (loopB - lxf)) / lxf, p2 = loopA + u * lxf; const float gi = (float) std::sin (u * 1.5707963), go = (float) std::cos (u * 1.5707963);
                    sl = sl * go + cubic (smp->L, p2, nn) * gi; sr_ = sr_ * go + cubic (smp->R, p2, nn) * gi;
                }
                if (haveB)
                {
                    const double pb = vo.samplePos * rB; const int nb = smpB->frames();
                    float bl = 0.0f, br = 0.0f; if (pb >= 0.0 && pb < (double) nb - 1.0) { bl = cubic (smpB->L, pb, nb) * gB; br = cubic (smpB->R, pb, nb) * gB; }
                    sl = sl * (1.0f - morph) + bl * morph; sr_ = sr_ * (1.0f - morph) + br * morph;
                }
                pl += sl * vSamp * vo.velocity * wA * ea; pr += sr_ * vSamp * vo.velocity * wA * ea;
                const double step = std::pow (2.0, ((double) vo.note - rootSemis) / 12.0) * rateRatio * pf * tuneMulA;
                if (! rev)
                {
                    vo.samplePos += step;
                    if (loopOn) { if (vo.samplePos >= loopB) vo.samplePos = loopA + lxf + std::fmod (vo.samplePos - loopB, std::max (1.0, loopB - loopA - lxf)); }
                    else if (vo.samplePos >= rgA.endF) vo.sampleActive = false;
                }
                else
                {
                    vo.samplePos -= step;
                    if (loopOn) { if (vo.samplePos <= loopA) vo.samplePos = loopB - std::fmod (loopA - vo.samplePos, loopB - loopA); }
                    else if (vo.samplePos <= rgA.revFloor) vo.sampleActive = false;
                }
            }
            if (indepB && vo.sampleActiveB && ! vo.chop)
            {
                const int nb = smpB->frames();
                float bl = cubic (smpB->L, vo.samplePosB, nb), br = cubic (smpB->R, vo.samplePosB, nb);
                const double bxf = loopOnB && ! revB ? std::min (xfBF, 0.5 * (loopBB - loopAB)) : 0.0;
                if (bxf > 0.0 && vo.samplePosB >= loopBB - bxf)
                {
                    const double u = (vo.samplePosB - (loopBB - bxf)) / bxf, p2 = loopAB + u * bxf; const float gi = (float) std::sin (u * 1.5707963), go = (float) std::cos (u * 1.5707963);
                    bl = bl * go + cubic (smpB->L, p2, nb) * gi; br = br * go + cubic (smpB->R, p2, nb) * gi;
                }
                pl += bl * vSampB * vo.velocity * wB * eb; pr += br * vSampB * vo.velocity * wB * eb;
                const double stepB = std::pow (2.0, ((double) vo.note - rootB) / 12.0) * rateB * pf * tuneMulB;
                if (! revB)
                {
                    vo.samplePosB += stepB;
                    if (loopOnB) { if (vo.samplePosB >= loopBB) vo.samplePosB = loopAB + bxf + std::fmod (vo.samplePosB - loopBB, std::max (1.0, loopBB - loopAB - bxf)); }
                    else if (vo.samplePosB >= rgB.endF) vo.sampleActiveB = false;
                }
                else
                {
                    vo.samplePosB -= stepB;
                    if (loopOnB) { if (vo.samplePosB <= loopAB) vo.samplePosB = loopBB - std::fmod (loopAB - vo.samplePosB, loopBB - loopAB); }
                    else if (vo.samplePosB <= rgB.revFloor) vo.sampleActiveB = false;
                }
            }
            if (msl != 0.0f) { const float sf = juce::jlimit (0.0f, 2.0f, 1.0f + msl); pl *= sf; pr *= sf; }      // the Sample Level target (samples only, not the texture)
            const float texMod = mtl != 0.0f ? juce::jlimit (0.0f, 2.0f, 1.0f + mtl) : 1.0f;
            if (texOn && ! vo.chop && vo.zone != 2)
            {
                const int nn = tsmp->frames();
                vo.grainTimer -= 1.0f;
                if (vo.grainTimer <= 0.0f)
                {
                    auto rnd = [&] { vo.gseed = vo.gseed * 1664525u + 1013904223u; return (float) (vo.gseed >> 8) / 16777216.0f; };
                    const float r1 = rnd(), r2 = rnd(), r3 = rnd();
                    vo.grainTimer = (float) sr / texDens * (0.7f + 0.6f * r1);
                    for (auto& g : vo.grains) if (! g.on)
                    {
                        g.on = true; g.age = 0; g.len = std::max (64, (int) (texSize * 0.001 * sr));
                        g.pos = (float) juce::jlimit (0.0, nTex - 2.0, (double) (juce::jlimit (0.0f, 1.0f, texPos + 0.5f * mtp) + (r2 * 2.0f - 1.0f) * texSpread * 0.5f) * nTex);
                        g.step = (float) (std::pow (2.0, ((double) vo.note - texRoot) / 12.0 + (double) ((r3 * 2.0f - 1.0f) * texJit / 1200.0f)) * texRate * pf * texTuneMul);
                        const float a = (1.0f + (r1 * 2.0f - 1.0f) * 0.6f) * (float) (rf::kPi / 4.0); g.gl = std::cos (a) * 1.41421356f; g.gr = std::sin (a) * 1.41421356f;
                        break;
                    }
                }
                for (auto& g : vo.grains) if (g.on)
                {
                    const float wgt = 0.5f - 0.5f * std::cos (2.0f * (float) rf::kPi * (float) g.age / (float) g.len) ;
                    const float xl = cubic (tsmp->L, g.pos, nn) * wgt * g.gl * texGain * vo.velocity * et * texMod, xr = cubic (tsmp->R, g.pos, nn) * wgt * g.gr * texGain * vo.velocity * et * texMod;
                    if (texSep) { tl += xl; tr += xr; } else { pl += xl; pr += xr; }
                    g.pos += g.step; if (++g.age >= g.len || g.pos >= (float) nn - 2.0f) g.on = false;
                }
            }
            if (texSep)       // own filter, then either with the samples (through the voice filter) or dry (after it)
            {
                if (texFiltOn) { tl = vo.ftl.process (tl, texFiltType); tr = vo.ftr.process (tr, texFiltType); }
                if (texDry) { dtl = tl; dtr = tr; } else { pl += tl; pr += tr; }
            }
            if (fOn)
            {
                auto run = [&] (float x, rf::Svf& f1, rf::Svf& f2, float& mp)
                {
                    switch (fModel)
                    {
                        case 1: x = std::tanh (1.6f * x) * (1.0f / 1.6f) * 1.15f; return f1.process (x, fType);                        // OBX: soft-clipped input, 12 dB
                        case 2: x = std::round (x * 2048.0f) / 2048.0f; return f2.process (f1.process (x, fType), fType);              // SP-12: 12-bit input, two filter stages
                        case 3: { x = std::round (x * 2048.0f) / 2048.0f; const float y = f1.process (x, fType); mp = y * (1.0f - mpA) + mp * mpA; return mp; }   // MPC60: 12-bit input, soft top end
                        default: return f1.process (x, fType);
                    }
                };
                if (fSrc == 0) { l += pl; r += pr; l = run (l, vo.fl, vo.fl2, vo.mpL); r = run (r, vo.fr, vo.fr2, vo.mpR); }
                else if (fSrc == 1) { l = run (l, vo.fl, vo.fl2, vo.mpL) + pl; r = run (r, vo.fr, vo.fr2, vo.mpR) + pr; }
                else { l += run (pl, vo.fl, vo.fl2, vo.mpL); r += run (pr, vo.fr, vo.fr2, vo.mpR); }
            }
            else { l += pl; r += pr; }
            l += dtl; r += dtr;       // the dry texture bypasses the voice filter
            if (mpn != 0.0f) { const float a = (juce::jlimit (-1.0f, 1.0f, mpn) + 1.0f) * (float) (rf::kPi / 4.0); l *= std::cos (a) * 1.41421356f; r *= std::sin (a) * 1.41421356f; }   // Pan target
            const float e = vo.env.next (aMs, dMs, sus, rMs, sr);
            L[i] += l * e; R[i] += r * e;
        }
        if (! vo.env.active()) vo.active = false;
    }
}

// Output safety: transparent below 0.9, then a soft knee that never passes full scale.
static inline float safe (float x)
{
    const float a = std::abs (x);
    if (a <= 0.9f) return x;
    const float y = 0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f);
    return x < 0.0f ? -y : y;
}

void RelicProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    { const juce::SpinLock::ScopedTryLockType l (sampleLock); if (l.isLocked()) { audioSample = sample; audioSampleB = sampleB; audioSampleT = sampleT; audioWt = wtUser; } }
    curWt = (audioWt != nullptr && apvts.getRawParameterValue ("wt_use_file")->load() > 0.5f) ? audioWt.get() : nullptr;
    const int N = buffer.getNumSamples();
    buffer.clear();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;

    keyboardState.processNextMidiBuffer (midi, 0, N, true);
    // ---- notes: keys -> chord engine -> arpeggiator -> voices ----
    double bpm = 120.0, ppq = -1.0; bool playing = false;
    if (auto* ph = getPlayHead()) if (auto info = ph->getPosition())
    { if (auto bp = info->getBpm()) bpm = *bp; hostBpm = bpm; if (auto q = info->getPpqPosition()) ppq = *q; playing = info->getIsPlaying(); }
    inEvts.clear(); outEvts.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage(); const int64_t t = sampleCounter + juce::jlimit (0, N - 1, meta.samplePosition);
        if (m.isNoteOn()) inEvts.push_back ({ t, true, m.getNoteNumber(), m.getFloatVelocity() });
        else if (m.isNoteOff()) inEvts.push_back ({ t, false, m.getNoteNumber(), 0.0f });
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.reset (t, outEvts);
        else if (m.isController() && m.getControllerNumber() == 1) modWheel = m.getControllerValue() / 127.0f;
        else if (m.isPitchWheel()) pitchBend = (m.getPitchWheelValue() - 8192) / 8192.0f;
    }
    engine.process (sampleCounter, N, inEvts, readChordArp(), bpm, ppq, playing, sr, outEvts);
    std::stable_sort (outEvts.begin(), outEvts.end(), [] (const rf::NoteEvt& a, const rf::NoteEvt& b) { return a.t < b.t; });
    int pos = 0;
    for (const auto& e : outEvts)
    {
        const int t = (int) juce::jlimit<int64_t> (0, N, e.t - sampleCounter);
        renderSegment (L + pos, R + pos, t - pos); pos = t;
        if (e.on) noteOn (e.note, e.vel); else noteOff (e.note);
    }
    arpStepShown = engine.isArpRunning() ? engine.currentStep() : -1;
    arpWaitingShown = engine.isArpWaiting();
    chordDegreeShown = engine.heldCount() > 0 ? engine.lastDegree() : -1;
    sampleCounter += N;
    renderSegment (L + pos, R + pos, N - pos);

    auto on = [this] (const char* id) { return p (id)->load() > 0.5f; };
    const bool cOn = on ("crush_on"), pOn = on ("pan_on"), tOn = on ("trem_on"), kOn = on ("comp_on"), dOn = on ("delay_on"), chOn = on ("chorus_on"), flOn = on ("flanger_on"), rvOn = on ("reverb_on"),
               eqOn = on ("eq_on"), tpOn = on ("tape_on"), wdOn = on ("widener_on"), whOn = on ("wah_on"), lfOn = on ("lofi_on"), otOn = on ("ott_on"), pdOn = on ("pdelay_on"), tkOn = on ("talk_on"), sgOn = on ("sgate_on");
    const float master = p ("master")->load();
    for (int i = 0; i < N; ++i)
    {
        float l = L[i], r = R[i];
        if (cOn) crush.process (l, r, p ("crush_bits")->load(), p ("crush_downsample")->load(), p ("crush_mix")->load());
        if (lfOn) lofi.process (l, r, p ("lofi_tone_hz")->load(), p ("lofi_noise")->load(), p ("lofi_wobble")->load(), p ("lofi_mix")->load(), sr);
        if (whOn) wah.process (l, r, (int) p ("wah_mode")->load(), p ("wah_rate_hz")->load(), p ("wah_depth")->load(), p ("wah_reso")->load(), p ("wah_mix")->load(), sr);
        if (tkOn) talk.process (l, r, p ("talk_vowel")->load(), p ("talk_reso")->load(), p ("talk_rate_hz")->load(), p ("talk_depth")->load(), p ("talk_mix")->load(), sr);
        if (tpOn) tape.process (l, r, p ("tape_drive")->load(), p ("tape_tone")->load(), p ("tape_wobble")->load(), sr);
        if (eqOn) eq.process (l, r, p ("eq_low_db")->load(), p ("eq_mid_hz")->load(), p ("eq_mid_db")->load(), p ("eq_high_db")->load(), sr);
        if (pOn) panner.process (l, r, p ("pan_rate_hz")->load(), p ("pan_depth")->load(), (int) p ("pan_shape")->load(), sr);
        if (tOn) trem.process (l, r, p ("trem_rate_hz")->load(), p ("trem_depth")->load(), sr);
        if (kOn) comp.process (l, r, p ("comp_threshold_db")->load(), p ("comp_ratio")->load(), p ("comp_attack_ms")->load(),
                               p ("comp_release_ms")->load(), p ("comp_makeup_db")->load(), sr);
        if (otOn) ott.process (l, r, p ("ott_depth")->load(), p ("ott_time")->load(), p ("ott_mix")->load(), sr);
        if (sgOn) sgate.process (l, r, p ("sgate_threshold_db")->load(), p ("sgate_depth")->load(), p ("sgate_release_ms")->load(), p ("sgate_mix")->load(), sr);
        if (chOn) chorus.process (l, r, p ("chorus_rate_hz")->load(), p ("chorus_depth")->load(), p ("chorus_mix")->load(), sr);
        if (flOn) flanger.process (l, r, p ("flanger_rate_hz")->load(), p ("flanger_depth")->load(), p ("flanger_feedback")->load(), p ("flanger_mix")->load(), sr);
        if (dOn) delay.process (l, r, p ("delay_time_ms")->load(), p ("delay_feedback")->load(), p ("delay_mix")->load(),
                                p ("delay_pingpong")->load() > 0.5f, sr, (int) p ("delay_mode")->load());
        if (pdOn) pdelay.process (l, r, p ("pdelay_time_ms")->load(), p ("pdelay_feedback")->load(), (int) p ("pdelay_semi")->load(), p ("pdelay_mix")->load(), sr);
        if (rvOn) reverb.process (l, r, p ("reverb_size")->load(), p ("reverb_damp")->load(), p ("reverb_predelay_ms")->load(), p ("reverb_mix")->load(), sr);
        if (wdOn) widener.process (l, r, p ("widener_width")->load());
        L[i] = safe (l * master); R[i] = safe (r * master);
    }
    levelL = std::max (buffer.getMagnitude (0, 0, N), levelL.load() * 0.92f);
    levelR = std::max (buffer.getMagnitude (buffer.getNumChannels() > 1 ? 1 : 0, 0, N), levelR.load() * 0.92f);
}

juce::AudioProcessorEditor* RelicProcessor::createEditor() { return new RelicEditor (*this); }

void RelicProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void RelicProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            // Older projects predate GO/STOP; preserve their always-running arp,
            // even when loaded over a stopped instance.
            if (! state.getChildWithProperty ("id", "arp_run").isValid())
            {
                juce::ValueTree run ("PARAM"); run.setProperty ("id", "arp_run", nullptr); run.setProperty ("value", 1.0f, nullptr);
                state.appendChild (run, nullptr);
            }
            auto rhythmDefault = [&] (const juce::String& id, float value)
            {
                if (state.getChildWithProperty ("id", id).isValid()) return;
                juce::ValueTree param ("PARAM"); param.setProperty ("id", id, nullptr); param.setProperty ("value", value, nullptr);
                state.appendChild (param, nullptr);
            };
            // Loading an older project over an edited lane restores the old sound.
            rhythmDefault ("arp_rhythm_on", 0); rhythmDefault ("arp_rhythm_length", 16);
            rhythmDefault ("arp_two_hand", 0); rhythmDefault ("arp_chance", 1); rhythmDefault ("arp_spray", 0);   // same for the later arp additions
            for (int i = 1; i <= 16; ++i)
            {
                const auto id = "arp_rhythm_" + juce::String (i);
                rhythmDefault (id + "_type", 0); rhythmDefault (id + "_velocity", 1);
            }
            // Any parameter this project does not mention (it predates it) takes its default, even when loaded over an edited instance.
            for (auto* prm : getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                {
                    const auto pid = rp->getParameterID();
                    if (! state.getChildWithProperty ("id", pid).isValid()) rhythmDefault (pid, rp->convertFrom0to1 (rp->getDefaultValue()));
                }
            apvts.replaceState (state);
            const auto path = apvts.state.getProperty ("samplePath").toString();
            if (path.isNotEmpty()) { juce::File f (path); if (f.existsAsFile()) loadSampleFile (f, false); else sampleError = "Sample not found: " + f.getFileName(); }
            const auto pathB = apvts.state.getProperty ("samplePathB").toString();
            if (pathB.isNotEmpty()) { juce::File f (pathB); if (f.existsAsFile()) loadSampleFile (f, false, true); }
            const auto pathT = apvts.state.getProperty ("samplePathT").toString();
            if (pathT.isNotEmpty()) { juce::File f (pathT); if (f.existsAsFile()) loadTextureFile (f); }
            const auto pathW = apvts.state.getProperty ("wavetablePath").toString();
            if (pathW.isNotEmpty()) { juce::File f (pathW); if (f.existsAsFile()) loadWavetableFile (f); else { clearWavetable(); sampleError = "Wavetable not found: " + f.getFileName(); } }
            else clearWavetable();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RelicProcessor(); }

// ============================ randomizers and undo history ============================
uint64_t RelicProcessor::displayStateKey() const
{
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h] (uint64_t v) { h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2); };
    for (auto* prm : getParameters()) { const float v = prm->getValue(); uint32_t bits = 0; std::memcpy (&bits, &v, sizeof (bits)); mix (bits); }
    mix ((uint64_t) (arpStepShown.load() + 2)); mix ((uint64_t) (chordDegreeShown.load() + 2)); mix (arpWaitingShown.load() ? 1u : 0u);
    mix ((uint64_t) (hostBpm * 10.0)); mix ((uint64_t) (uintptr_t) getSample().get()); mix ((uint64_t) (uintptr_t) getSampleB().get()); mix ((uint64_t) (uintptr_t) getSampleT().get());
    return h;
}

RelicProcessor::Snap RelicProcessor::snapshot() const
{
    Snap s; for (auto* prm : getParameters()) s.push_back (prm->getValue()); return s;
}
void RelicProcessor::applySnap (const Snap& s)
{
    const auto& ps = getParameters();
    for (int i = 0; i < ps.size() && i < (int) s.size(); ++i)
        if (std::abs (ps[i]->getValue() - s[(size_t) i]) > 1.0e-7f) { ps[i]->beginChangeGesture(); ps[i]->setValueNotifyingHost (s[(size_t) i]); ps[i]->endChangeGesture(); }
}
static bool snapDiffers (const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size()) return true;
    for (size_t i = 0; i < a.size(); ++i) if (std::abs (a[i] - b[i]) > 1.0e-5f) return true;
    return false;
}
void RelicProcessor::flushPending()
{
    pending = snapshot();
    if (cur.empty()) { cur = pending; return; }
    if (snapDiffers (pending, cur)) { undoS.push_back (cur); if (undoS.size() > 100) undoS.erase (undoS.begin()); cur = pending; redoS.clear(); }
    quiet = 0;
}
bool RelicProcessor::pendingDiffers() const { return snapDiffers (pending, cur); }
void RelicProcessor::historyTick()
{
    const auto s = snapshot();
    if (cur.empty()) { cur = pending = s; return; }
    if (snapDiffers (s, pending)) { pending = s; quiet = 0; return; }
    if (snapDiffers (pending, cur) && ++quiet >= 3) flushPending();
}
bool RelicProcessor::undo()
{
    flushPending(); if (undoS.empty()) return false;
    redoS.push_back (cur); cur = undoS.back(); undoS.pop_back(); applySnap (cur); pending = cur; return true;
}
bool RelicProcessor::redo()
{
    flushPending(); if (redoS.empty()) return false;
    undoS.push_back (cur); cur = redoS.back(); redoS.pop_back(); applySnap (cur); pending = cur; return true;
}

static bool isEffectId (const juce::String& id)
{
    static const char* fx[] = { "trem_", "delay_", "comp_", "crush_", "pan_", "chorus_", "flanger_", "reverb_", "eq_", "tape_", "widener_", "wah_", "lofi_", "ott_", "pdelay_", "talk_", "sgate_" };
    for (auto k : fx) if (id.startsWith (k)) return true;
    return false;
}
void RelicProcessor::switchFxSlot()
{
    flushPending();
    std::vector<float> now; const auto& ps = getParameters();
    for (auto* prm : ps) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm)) if (isEffectId (rp->paramID)) now.push_back (rp->getValue());
    fxSlot[fxActive] = now; const int other = 1 - fxActive;
    if (fxSlot[other].empty()) fxSlot[other] = now;                 // first switch: B starts as a copy of A
    size_t k = 0;
    for (auto* prm : ps) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm)) if (isEffectId (rp->paramID))
    { const float v = fxSlot[other][k++]; rp->beginChangeGesture(); rp->setValueNotifyingHost (v); rp->endChangeGesture(); }
    fxActive = other;
}

void RelicProcessor::craftSound()
{
    auto put = [&] (const char* id, float actual)
    { if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id))) { rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->convertTo0to1 (actual)); rp->endChangeGesture(); } };
    auto uni = [&] (float a, float b) { return a + (b - a) * rng.nextFloat(); };
    auto logu = [&] (float a, float b) { return std::exp (uni (std::log (a), std::log (b))); };
    auto pick = [&] (std::initializer_list<int> v) { return *(v.begin() + rng.nextInt ((int) v.size())); };
    enum { Sine = 0, Saw = 1, Square = 2, Tri = 3, Table = 4 };
    const int style = rng.nextInt (5);          // 0 pad, 1 pluck, 2 lead, 3 bass, 4 keys
    int wave = Saw; float uniV = 1, det = 8, spread = 0.6f, a = 3, d = 300, s = 0.7f, r = 300, cut = 4000, reso = 0.15f; bool useTable = false;
    switch (style)
    {
        case 0: wave = pick ({ Saw, Saw, Square, Tri, Table }); uniV = (float) (3 + rng.nextInt (3)); det = uni (8, 20); spread = uni (0.6f, 0.9f); a = uni (250, 900); d = uni (400, 1200); s = uni (0.6f, 0.9f); r = uni (600, 1800); cut = logu (1500, 6000); reso = uni (0.05f, 0.25f); break;
        case 1: wave = pick ({ Saw, Square, Table, Tri }); uniV = (float) (1 + rng.nextInt (3)); det = uni (4, 12); spread = uni (0.3f, 0.7f); a = uni (0.5f, 4); d = uni (150, 450); s = uni (0.0f, 0.25f); r = uni (120, 400); cut = logu (1200, 5000); reso = uni (0.1f, 0.28f); break;
        case 2: wave = pick ({ Saw, Saw, Square }); uniV = (float) (2 + rng.nextInt (2)); det = uni (6, 14); spread = uni (0.4f, 0.8f); a = uni (2, 15); d = uni (100, 300); s = uni (0.6f, 0.9f); r = uni (80, 250); cut = logu (2500, 9000); reso = uni (0.05f, 0.3f); break;
        case 3: wave = pick ({ Saw, Square, Tri }); uniV = (float) (1 + rng.nextInt (2)); det = uni (3, 8); spread = uni (0.1f, 0.3f); a = uni (1, 5); d = uni (150, 400); s = uni (0.5f, 0.8f); r = uni (80, 200); cut = logu (300, 1200); reso = uni (0.1f, 0.4f); break;
        default: wave = pick ({ Tri, Sine, Table, Square }); uniV = (float) (1 + rng.nextInt (2)); det = uni (3, 9); spread = uni (0.3f, 0.7f); a = uni (1, 6); d = uni (300, 900); s = uni (0.2f, 0.5f); r = uni (200, 600); cut = logu (3000, 10000); reso = uni (0.0f, 0.2f); break;
    }
    useTable = wave == Table;
    // loudness: each style has its own level, and extra oscillator layers pull the main one down so chords never reach the limiter
    static const float styleLevel[5] = { 0.36f, 0.36f, 0.32f, 0.28f, 0.38f };
    float layers = 0.0f;
    put ("waveform", (float) wave); put ("pulse_width", useTable ? uni (0.15f, 0.7f) : uni (0.35f, 0.65f));
    put ("unison_voices", uniV); put ("unison_detune", det); put ("unison_spread", spread);
    put ("attack_ms", a); put ("decay_ms", d); put ("sustain", s); put ("release_ms", r);
    put ("filter_on", 1.0f); put ("filter_type", 0.0f); put ("filter_model", (float) pick ({ 0, 0, 1 })); put ("filter_source", 0.0f); put ("cutoff_hz", cut); put ("resonance", reso);
    // extra oscillators: only consonant layers (unison, octaves, a fifth), gently detuned
    put ("osc2_on", 0.0f); put ("osc3_on", 0.0f);
    if (rng.nextFloat() < 0.45f)
    {
        const int kind = rng.nextInt (4); put ("osc2_on", 1.0f); put ("osc2_wave", (float) pick ({ Saw, Square, Tri, Sine }));
        put ("osc2_oct", kind == 1 ? 1.0f : (kind == 2 ? -1.0f : 0.0f)); put ("osc2_semi", kind == 3 ? 7.0f : 0.0f); put ("osc2_fine", kind == 0 ? uni (5, 12) : uni (-4, 4)); { const float l2 = kind == 2 ? uni (0.55f, 0.85f) : uni (0.4f, 0.75f); put ("osc2_level", l2); layers += l2; }
    }
    if (rng.nextFloat() < 0.2f)
    {
        put ("osc3_on", 1.0f); put ("osc3_wave", (float) pick ({ Saw, Tri, Sine })); put ("osc3_oct", (float) pick ({ -1, 1 })); put ("osc3_semi", 0.0f); put ("osc3_fine", uni (-9, -4)); { const float l3 = uni (0.3f, 0.55f); put ("osc3_level", l3); layers += l3; }
    }
    static const float waveGain[5] = { 0.85f, 1.0f, 0.72f, 0.95f, 1.0f };   // sine, saw, square, triangle, wavetable: roughly equal loudness
    put ("osc_level", std::min (0.9f, styleLevel[style] * waveGain[wave] / (1.0f + 0.35f * layers)));
    // modulation: start clean, then maybe one slow, gentle movement
    for (int k = 1; k <= 4; ++k) { put ((juce::String ("mod") + juce::String (k) + "_src").toRawUTF8(), 0.0f); put ((juce::String ("mod") + juce::String (k) + "_dst").toRawUTF8(), 0.0f); put ((juce::String ("mod") + juce::String (k) + "_amt").toRawUTF8(), 0.0f); }
    put ("lfo1_shape", 0.0f); put ("lfo1_sync", 0.0f); put ("lfo1_keysync", 0.0f);
    if (style != 1 && rng.nextFloat() < 0.4f) { put ("lfo1_rate_hz", uni (0.25f, 0.9f)); put ("mod1_src", 1.0f); put ("mod1_dst", 1.0f); put ("mod1_amt", uni (0.05f, 0.14f)); }
    if (style == 1 || style == 2) { put ("mod2_src", 5.0f); put ("mod2_dst", 1.0f); put ("mod2_amt", uni (0.08f, 0.22f)); put ("menv1_attack_ms", 1.0f); put ("menv1_decay_ms", uni (150, 450)); put ("menv1_sustain", 0.0f); }
    // effects: a little space and warmth only. Everything off first, then one or two musical ones, with tuned settings.
    if (! lockFx.load())
    {
        for (auto id : { "trem_on", "delay_on", "comp_on", "crush_on", "pan_on", "chorus_on", "flanger_on", "reverb_on", "eq_on", "tape_on", "widener_on", "wah_on", "lofi_on", "ott_on", "pdelay_on", "talk_on", "sgate_on" }) put (id, 0.0f);
        put ("delay_mode", 0.0f);
        const bool wantReverb = style == 0 || rng.nextFloat() < 0.65f, wantDelay = style != 0 && rng.nextFloat() < 0.5f, wantChorus = (style == 0 || style == 4) && rng.nextFloat() < 0.6f, wantTape = rng.nextFloat() < 0.25f;
        if (wantReverb) { put ("reverb_on", 1.0f); put ("reverb_size", uni (0.35f, 0.75f)); put ("reverb_damp", uni (0.3f, 0.65f)); put ("reverb_predelay_ms", uni (5, 25)); put ("reverb_mix", style == 0 ? uni (0.22f, 0.38f) : uni (0.1f, 0.24f)); }
        if (wantDelay) { const double beat = 60000.0 / std::max (40.0, hostBpm); put ("delay_on", 1.0f); put ("delay_time_ms", (float) juce::jlimit (60.0, 1500.0, beat * (rng.nextBool() ? 0.75 : 0.5))); put ("delay_feedback", uni (0.25f, 0.45f)); put ("delay_mix", uni (0.12f, 0.24f)); put ("delay_pingpong", rng.nextBool() ? 1.0f : 0.0f); put ("delay_mode", (float) pick ({ 0, 1 })); }
        if (wantChorus) { put ("chorus_on", 1.0f); put ("chorus_rate_hz", uni (0.4f, 1.1f)); put ("chorus_depth", uni (0.3f, 0.55f)); put ("chorus_mix", uni (0.25f, 0.4f)); }
        if (wantTape) { put ("tape_on", 1.0f); put ("tape_drive", uni (0.3f, 0.5f)); put ("tape_tone", uni (0.5f, 0.75f)); put ("tape_wobble", uni (0.08f, 0.2f)); }
    }
    apvts.state.setProperty ("presetName", "Crafted", nullptr);
}

void RelicProcessor::randomizeSound (bool chaos)
{
    flushPending();
    if (! chaos) { craftSound(); return; }
    static const char* fxPrefix[] = { "trem_", "delay_", "comp_", "crush_", "pan_", "chorus_", "flanger_", "reverb_", "eq_", "tape_", "widener_", "wah_", "lofi_", "ott_", "pdelay_", "talk_", "sgate_" };
    static const char* kept[] = { "master", "voices", "synth_mix", "sample_", "loop_", "tex_", "chop_", "chord_", "arp_", "lead_on", "porta_", "vib_" };
    auto starts = [] (const juce::String& id, const char* pre) { return id.startsWith (pre); };
    auto uni = [&] (float a, float b) { return a + (b - a) * rng.nextFloat(); };
    for (auto* prm : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm); if (rp == nullptr) continue;
        const auto id = rp->paramID; bool skip = false, isFx = false;
        for (auto k : kept) if (starts (id, k)) skip = true;
        for (auto k : fxPrefix) if (starts (id, k)) isFx = true;
        if (skip || (isFx && lockFx.load())) continue;
        if (! chaos && starts (id, "mod")) continue;          // CRAFT leaves the mod matrix as it is
        if (! chaos && starts (id, "lfo")) continue;
        auto set01 = [&] (float v) { rp->beginChangeGesture(); rp->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v)); rp->endChangeGesture(); };
        auto setVal = [&] (float v) { set01 (rp->convertTo0to1 (v)); };
        const bool isToggle = dynamic_cast<juce::AudioParameterBool*> (rp) != nullptr;
        if (isToggle) { set01 (rng.nextFloat() < (chaos ? 0.5f : (isFx ? 0.2f : 0.5f)) ? 1.0f : 0.0f); if (id == "filter_on") set01 (1.0f); continue; }
        if (chaos)
        {
            float v = rng.nextFloat();
            if (id == "cutoff_hz") v = uni (0.4f, 1.0f); else if (id == "resonance") v *= 0.8f; else if (id == "osc_level") v = uni (0.3f, 0.9f);
            else if (id == "sustain") v = uni (0.3f, 1.0f); else if (id == "attack_ms") v *= 0.6f; else if (id == "filter_type") v = rng.nextFloat() < 0.6f ? 0.0f : v;
            else if (id == "waveform") v = rng.nextFloat();
            set01 (v); continue;
        }
        // CRAFT: stays inside musical ranges
        if (id == "waveform") set01 (rng.nextFloat());
        else if (id == "pulse_width") setVal (uni (0.25f, 0.75f));
        else if (id == "osc_level") setVal (uni (0.4f, 0.7f));
        else if (id == "unison_voices") setVal ((float) (1 + rng.nextInt (5)));
        else if (id == "unison_detune") setVal (uni (0.0f, 28.0f));
        else if (id == "unison_spread") setVal (uni (0.2f, 1.0f));
        else if (id == "attack_ms") setVal (rng.nextFloat() < 0.6f ? uni (0.5f, 15.0f) : uni (40.0f, 700.0f));
        else if (id == "decay_ms") setVal (uni (60.0f, 1400.0f));
        else if (id == "sustain") setVal (uni (0.25f, 1.0f));
        else if (id == "release_ms") setVal (uni (40.0f, 1500.0f));
        else if (id == "filter_type") set01 (rng.nextFloat() < 0.7f ? 0.0f : rng.nextFloat());
        else if (id == "cutoff_hz") setVal (std::exp (uni (std::log (500.0f), std::log (16000.0f))));
        else if (id == "resonance") setVal (uni (0.0f, 0.5f));
        else if (isFx) set01 (uni (0.2f, 0.7f));
        else set01 (rng.nextFloat());
    }
    if (! chaos) if (auto* f = apvts.getParameter ("filter_on")) { f->beginChangeGesture(); f->setValueNotifyingHost (1.0f); f->endChangeGesture(); }
    apvts.state.setProperty ("presetName", "Crafted", nullptr);
    if (chaos) apvts.state.setProperty ("presetName", "Chaos", nullptr);
}
