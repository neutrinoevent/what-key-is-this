#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Pass-through effect that runs whatkey.py on a dropped file in the background.
class WhatKeyProcessor : public juce::AudioProcessor,
                         public juce::ChangeBroadcaster
{
public:
    WhatKeyProcessor();
    ~WhatKeyProcessor() override;

    // What the UI shows: "idle", "busy", "error" (with text) or "result" (with parsed JSON).
    struct Snapshot
    {
        juce::String mode = "idle", text = "Drop an audio file on the left";
        juce::var data;
        juce::String fileName;
    };

    void analyze (const juce::File& file);
    Snapshot getSnapshot() const;

    bool showRelatives = false; // UI toggle, default off (not saved)

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {} // audio passes through untouched

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "WhatKey"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    class Worker;
    friend class Worker;

    void setSnapshot (Snapshot s);

    mutable juce::CriticalSection lock;
    Snapshot snapshot;
    juce::File lastFile;
    std::unique_ptr<Worker> worker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhatKeyProcessor)
};
