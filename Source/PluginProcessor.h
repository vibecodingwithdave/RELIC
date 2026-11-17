#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp/WaveTables.h"
#include "Dsp/Svf.h"
#include "Dsp/Adsr.h"
#include "Dsp/Effects.h"
#include "Dsp/Effects2.h"
#include "Dsp/Mod.h"
#include "Dsp/Automation.h"
#include "NoteEngine.h"

// A loaded sample: stereo float data plus a coarse peak overview for drawing.
struct SampleData
{
    std::vector<float> L, R;
    double rate = 44100.0;
    float peak = 1.0f;
    juce::String name;
    std::vector<float> peaksMin, peaksMax;      // 1024 buckets
    std::vector<float> onsets;                  // onset positions (0..1), strongest first
    int frames() const { return (int) L.size(); }
};

// Which part of the sound a preset holds. A "partial" preset carries only that part, and loading it leaves every other control as it is.
enum class PresetPart { Full = 0, Synth, Sample, Mod, Effects, Notes };
inline const juce::StringArray& presetPartNames() { static const juce::StringArray n { "Full", "Synth", "Sample", "Modulation", "Effects", "Arp & Chords" }; return n; }
PresetPart presetPartOfParam (const juce::String& paramId);      // the part a control belongs to (never Full)

struct PresetEntry { juce::String bank, name, category, part; juce::File file; };      // part: empty for a full preset, else its type name

// The categories shown in the preset browser. A preset whose category is empty or unknown counts as "Other".
inline const juce::StringArray& presetCategories()
{
    static const juce::StringArray c { "Pads", "Keys", "Leads", "Bass", "Plucks & Bells", "Arps", "FX & Ambient" };
    return c;
}

class RelicProcessor : public juce::AudioProcessor
{
public:
    RelicProcessor();
    ~RelicProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Relic"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- presets ----
    static juce::File presetsRoot();
    std::vector<PresetEntry> scanPresets() const;                 // factory, user and imported presets on disk
    bool loadPresetFile (const juce::File& f);                    // message thread
    bool loadPresetPart (const juce::XmlElement& xml, PresetPart part);   // applies only that part of a sound
    void initPreset();                                            // every control back to its default, no sample
    juce::File savePreset (const juce::String& name, const juce::String& category = {}, PresetPart part = PresetPart::Full);   // saves to Presets/User
    juce::String getPresetName() const { return apvts.state.getProperty ("presetName", "Init").toString(); }
    juce::String getPresetBank() const { return apvts.state.getProperty ("presetBank", "").toString(); }
    juce::String getPresetCategory() const { return apvts.state.getProperty ("presetCategory", "").toString(); }

    // ---- sampler ----
    bool loadSampleFile (const juce::File& f, bool setMixToSample = true, bool slotB = false);      // message thread
    void clearSampleB();
    std::shared_ptr<const SampleData> getSampleB() const { const juce::SpinLock::ScopedLockType l (sampleLock); return sampleB; }
    std::shared_ptr<const SampleData> getSampleT() const { const juce::SpinLock::ScopedLockType l (sampleLock); return sampleT; }
    bool loadTextureFile (const juce::File& f);
    std::shared_ptr<SampleData> decodeSample (const juce::File& f);      // reads and analyses a file; sets sampleError and returns null on failure
    void clearTexture();
    void clearSample();
    std::shared_ptr<const SampleData> getSample() const { const juce::SpinLock::ScopedLockType l (sampleLock); return sample; }
    // ---- external wavetables: each of the three oscillators can have its own file (osc 0 = the main one, 1 and 2 = OSC 2 and 3); it plays while that oscillator's FILE switch is on ----
    static juce::File wavetablesRoot();                                   // the user's wavetable folder: loading starts here
    bool loadWavetableFile (const juce::File& f, int osc = 0);            // message thread; false (and sampleError set) when the file is unusable
    void clearWavetable (int osc = 0);
    static const char* wtPathKey (int osc) { static const char* k[3] = { "wavetablePath", "wavetablePath2", "wavetablePath3" }; return k[juce::jlimit (0, 2, osc)]; }
    static const char* wtToggleId (int osc) { static const char* k[3] = { "wt_use_file", "osc2_wt_file", "osc3_wt_file" }; return k[juce::jlimit (0, 2, osc)]; }
    juce::File getWavetableFile (int osc = 0) const { return juce::File (apvts.state.getProperty (wtPathKey (osc)).toString()); }
    juce::String getWavetableName (int osc = 0) const { const juce::SpinLock::ScopedLockType l (sampleLock); return wtUser[osc] != nullptr ? juce::String (wtUser[osc]->name) : juce::String(); }
    std::shared_ptr<const rf::WaveTables::UserBank> getWavetableBank (int osc = 0) const { const juce::SpinLock::ScopedLockType l (sampleLock); return wtUser[osc]; }
    int getWavetableFrames (int osc = 0) const { const juce::SpinLock::ScopedLockType l (sampleLock); return wtUser[osc] != nullptr ? wtUser[osc]->frames : 0; }
    static std::vector<juce::File> scanWavetables (const juce::File& folder);   // every wav / aiff / flac under the folder, in natural order
    bool stepWavetable (int dir, int osc = 0);                            // loads the next / previous file in the loaded file's folder
    juce::String sampleError;
    std::atomic<int> arpStepShown { -1 }, chordDegreeShown { -1 };      // for the displays
    std::atomic<bool> arpWaitingShown { true };
    rf::ChordArpSettings readChordArp() const;
    juce::MidiKeyboardState keyboardState;          // on-screen keyboard
    std::atomic<float> levelL { 0.0f }, levelR { 0.0f };   // output meters
    static constexpr int kChopBase = 36;               // pad 1 = C1 (MIDI 36)
    // Slice boundaries as fractions 0..1: out[0..count] (count+1 values). Used by the audio thread and the display.
    int chopSlices (const SampleData& s, double bpm, double* out) const;
    double getHostBpm() const { return hostBpm; }
    // A number that changes whenever anything the displays draw changes (parameters, arp step, chord tile, tempo, loaded sample), so they can skip redrawing when it has not.
    uint64_t displayStateKey() const;

