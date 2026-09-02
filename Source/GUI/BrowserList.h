#pragma once

#include "LookAndFeel.h"
#include "../Core/Types.h"

namespace pt
{

class BrowserList : public juce::Component
{
public:
    std::function<void (const foundry::LibraryItem&)> onPreview;
    std::function<void (const foundry::LibraryItem&)> onToggleFavorite;
    std::function<void (const foundry::LibraryItem&)> onReveal;
    std::function<void (const foundry::LibraryItem&)> onDelete;
    std::function<void (const foundry::LibraryItem&)> onLockFrom;
    std::function<void (const foundry::LibraryItem&)> onSendToSplicer;
    std::function<void (const foundry::LibraryItem&)> onSelectionChanged;

    BrowserList();

    void setTheme (const Theme& t);
    void setItems (std::vector<foundry::LibraryItem> items);
    void setSelectedPath (const juce::String& path);
    const foundry::LibraryItem* getSelected() const;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int kRowH = 28;

private:
    class Body : public juce::Component
    {
    public:
        explicit Body (BrowserList& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

        int rowAt (juce::Point<int> p) const noexcept;

        BrowserList& owner;
        int hover = -1;
        int dragRow = -1;
        juce::Point<int> dragStart;
        bool dragging = false;
    };

    void showMenu (int row, juce::Point<int> screen);

    juce::Viewport viewport;
    Body body { *this };
    std::vector<foundry::LibraryItem> items;
    juce::String selectedPath;
    Theme theme { getTheme() };
};

} // namespace pt
