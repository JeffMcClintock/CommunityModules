#pragma once
#include "WithImageEffects.h"

class EmmissiveComponent : public WithImageEffects
{
#if 0 //def _DEBUG
    static const int KERNAL_SIZE = 16;
#else
    static const int KERNAL_SIZE = 48;
#endif

    static float filter[KERNAL_SIZE][KERNAL_SIZE];

    //std::unique_ptr<juce::Image> sourceImage;
    //std::unique_ptr<juce::Image> image;
    bool invalidated = true;

    static void initFilterKernal();
    void updateBitmap();
    //void paint(juce::Graphics& g) override;
    //bool hitTest(int x, int y) override;
    //void resized() override;

public:
    EmmissiveComponent() {}
#if 0
    static std::unique_ptr<juce::Image> renderEmmisiveImage(
        std::function<void(juce::Graphics&)>,
        float brightness = 0.3f,
        int sourceW = 32,
        int sourceH = 32,
        int glowWidth = -1  
    );

    virtual void paintEmmisive(juce::Graphics& g);
    void setBoundsAdj(juce::Rectangle<int> r);
    juce::Rectangle<int> getLocalBoundsAdj() const noexcept;
#endif
    int32_t filterImage(Bitmap& bitmap) override;

    void update();

    static int getBorderSize()
    {
        return KERNAL_SIZE;
    }
};
