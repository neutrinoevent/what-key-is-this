#pragma once

#include "PluginProcessor.h"

// Mirrors the Ableton device: drop zone on the left, results panel on the right.
class WhatKeyEditor : public juce::AudioProcessorEditor,
                      public juce::FileDragAndDropTarget,
                      private juce::ChangeListener
{
public:
    explicit WhatKeyEditor (WhatKeyProcessor&);
    ~WhatKeyEditor() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { setDragHover (true); }
    void fileDragExit (const juce::StringArray&) override { setDragHover (false); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    void setDragHover (bool h) { dragHover = h; repaint(); }
    void paintDropZone (juce::Graphics&, const WhatKeyProcessor::Snapshot&);
    void paintPanel (juce::Graphics&, const WhatKeyProcessor::Snapshot&, float W, float H);

    WhatKeyProcessor& proc;
    juce::Rectangle<int> dropZone { 7, 5, 94, 159 }, panel { 106, 5, 362, 159 };
    juce::Rectangle<float> button; // Relatives toggle, in editor coordinates
    bool dragHover = false;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhatKeyEditor)
};
