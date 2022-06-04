#pragma once

#include <JuceHeader.h>
#include <juce_video/playback/juce_VideoComponent.h>
#include "GaitEventDetectorComponent.h"
#include "Synthesis/FMSynth.h"
#include "Processing/AllpassFilter.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent : public juce::AudioAppComponent,
                      juce::HighResolutionTimer {
public:
    static constexpr int NUM_OUTPUT_CHANNELS{2};

    enum class PlayState {
        Playing,
        Stopped
    };

    enum SonificationMode {
        SynthRhythmic = 1,
        SynthConstant,
        AudioFile
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

    void play();

    void stop();

    void selectCaptureFile();

    void changePlaybackSpeed();

    void switchPlayState(PlayState state);

    void updateSonification();

    void showOptions();

    void syncVideoToIMU();

    void hiResTimerCallback() override;

    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::TextButton openCaptureBrowserButton;
    juce::File captureFile;
    juce::Label selectedCaptureFileLabel;

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
    juce::Label asymmetryThresholdsLabel;
    juce::Slider asymmetryThresholdsSlider;

    juce::TextButton optionsButton;
    SafePointer <DialogWindow> optionsWindow;

    GaitEventDetectorComponent gaitEventDetector;

    SonificationMode sonificationMode{SonificationMode::SynthRhythmic};
    juce::Label sonificationModeLabel;
    juce::ComboBox sonificationModeSelector;
    FMSynth synth;

//    CriticalSection audioCallbackLock;
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    juce::TextButton openAudioBrowserButton;
    juce::File audioFile;
    juce::Label selectedAudioFileLabel;

    AllpassFilter allpass1{2};
    AllpassFilter allpass2{2};
    juce::dsp::Gain<float> gain;
    juce::dsp::Panner<float> panner;
    juce::dsp::Reverb reverb;

    float asymmetryThresholdLow{.515f},
            asymmetryThresholdHigh{.53f},
            carrierBasisFreq{164.81f},
            fmMultiplier{70.f},
            synthDecayTime{.2f},
            reverbAmountMultiplier{100.f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)

    void selectAudioFile();
};
