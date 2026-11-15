#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp/WaveTables.h"
#include "Dsp/Svf.h"
#include "Dsp/Adsr.h"
#include "Dsp/Effects.h"
#include "Dsp/Effects2.h"
#include "Dsp/Mod.h"
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

struct PresetEntry { juce::String bank, name; juce::File file; };

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
    void initPreset();                                            // every control back to its default, no sample
    juce::File savePreset (const juce::String& name);             // saves to Presets/User
    juce::String getPresetName() const { return apvts.state.getProperty ("presetName", "Init").toString(); }
    juce::String getPresetBank() const { return apvts.state.getProperty ("presetBank", "").toString(); }

    // ---- sampler ----
    bool loadSampleFile (const juce::File& f, bool setMixToSample = true, bool slotB = false);      // message thread
    void clearSampleB();
    std::shared_ptr<const SampleData> getSampleB() const { const juce::SpinLock::ScopedLockType l (sampleLock); return sampleB; }
    void clearSample();
    std::shared_ptr<const SampleData> getSample() const { const juce::SpinLock::ScopedLockType l (sampleLock); return sample; }
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
        double phase[7] {}; double phaseX[2] {};   // phaseX: oscillators 2 and 3
        double samplePos = 0.0; bool sampleActive = false;
        double baseHz = 440.0; float glide = 0.0f;       // glide: semitones still to travel
        struct Grain { float pos = 0, step = 1, gl = 1, gr = 1; int age = 0, len = 0; bool on = false; };
        Grain grains[12]; float grainTimer = 0.0f; unsigned int gseed = 1;       // texture layer
        float ks[8192] {}; int ksPos = 0; float ksLp = 0.0f;                    // plucked-string model
        float bellAmp[6] {}; unsigned int nseed = 1;                            // bell model
        int zone = 0;                                                           // keyboard split: 0 none, 1 sample only, 2 synth only
        bool chop = false; double sliceA = 0.0, sliceB = 0.0;   // chop voice: slice bounds in frames
        rf::Adsr env, menv[2];                           // amp envelope; two modulation envelopes
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
    float oscSample (int shape, double phase, double hz, float pw) const;

    std::vector<float> fxSlot[2]; int fxActive = 0;
    using Snap = std::vector<float>;
    Snap snapshot() const; void applySnap (const Snap&); void flushPending();
    std::vector<Snap> undoS, redoS; Snap cur, pending; int quiet = 0; juce::Random rng;
    std::array<std::atomic<float>*, 16> rhythmTypes {}, rhythmVelocities {};
    std::atomic<float>* p (const char* id) const { return apvts.getRawParameterValue (id); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RelicProcessor)
};
