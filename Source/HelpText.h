// Hover help: short plain-language descriptions for the controls. A tip appears when the mouse rests on a control for a moment.
// forId() takes a parameter id ("delay_time_ms"), forName() a button name ("CRAFT"). Both return an empty string when there is no help.
#pragma once
#include <juce_core/juce_core.h>
#include <map>

namespace help
{
using Table = std::map<juce::String, juce::String>;

// Parameters that come in numbered families share one entry: osc3_* reads osc2_*, lfo2_* reads lfo1_*, mod2..4_* read mod1_*, menv2_* reads menv1_*.
inline juce::String familyKey (juce::String id)
{
    if (id.startsWith ("osc3_")) return "osc2_" + id.fromFirstOccurrenceOf ("_", false, false);
    if (id.startsWith ("lfo2_")) return "lfo1_" + id.fromFirstOccurrenceOf ("_", false, false);
    if (id.startsWith ("menv2_")) return "menv1_" + id.fromFirstOccurrenceOf ("_", false, false);
    if (id.length() > 5 && id.startsWith ("mod") && id[3] >= '2' && id[3] <= '4' && id[4] == '_') return "mod1_" + id.substring (5);
    return id;
}

inline const Table& params()
{
    static const Table t = {
        // oscillator, envelope, master
        { "waveform", "The main oscillator's waveform. The arrows on either side step through them." },
        { "pulse_width", "Pulse width of the square wave, from narrow to wide. You only hear it on the square waveform." },
        { "osc_level", "Level of the main oscillator." },
        { "unison_voices", "How many slightly detuned copies of each note play together, from 1 to 7. More makes a thicker sound." },
        { "unison_detune", "How far apart in pitch the unison copies are, in cents (hundredths of a semitone)." },
        { "unison_spread", "How widely the unison copies are spread between the left and right speakers." },
        { "voices", "Polyphony: how many notes can sound at once. 1 plays one note at a time." },
        { "attack_ms", "Attack: how long a note takes to rise to full level after you press a key." },
        { "decay_ms", "Decay: how long the level takes to fall from the peak to the sustain level." },
        { "sustain", "Sustain: the level a note holds at while the key stays down." },
        { "release_ms", "Release: how long a note takes to fade out after you let go of the key." },
        { "master", "Master volume for the whole instrument." },
        { "vib_on", "Vibrato: a gentle pitch wobble on every note." },
        { "vib_rate_hz", "Vibrato speed, in Hz." },
        { "vib_depth_ct", "Vibrato depth, in cents (hundredths of a semitone)." },
        { "porta_on", "Portamento: notes slide from one pitch to the next instead of jumping." },
        { "porta_ms", "Glide time: how long the slide between notes takes." },
        { "lead_on", "Lead mode: one voice at a time, the last note wins. Overlapping notes glide without restarting the envelope." },
        // filter
        { "filter_on", "Turns the filter on or off." },
        { "cutoff_hz", "Filter cutoff: the frequency where the filter starts turning the sound down." },
        { "resonance", "Filter resonance: boosts the frequencies around the cutoff. High settings ring." },
        // oscillators 2 and 3
        { "osc2_on", "Turns this extra oscillator on or off. It plays together with the main oscillator." },
        { "osc2_wave", "This oscillator's waveform." },
        { "osc2_oct", "Shifts this oscillator up or down in octaves." },
        { "osc2_semi", "Shifts this oscillator up or down in semitones." },
        { "osc2_level", "Level of this oscillator." },
        { "osc2_fine", "Fine tuning of this oscillator, in cents. Small amounts make a chorus-like thickening." },
        // sampler
        { "sample_loop", "Loops the sample between the Loop in and Loop out points while a key is held." },
        { "sample_reverse", "Plays the sample backwards." },
        { "sample_normalize", "Raises the sample's level so its loudest point reaches full scale." },
        { "sample_root", "The key at which the sample plays at its original pitch." },
        { "sample_octave", "Shifts the sample up or down in octaves." },
        { "sample_gain", "Level of the sample." },
        { "sample_start", "Where in the sample playback starts." },
        { "sample_end", "Where playback of the sample stops, independent of the loop end. At the far right it plays to the end of the file." },
        { "sample_tune_ct", "Fine tuning of the sample, in cents (hundredths of a semitone)." },
        { "loop_start", "Where the loop begins inside the sample." },
        { "loop_end", "Where the loop ends inside the sample." },
        { "loop_xfade", "Loop fade: blends the end of the loop into its start so the loop point does not click. Off or 5 to 500 ms." },
        { "synth_mix", "Balance between the sample (left) and the synth oscillators (right)." },
        { "sample_morph", "Morphs between sample A (left) and sample B (right)." },
        { "chop_on", "Chop: turns the loaded sample into pads you play with the low keys (C1 to D#2)." },
        { "chop_mode", "How the sample is cut: Even (equal parts), Onset (at the strongest hits) or Grid (on the tempo)." },
        { "chop_count", "How many slices the sample is cut into." },
        { "chop_grid", "The note length of each slice in Grid mode." },
        { "chop_pitch", "Shifts the pitch of the chop pads, in semitones." },
        // effects: tremolo, delay, compressor, crusher, panner
        { "trem_on", "Tremolo: turns the volume wobble on or off. Settings are on the FX tab." },
        { "trem_depth", "How deeply the volume wobbles. At full depth it drops to silence." },
        { "trem_rate_hz", "Tremolo speed, in Hz." },
        { "delay_on", "Delay: turns the echo on or off. Settings are on the FX tab." },
        { "delay_time_ms", "Time between echoes, in milliseconds." },
        { "delay_feedback", "How much of each echo feeds back into the next. Higher gives more repeats." },
        { "delay_mix", "Balance between the dry sound and the echoes." },
        { "delay_pingpong", "Ping-pong: the echoes bounce between the left and right speakers." },
        { "delay_mode", "Delay character: Clean, Tape (dark, saturating, wobbling repeats) or Wash (repeats smear together). Click to change." },
        { "comp_on", "Compressor: turns it on or off. Settings are on the FX tab." },
        { "comp_threshold_db", "The level above which the compressor starts turning the sound down." },
        { "comp_ratio", "How strongly the sound above the threshold is reduced." },
        { "comp_attack_ms", "How quickly the compressor reacts to a loud peak." },
        { "comp_release_ms", "How quickly the compressor lets go afterwards." },
        { "comp_makeup_db", "Volume added after compression to make up for what it removed." },
        { "crush_on", "Bitcrusher: turns it on or off. Settings are on the FX tab." },
        { "crush_bits", "Bit depth. Fewer bits give a rougher, more digital sound." },
        { "crush_downsample", "Sample-rate reduction. Higher values add a harsh, lo-fi edge." },
        { "crush_mix", "Balance between the dry sound and the crushed sound." },
        { "pan_on", "Auto-panner: turns it on or off. Settings are on the FX tab." },
        { "pan_rate_hz", "How fast the sound sweeps between left and right, in Hz." },
        { "pan_depth", "How far the sound sweeps. At full depth it goes hard left to hard right." },
        { "pan_shape", "Shape of the panning sweep: sine, triangle or square. Click to change." },
        // chorus, flanger, reverb
        { "chorus_on", "Chorus: turns it on or off. Settings are on the FX 2 tab." },
        { "chorus_rate_hz", "Speed of the chorus movement, in Hz." },
        { "chorus_depth", "How much the chorus moves the pitch. More depth is a wider, wobblier sound." },
        { "chorus_mix", "Balance between the dry sound and the chorus." },
        { "flanger_on", "Flanger: turns it on or off. Settings are on the FX 2 tab." },
        { "flanger_rate_hz", "Speed of the flanger sweep, in Hz." },
        { "flanger_depth", "How far the flanger sweeps." },
        { "flanger_feedback", "How much of the flanger output is fed back in. More gives a stronger, more metallic sweep." },
        { "flanger_mix", "Balance between the dry sound and the flanger." },
        { "reverb_on", "Reverb: turns it on or off. Settings are on the FX 2 tab." },
        { "reverb_size", "The size of the room. Larger rooms have longer tails." },
        { "reverb_damp", "How quickly high frequencies die away in the reverb tail. Higher is darker." },
        { "reverb_predelay_ms", "A short gap before the reverb starts, in milliseconds." },
        { "reverb_mix", "Balance between the dry sound and the reverb." },
        // EQ, tape, widener, wah, lo-fi, OTT
        { "eq_on", "EQ: turns the equalizer on or off. Settings are on the FX 3 tab." },
        { "eq_low_db", "Boosts or cuts the low frequencies." },
        { "eq_mid_hz", "The centre frequency of the mid band." },
        { "eq_mid_db", "Boosts or cuts the mid band." },
        { "eq_high_db", "Boosts or cuts the high frequencies." },
        { "tape_on", "Tape: turns the tape saturation on or off. Settings are on the FX 3 tab." },
        { "tape_drive", "How hard the sound is pushed into the tape. More drive is warmer and more distorted." },
        { "tape_tone", "Brightness of the tape sound." },
        { "tape_wobble", "Slow pitch wobble, like a worn tape." },
        { "widener_on", "Stereo widener: turns it on or off. Settings are on the FX 3 tab." },
        { "widener_width", "Stereo width: 0 is mono, 1 is normal and 2 is doubled." },
        { "wah_on", "Wah: turns it on or off. Settings are on the FX 3 tab." },
        { "wah_mode", "What moves the wah: an LFO or the loudness of the sound (Envelope). Click to change." },
        { "wah_rate_hz", "Speed of the wah sweep in LFO mode, in Hz." },
        { "wah_depth", "How far the wah sweeps." },
        { "wah_reso", "How sharp the wah's peak is." },
        { "wah_mix", "Balance between the dry sound and the wah." },
        { "lofi_on", "Lo-fi: turns it on or off. Settings are on the FX 3 tab." },
        { "lofi_tone_hz", "Where the lo-fi sound is cut off. Lower is duller." },
        { "lofi_noise", "How much vinyl-style crackle and noise is added." },
        { "lofi_wobble", "Slow pitch wobble, like an old record or tape." },
        { "lofi_mix", "Balance between the dry sound and the lo-fi sound." },
        { "ott_on", "OTT: turns it on or off. A heavy multiband compressor that makes the sound loud and dense." },
        { "ott_depth", "How hard OTT squashes the sound." },
        { "ott_time", "How fast OTT reacts." },
        { "ott_mix", "Balance between the dry sound and OTT." },
        // FX 4
        { "talk_on", "Talk box: turns it on or off. It shapes the sound into vowels." },
        { "talk_vowel", "Morphs between the vowels, from A to U." },
        { "talk_reso", "How sharply the vowel shapes are drawn." },
        { "talk_rate_hz", "Speed of the automatic vowel sweep, in Hz." },
        { "talk_depth", "How far the vowel sweep moves." },
        { "talk_mix", "Balance between the dry sound and the talk box." },
        { "sgate_on", "Spectral gate: turns it on or off. It turns down the quiet frequencies and adds about 23 ms of delay." },
        { "sgate_threshold_db", "Frequencies quieter than this are turned down." },
        { "sgate_depth", "How much the quiet frequencies are turned down." },
        { "sgate_release_ms", "How slowly the gate lets go." },
        { "sgate_mix", "Balance between the dry sound and the gated sound." },
        { "pdelay_on", "Pitch delay: turns it on or off. Each echo is shifted in pitch." },
        { "pdelay_time_ms", "Time between echoes, in milliseconds." },
        { "pdelay_feedback", "How much of each echo feeds back into the next." },
        { "pdelay_semi", "How many semitones each echo is shifted." },
        { "pdelay_mix", "Balance between the dry sound and the echoes." },
        // modulation
        { "lfo1_shape", "The LFO's wave shape: sine, triangle, saw, square or random." },
        { "lfo1_sync", "Free, or locked to the host tempo at a note length." },
        { "lfo1_rate_hz", "Speed of the LFO when it is not synced, in Hz." },
        { "lfo1_keysync", "Restarts the LFO whenever a note starts from silence." },
        { "mod1_src", "What drives this modulation slot: an LFO, an envelope, velocity, the mod wheel, the XY pad or a macro." },
        { "mod1_dst", "What this slot controls: cutoff, resonance, pitch, level or pulse width." },
        { "mod1_amt", "How strongly the source moves the target. Centre is none; left reverses it." },
        { "menv1_attack_ms", "Attack of this modulation envelope." },
        { "menv1_decay_ms", "Decay of this modulation envelope." },
        { "menv1_sustain", "Sustain level of this modulation envelope." },
        { "menv1_release_ms", "Release of this modulation envelope." },
        { "macro_1", "A free macro. Route it to a target with a modulation slot (Source: Macro 1)." },
        { "macro_2", "A free macro. Route it to a target with a modulation slot (Source: Macro 2)." },
        // texture
        { "tex_level", "Level of the grain cloud. Texture needs a sample loaded on the SAMPLER tab." },
        { "tex_pos", "Where in the sample the grains are taken from." },
        { "tex_spread", "How much the grain position wanders around that point." },
        { "tex_size_ms", "Length of each grain, in milliseconds." },
        { "tex_density", "How many grains play per second." },
        { "tex_jitter_ct", "Random pitch scatter on each grain, in cents." },
        // chords
        { "chord_on", "Chord engine: every key you press plays a whole chord built from the key and scale below." },
        { "chord_key", "The key (root note) the chords are built in." },
        { "chord_scale", "The scale the chords are built from." },
        { "chord_type", "What kind of chord each key plays: triad, sus2, sus4, 7th, 9th, add9, power, 11th or 13th." },
        { "chord_voicing", "How the chord notes are spread out: close, open, drop 2 or spread." },
        { "chord_inversion", "Turns the chord over: 0 is the root position, higher values move the bottom notes up." },
        { "chord_mode", "Scale: chords follow the key and scale. Fixed: one chord shape under every key, ignoring the scale." },
        { "chord_shape", "In Fixed mode, which chord shape is played under every key." },
        { "chord_lock", "Scale lock: keys outside the scale are moved to the nearest scale note." },
        { "chord_strum_ms", "Spreads the chord notes out in time like a strum, in milliseconds per note." },
        { "voice_lead", "Voice leading: each chord moves to the closest arrangement of the last one, like a pianist." },
        // arp
        { "arp_on", "Arpeggiator: plays the held notes (or chord) one after another." },
        { "arp_div", "Rate: the length of each arp step. Triplet (T) and dotted (D) lengths are included." },
        { "arp_mode", "Pattern: Up, Down, Up-Down, Down-Up, Random, Played, Chord, or Waterfall (overlapping notes that cascade up and mirror down)." },
        { "arp_octaves", "How many octaves the arp covers." },
        { "arp_order", "The order the held notes are taken in: by pitch, as played, converging, diverging or random." },
        { "arp_grouping", "Phrases the line into groups (such as 5-5-6) and restarts the pattern at the start of each group." },
        { "arp_wshape", "The shape of the Waterfall pattern." },
        { "arp_hold", "Hold: keeps the arp going after you let go of the keys." },
        { "arp_run", "GO loops while you hold the keys. STOP plays one pass per fresh chord and then waits." },
        { "arp_gate", "Gate: how long each note lasts compared with its step. At the top, notes tie into the next." },
        { "arp_swing", "Swing: pushes every other step late for a shuffle." },
        { "arp_human", "Human: loosens the timing and varies the loudness, with an occasional ghost note." },
        { "arp_chance", "Chance: the probability that each step plays. Lower values drop steps at random." },
        { "arp_spray", "Spray: the probability that a note jumps an octave up or down at random." },
        { "arp_two_hand", "2-HAND: adds a slow low voice under the arp, one octave below a held note, every 4 steps." },
        { "arp_rhythm_on", "Rhythm lane: turns on the hit, rest and tie pattern drawn above." },
        { "arp_rhythm_length", "How many steps the rhythm lane has, from 1 to 16." },
        // keyboard
        { "split_on", "Split: keys below the split point play single notes even when chords are on. Right-click to place the split." },
    };
    return t;
}

inline const Table& names()
{
    static const Table t = {
        // sidebar
        { "Presets", "Opens the preset browser: the Factory, User and Imported banks." },
        { "Chords", "Opens the CHORD tab." },
        { "One-shots", "A menu of the audio files in your Samples folder. Picking one loads it into the sampler." },
        { "Textures", "Opens the TEXTURE page: a cloud of tiny grains made from the loaded sample." },
        { "Expand", "Switches the window between its normal size and a larger one." },
        { "History", "Undo and redo, including undoing everything since the sound was loaded." },
        { "Save", "Saves the current sound as a preset in your User bank." },
        { "Init", "Resets every control to its default and removes the samples. It asks first." },
        // picture buttons
        { "CRAFT", "Builds a new, musical sound for you (pad, pluck, lead, bass or keys). Keeps your master level, voices, sample, chords and arp." },
        { "UNDO", "Steps back through your changes, up to 100 of them." },
        { "REDO", "Steps forward again after an undo." },
        { "CHAOS", "Rolls everything wider, including the modulation, but avoids silent results." },
        { "scene_prev", "Shows the previous picture." },
        { "scene_next", "Shows the next picture. The choice is saved with the project." },
        // tabs
        { "SAMPLE", "Sampler: load a sound, set its root note, loop, reverse and chop it." },
        { "OSC", "The main oscillator, unison and the amplitude envelope." },
        { "FX", "Tremolo, delay, compressor, bitcrusher and auto-panner." },
        { "FX 2", "Chorus, flanger and reverb." },
        { "FX 3", "EQ, tape, stereo widener, wah, lo-fi and OTT." },
        { "ARP", "The arpeggiator." },
        { "CHORD", "The chord engine: turns each key into a chord from a key and scale." },
        { "MOD", "Two LFOs and the modulation matrix." },
        { "OSC 2", "Oscillators 2 and 3, which play together with the main oscillator." },
        { "ENV", "Two modulation envelopes you can use as sources in the matrix." },
        { "FX 4", "Talk box, spectral gate and pitch delay." },
        // buttons around the picture and the effect pads' neighbours
        { "SAVE", "Saves the current sound as a preset in your User bank." },
        { "preset_prev", "Loads the previous preset." },
        { "preset_next", "Loads the next preset." },
        { "preset_bar", "Click to open the preset browser." },
        { "preset_search", "Type to filter the presets by name, category or bank. Esc clears it." },
        { "cat:ALL", "Shows the presets from every category." },
        { "cat:PADS", "Pads: slow, wide sounds that fill the space." },
        { "cat:KEYS", "Keys: piano, organ and electric-piano style sounds." },
        { "cat:LEADS", "Leads: bright sounds for melodies, plus brass and stacks." },
        { "cat:BASS", "Bass: low sounds, from clean sub to wobbles." },
        { "cat:PLUCK", "Plucks and bells: short, struck sounds that ring out." },
        { "cat:ARPS", "Arps: presets with the arpeggiator switched on." },
        { "cat:FX", "FX and ambient: drones, sweeps and textures." },
        { "cat:OTHER", "Presets without a category, such as ones you saved as Other." },
        { "LOCK FX", "Keeps the effects as they are when you press CRAFT or CHAOS." },
        { "BYPASS FX", "Switches every effect off." },
        { "ALL ON", "Switches the standard effects on. The creative ones (talk box, spectral gate, pitch delay) stay off." },
        { "EFFECTS A", "Switches between two complete sets of effect settings, A and B. The first time, B starts as a copy of A." },
        { "EFFECTS B", "Switches between two complete sets of effect settings, A and B." },
        { "POLY", "Switches between playing one note at a time and eight notes at once." },
        { "wave_prev", "Previous waveform." },
        { "wave_next", "Next waveform." },
        { "LP", "Low-pass: lets the low frequencies through and turns the highs down." },
        { "HP", "High-pass: lets the high frequencies through and turns the lows down." },
        { "BP", "Band-pass: lets a band around the cutoff through." },
        { "N", "Notch: removes a band around the cutoff." },
        { "filter_display", "The filter's response. Click to change the filter model (Clean, OBX, SP-12, MPC60). Shift-click to choose what it filters: everything, the synth only or the sample only." },
        { "LOAD", "Loads an audio file into the sampler. You can also drag a file onto the window." },
        { "LOAD B", "Loads a second sample into slot B. The A/B slider morphs between the two." },
        { "+", "Moves the on-screen keyboard up an octave." },
        { "-", "Moves the on-screen keyboard down an octave." },
        { "SPLIT", "Splits the keyboard. Click to turn it on; right-click a key to choose where." },
        { "arp_display", "Shows the arp pattern and the current step." },
        { "chord_display", "Shows the chord each scale degree plays. The tile you just played lights up." },
        { "sample_display", "The loaded sample. Drag the start marker, or the loop markers when looping is on." },
        { "wave_display", "The oscillator's waveform." },
        { "env_display", "The shape of the amplitude envelope: attack, decay, sustain and release. Change it with the knobs." },
        { "xy_pad", "Drag to move two sources at once (XY X and XY Y). Double-click to close." },
        { "meter", "Output level." },
    };
    return t;
}

inline juce::String forId (const juce::String& id)
{
    const auto it = params().find (familyKey (id));
    return it == params().end() ? juce::String() : it->second;
}
inline juce::String forName (const juce::String& name)
{
    const auto it = names().find (name);
    return it == names().end() ? juce::String() : it->second;
}
} // namespace help
