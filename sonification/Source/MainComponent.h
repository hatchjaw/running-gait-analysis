#pragma once

#include <JuceHeader.h>
#include <juce_video/playback/juce_VideoComponent.h>
#include "GaitEventDetectorComponent.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent : public juce::AudioAppComponent,
                      juce::Button::Listener,
                      juce::Slider::Listener,
                      juce::HighResolutionTimer {
public:
    enum class PlayState {
        Playing,
        Stopped
    };
    //==============================================================================
    MainComponent();

    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override;

    void releaseResources() override;

    //==============================================================================
    void paint(juce::Graphics &g) override;

    void resized() override;

private:
    static constexpr unsigned int TIMER_INCREMENT_MS{1};
    static constexpr float VIDEO_NUDGE{.2F};
    const juce::NamedValueSet VIDEO_OFFSETS{
            {"Normal_7_5",     31.625},
            {"Normal_10",      32.625},
            {"Normal_12_5",    30.895},
            {"Normal_15",      31.045},
            {"Horizontal_7_5", 31.625},
            {"Horizontal_10",  30.575},
            {"Vertical_12_5",  30.475},
            {"Vertical_15",    29.6}
    };

    void hiResTimerCallback() override;

    void buttonClicked(Button *button) override;

    void sliderValueChanged(Slider *slider) override;

    void sliderDragEnded(Slider *slider) override;

    void switchPlayState(PlayState state);
    //==============================================================================
    PlayState playState{PlayState::Stopped};

    juce::TextButton openBrowserButton;
    std::unique_ptr<FileChooser> fileChooser;
    juce::File captureFile;
    juce::Label selectedFileLabel;

    juce::VideoComponent video{false};
    float videoOffset{0.f};

    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::Label playbackSpeedLabel;
    juce::Slider playbackSpeedSlider;
    float imuSampleTimeMs{0.f};
    float playbackSpeed{1.f};
    juce::Label strideLookbackLabel;
    juce::Slider strideLookbackSlider;

    GaitEventDetectorComponent gaitEventDetector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
