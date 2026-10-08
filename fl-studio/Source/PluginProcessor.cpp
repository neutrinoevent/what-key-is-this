#include "PluginProcessor.h"
#include "PluginEditor.h"

// Runs `whatkey.py <file> --json` and reports the outcome back to the processor.
class WhatKeyProcessor::Worker : public juce::Thread
{
public:
    Worker (WhatKeyProcessor& p, juce::File f) : juce::Thread ("WhatKey analysis"), owner (p), file (std::move (f)) {}

    void run() override
    {
        juce::ChildProcess proc;
        juce::StringArray args { "/usr/bin/env", "PATH=" WHATKEY_PATH, WHATKEY_PYTHON, WHATKEY_SCRIPT,
                                 file.getFullPathName(), "--json", "-n", "5" };

        if (! proc.start (args))
            return report ("error", "Couldn't start Python (" WHATKEY_PYTHON ")");

        juce::String output;
        const auto deadline = juce::Time::getMillisecondCounter() + 180000;
        char buf[4096];

        // Drain output while waiting so the pipe never fills; bail out if superseded.
        for (;;)
        {
            if (threadShouldExit()) { proc.kill(); return; }
            if (juce::Time::getMillisecondCounter() > deadline) { proc.kill(); return report ("error", "Analysis timed out"); }

            const int n = proc.readProcessOutput (buf, sizeof (buf));
            if (n > 0) output += juce::String::fromUTF8 (buf, n);
            else if (! proc.isRunning()) break;
            else wait (20);
        }

        juce::StringArray lines;
        lines.addLines (output.trim());
        lines.removeEmptyStrings();

        for (auto& line : lines)
        {
            if (line.startsWith ("{"))
            {
                auto data = juce::JSON::parse (line);
                if (data.isObject())
                {
                    Snapshot s;
                    s.mode = "result";
                    s.data = data;
                    s.fileName = file.getFileName();
                    return owner.setSnapshot (s);
                }
            }
        }
        report ("error", lines.isEmpty() ? juce::String ("Analysis failed") : lines[0]);
    }

private:
    void report (const juce::String& mode, const juce::String& text)
    {
        Snapshot s;
        s.mode = mode;
        s.text = text;
        s.fileName = file.getFileName();
        owner.setSnapshot (s);
    }

    WhatKeyProcessor& owner;
    juce::File file;
};

WhatKeyProcessor::WhatKeyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

WhatKeyProcessor::~WhatKeyProcessor()
{
    if (worker) worker->stopThread (3000);
}

bool WhatKeyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && out == layouts.getMainInputChannelSet();
}

void WhatKeyProcessor::analyze (const juce::File& file)
{
    if (worker) worker->stopThread (3000);

    lastFile = file;
    Snapshot s;
    s.mode = "busy";
    s.text = juce::String::fromUTF8 ("Analyzing ") + file.getFileName() + juce::String::fromUTF8 ("…");
    s.fileName = file.getFileName();
    setSnapshot (s);

    worker = std::make_unique<Worker> (*this, file);
    worker->startThread();
}

void WhatKeyProcessor::setSnapshot (Snapshot s)
{
    {
        const juce::ScopedLock sl (lock);
        snapshot = std::move (s);
    }
    sendChangeMessage(); // async; safe from any thread
}

WhatKeyProcessor::Snapshot WhatKeyProcessor::getSnapshot() const
{
    const juce::ScopedLock sl (lock);
    return snapshot;
}

// Remember the last file with the project and re-analyze it on load (like the Live device).
void WhatKeyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement xml ("WhatKey");
    xml.setAttribute ("lastFile", lastFile.getFullPathName());
    copyXmlToBinary (xml, dest);
}

void WhatKeyProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;

    const juce::File f (xml->getStringAttribute ("lastFile"));
    if (f.existsAsFile() && f != lastFile)
        analyze (f);
}

juce::AudioProcessorEditor* WhatKeyProcessor::createEditor() { return new WhatKeyEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WhatKeyProcessor(); }