    // ---- randomizers and undo history (message thread) ----
    void randomizeSound (bool chaos);
    void craftSound();                                // the curated, musical roll behind CRAFT                 // CRAFT (musical) or CHAOS (anything goes); sample, voices, master, chops, chords, arp are kept
    std::atomic<bool> lockFx { false };               // when set, randomizing leaves the effects alone
    void historyTick();                               // call ~15 times a second; records a step once edits settle
    bool undo(); bool redo();
    bool canUndo() const { return ! undoS.empty() || pendingDiffers(); }
    bool pendingDiffers() const;
    bool canRedo() const { return ! redoS.empty(); }
    // effects A/B: two stored sets of every effect control
    int fxSlotActive() const { return fxActive; }
    void switchFxSlot();
    int undoCount() const { return (int) undoS.size() + (pendingDiffers() ? 1 : 0); }
    int redoCount() const { return (int) redoS.size(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Voice
    {
        bool active = false;
        int note = -1; float velocity = 0.0f; unsigned long long age = 0;
        double vibPh = 0.0; unsigned long long vibAge = 0, vibRelAge = 0;   // this note's own vibrato phase and age in samples (used when the vibrato has a delay or fade-in)
        double phase[7] {}; double phaseX[2] {};   // phaseX: oscillators 2 and 3
        double samplePos = 0.0; bool sampleActive = false;
        double samplePosB = 0.0; bool sampleActiveB = false;   // slot B when it plays as its own layer
        double baseHz = 440.0; float glide = 0.0f;       // glide: semitones still to travel
        struct Grain { float pos = 0, step = 1, gl = 1, gr = 1; int age = 0, len = 0; bool on = false; };
        Grain grains[12]; float grainTimer = 0.0f; unsigned int gseed = 1; float modRand = 0.0f;       // texture layer; modRand: the Random modulation source for this note
        float ks[8192] {}; int ksPos = 0; float ksLp = 0.0f;                    // plucked-string model
        float bellAmp[6] {}; unsigned int nseed = 1;                            // bell model
        int zone = 0;                                                           // keyboard split: 0 none, 1 sample only, 2 synth only
        bool chop = false; double sliceA = 0.0, sliceB = 0.0; int choke = 0; bool choked = false; float chokeGain = 1.0f; bool chopRev = false; double chopRateMul = 1.0; float chopMul = 1.0f;   // kit settings of the pad this voice plays
          // chop voice: slice bounds in frames
        rf::Adsr env, menv[2];                           // amp envelope; two modulation envelopes
        rf::Adsr envA, envB, envT;                       // optional own envelopes of sample layers A and B and of the texture
        rf::Svf ftl, ftr;                                // the texture's own filter
        rf::Svf fl, fr, fl2, fr2; float mpL = 0.0f, mpR = 0.0f;   // fl2/fr2: second pole pair for the cascaded models; mp*: MPC60 output low-pass state
    };
    static constexpr int MaxVoices = 16;
    Voice voices[MaxVoices];
    unsigned long long counter = 0;
    double sr = 44100.0;

    rf::NoteEngine engine; int64_t sampleCounter = 0; std::vector<rf::NoteEvt> inEvts, outEvts;
    mutable juce::SpinLock sampleLock;
    std::shared_ptr<const SampleData> sample, audioSample;   // audioSample is the audio thread's copy
    std::shared_ptr<const SampleData> sampleB, audioSampleB;  // slot B (A/B morph)
    std::shared_ptr<const SampleData> sampleT, audioSampleT;  // the texture's own file (used when "tex_indep" is on)
    struct WtRaw { std::vector<float> data; int frameLen = 0; std::string name; };
    std::shared_ptr<const WtRaw> wtRaw[3];                                // each file's samples, kept so a sample-rate change can rebuild the bank
    std::shared_ptr<const rf::WaveTables::UserBank> wtUser[3], audioWt[3];   // audioWt is the audio thread's copy
    const rf::WaveTables::UserBank* curWt[3] = { nullptr, nullptr, nullptr };   // the bank each oscillator plays this block (null: built-in)
    bool wtSmooth = false;                                                // blocks: morph across frames with a cubic blend instead of a linear one
    void rebuildWavetable();
    rf::WaveTables tables;
    rf::Tremolo trem; rf::Panner panner; rf::PingPongDelay delay; rf::Compressor comp; rf::Crusher crush; rf::Chorus chorus; rf::Flanger flanger; rf::Reverb reverb;
    rf::Equalizer eq; rf::Tape tape; rf::Widener widener; rf::Wah wah; rf::LoFi lofi; rf::Ott ott; rf::PitchDelay pdelay; rf::TalkBox talk; rf::SpectralGate sgate; std::vector<int> leadHeld;

    rf::ModLfo lfo[2], vibLfo; int lastNote = -1; double hostBpm = 120.0;
    std::atomic<float> modWheel { 0.0f }, pitchBend { 0.0f };
    void reset() override { lfo[0].reset(); lfo[1].reset(); vibLfo.reset(); }
    void leadRetarget (int note, float vel);
    void noteOn (int note, float vel);
    void noteOff (int note);
    void renderSegment (float* L, float* R, int n);
    // Where a sample slot plays, in frames. "Trimmed" means an End point has been set; with End at its default the region is the old one
    // (forward: Start to the last frame, reverse: last frame back to the beginning, Start ignored), so existing presets render as before.
    struct SampleRegion { double startRaw = 0.0, startF = 0.0, endF = 0.0, revFloor = 0.0; bool trimmed = false; };
    SampleRegion sampleRegion (double nF, const char* startId, const char* endId) const;
    float oscSample (int shape, double phase, double hz, float pw, const rf::WaveTables::UserBank* wt) const;

    std::vector<float> fxSlot[2]; int fxActive = 0;
    using Snap = std::vector<float>;
    Snap snapshot() const; void applySnap (const Snap&); void flushPending();
    std::vector<Snap> undoS, redoS; Snap cur, pending; int quiet = 0; juce::Random rng;
    std::array<std::atomic<float>*, 16> rhythmTypes {}, rhythmVelocities {}; std::array<std::atomic<float>*, 64> patSteps {}; std::array<std::atomic<float>*, 7> degShapes {};
    std::array<std::atomic<float>*, 64> evMask {}, evLen {}, evVel {};      // Events mode, one per step
    std::array<std::atomic<float>*, 15> chopPts {};
    std::array<std::array<std::atomic<float>*, 32>, 2> lfoPts {};      // the drawn LFO cycles (rf::kLfoPts points each)
    // Automation lanes (see Dsp/Automation.h): cached parameters, this block's settings and the per-target modulation buffers
    struct AutoPtrs { std::atomic<float> *on, *src, *depth, *dst, *div, *feel, *hz, *phase, *trig, *envA, *envD; std::array<std::atomic<float>*, rf::kAutoPts> pts; };
    std::array<AutoPtrs, rf::kAutoLanes> autoP {};
    std::atomic<float> *modHeatP = nullptr, *llOn = nullptr, *llShape = nullptr, *llRate = nullptr, *llSync = nullptr, *llDepth = nullptr; std::array<std::atomic<float>*, 11> llBit {}, llAmt {};
    rf::AutoBank autoBank; rf::AutoLaneCfg autoCfg[rf::kAutoLanes]; rf::LegacyCfg legCfg; float autoPts[rf::kAutoLanes][rf::kAutoPts] {}; float heatNow = 1.0f; const float* blockL = nullptr;
    void readAuto();
    std::array<std::atomic<float>*, 16> kitSlice {}, kitPitch {}, kitRev {}, kitLevel {}, kitChoke {};
    std::atomic<float>* p (const char* id) const { return apvts.getRawParameterValue (id); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RelicProcessor)
};
