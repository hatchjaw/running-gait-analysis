#include "MainComponent.h"
#include "Utils.h"

//==============================================================================
MainComponent::MainComponent() :
        gaitEventDetector(captureFile) {
    // Make sure you set the size of the component after
    // you add any child components.
    setSize(1000, 800);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
        && !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio)) {
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
                                          [&](bool granted) {
                                              setAudioChannels(granted ? 2 : 0, NUM_OUTPUT_CHANNELS);
                                          });
    } else {
        // Specify the number of input and output channels that we want to open
        setAudioChannels(2, NUM_OUTPUT_CHANNELS);
    }

    fileChooser = std::make_unique<FileChooser>("Select a capture file",
                                                File("~/src/matlab/smc/ei"),
                                                "*.csv");

    addAndMakeVisible(openBrowserButton);
    openBrowserButton.setButtonText("Select capture file");
    openBrowserButton.addListener(this);

    addAndMakeVisible(selectedFileLabel);
    selectedFileLabel.setJustificationType(Justification::centredLeft);

    addAndMakeVisible(playButton);
    playButton.setButtonText("Play");
    playButton.addListener(this);
    playButton.setEnabled(false);

    addAndMakeVisible(stopButton);
    stopButton.setButtonText("Stop");
    stopButton.addListener(this);
    stopButton.setEnabled(false);

    addAndMakeVisible(playbackSpeedLabel);
    playbackSpeedLabel.attachToComponent(&playbackSpeedSlider, true);
    playbackSpeedLabel.setText("Data playback rate", juce::dontSendNotification);

    addAndMakeVisible(playbackSpeedSlider);
    playbackSpeedSlider.addListener(this);
    playbackSpeedSlider.setEnabled(false);
    playbackSpeedSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    playbackSpeedSlider.setNormalisableRange({.01, 3.0, .01});
    playbackSpeedSlider.setValue(1.0);
    playbackSpeedSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60,
                                        playbackSpeedSlider.getTextBoxHeight());
    playbackSpeedSlider.setTextValueSuffix("x");

    addAndMakeVisible(strideLookbackLabel);
    strideLookbackLabel.attachToComponent(&strideLookbackSlider, true);
    strideLookbackLabel.setText("#stride average", juce::dontSendNotification);

    addAndMakeVisible(strideLookbackSlider);
    strideLookbackSlider.addListener(this);
    strideLookbackSlider.setSliderStyle(juce::Slider::IncDecButtons);
    strideLookbackSlider.setNormalisableRange({1, 10, 1});
    strideLookbackSlider.setValue(4);
    strideLookbackSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, true, 40,
                                         strideLookbackSlider.getTextBoxHeight());

    addChildComponent(video);

    addAndMakeVisible(gaitEventDetector);

    addAndMakeVisible(optionsButton);
    optionsButton.setButtonText("Options");
    optionsButton.addListener(this);
}

MainComponent::~MainComponent() {
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
    openBrowserButton.removeListener(this);
    playButton.removeListener(this);
    stopButton.removeListener(this);
    optionsButton.removeListener(this);
    playbackSpeedSlider.removeListener(this);
    strideLookbackSlider.removeListener(this);
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()
    auto spec = juce::dsp::ProcessSpec{sampleRate, static_cast<uint32>(samplesPerBlockExpected), NUM_OUTPUT_CHANNELS};
    panner.setRule(juce::dsp::PannerRule::linear);
    panner.prepare(spec);

    reverb.prepare(spec);
    reverb.setParameters(juce::Reverb::Parameters{.5f, .25f, 0.f, 1.f, .5f});

    gain.prepare(spec);
    gain.setGainLinear(.5);

    auto params = FMSynth::Parameters{
            FMOsc::LINEAR,
            {
                    FMOsc::Parameters(1.4, 500., 0.1, FMOsc::EXPONENTIAL, nullptr, {
                            FMOsc::Parameters(1.4, 1.9)
                    }),
                    FMOsc::Parameters(1.35, .5)
            },
            OADEnv::Parameters(0.f, 0.05f, .2f)
    };

    FMOsc carrier{params.carrierMode};
    for (auto s: params.modulatorParameters) {
        auto m = s.generateOscillator();
        carrier.addModulator(m);
    }
    carrier.setEnvelope(params.envParams);
    synth.setCarrier(carrier);
    synth.setModulationAmount(0.f);

    synth.prepareToPlay(sampleRate, samplesPerBlockExpected, NUM_OUTPUT_CHANNELS);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) {
    juce::dsp::AudioBlock<float> block(*bufferToFill.buffer,
                                       (size_t) bufferToFill.startSample);
    switch (sonificationMode) {
        case SonificationMode::SynthConstant:
        case SonificationMode::SynthRhythmic:
            bufferToFill.clearActiveBufferRegion();
            synth.renderNextBlock(*bufferToFill.buffer, 0, bufferToFill.numSamples);
            break;
        case SonificationMode::AudioFile:
            inputSource->getNextAudioBlock(bufferToFill);
            ScopedLock audioLock(audioCallbackLock);
//            this->process (juce::dsp::ProcessContextReplacing<float> (block));
            break;
    }

    auto context = juce::dsp::ProcessContextReplacing<float>(block);
    reverb.process(context);
    panner.process(context);
    gain.process(context);
}

void MainComponent::releaseResources() {
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
}

//==============================================================================
void MainComponent::paint(juce::Graphics &g) {
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    auto bounds = getLocalBounds();
    auto font = juce::Font{};
    g.setFont(juce::Font{"Monaco", 12.f, juce::Font::plain});
    g.setColour(Colours::pink);
    g.drawText("Video: " + juce::String(video.getPlayPosition()), bounds.getRight() - 100, bounds.getBottom() - 50,
               100,
               50,
               juce::Justification::left);
    g.setColour(Colours::lightblue);
    g.drawText("IMU:   " + juce::String(gaitEventDetector.getCurrentTime() * .001 + videoOffset), getRight() - 100,
               getBottom
                       () - 80,
               100,
               50,
               juce::Justification::left);
}

void MainComponent::resized() {
    auto bounds = getLocalBounds();

    auto videoWidth = 360;
    auto padding = 5;

    openBrowserButton.setBounds(bounds.getX() + padding, bounds.getY() + padding, 150, 30);
    selectedFileLabel.setBounds(openBrowserButton.getRight(), bounds.getY() + padding, 200, 30);
    playButton.setBounds(bounds.getX() + padding, openBrowserButton.getBottom() + padding, 100, 30);
    stopButton.setBounds(playButton.getRight() + padding, openBrowserButton.getBottom() + padding, 100, 30);
    playbackSpeedSlider.setBounds(stopButton.getRight() + 140,
                                  openBrowserButton.getBottom() + padding,
                                  225,
                                  30);
    strideLookbackSlider.setBounds(playbackSpeedSlider.getRight() + 120,
                                   openBrowserButton.getBottom() + padding,
                                   125,
                                   30);
    video.setBounds(bounds.getRight() - videoWidth - padding, playButton.getBottom() + 20, videoWidth, 640);
    gaitEventDetector.setBounds(bounds.getX() + padding,
                                playButton.getBottom() + 20,
                                bounds.getWidth() - videoWidth - padding * 2,
                                video.getHeight());
    optionsButton.setBounds(bounds.getRight() - 60, padding * 2, 50, 20);
}

void MainComponent::buttonClicked(Button *button) {
    if (button == &optionsButton) {
        showOptions();
    } else if (button == &openBrowserButton) {
        switchPlayState(PlayState::Stopped);
        fileChooser->launchAsync(
                FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                [this](const FileChooser &chooser) {
                    auto csvFile = chooser.getResult();
                    if (csvFile.getFileName() != "" && csvFile.existsAsFile()) {
                        // Try to load associated video.
                        auto videoFile = File{csvFile.getParentDirectory().getFullPathName() +
                                              "/../videos/" +
                                              csvFile.getFileNameWithoutExtension() +
                                              "_480.mov"};
                        if (videoFile.existsAsFile()) {
                            video.load(videoFile);
                            video.setVisible(true);

                            selectedFileLabel.setText("File: " + csvFile.getFileName(),
                                                      NotificationType::dontSendNotification);

                            captureFile = csvFile;

                            playbackSpeedSlider.setEnabled(true);
                            switchPlayState(PlayState::Stopped);
                        } else {
                            selectedFileLabel.setText("Loaded capture data, but failed to load video.",
                                                      NotificationType::dontSendNotification);;
                        }
                    } else {
                        selectedFileLabel.setText("Failed to load capture data.",
                                                  NotificationType::dontSendNotification);
                    }
                }
        );
    } else if (button == &playButton && playButton.isEnabled()) {
        if (!gaitEventDetector.prepareToProcess()) {
            return;
        }
        auto imuTime = gaitEventDetector.getCurrentTime() * .001;
        video.setPlayPosition(videoOffset + VIDEO_NUDGE + imuTime);
        switchPlayState(PlayState::Playing);
    } else if (button == &stopButton) {
        switchPlayState(PlayState::Stopped);
    }
}

void MainComponent::showOptions() {
    DialogWindow::LaunchOptions options;
    auto deviceSelector = new AudioDeviceSelectorComponent(
            deviceManager,
            0,     // minimum input channels
            256,   // maximum input channels
            0,     // minimum output channels
            256,   // maximum output channels
            false, // ability to select midi inputs
            false, // ability to select midi output device
            true, // treat channels as stereo pairs
            false // hide advanced options
    );

    options.content.setOwned(deviceSelector);

    Rectangle<int> area(0, 0, 400, 300);

    options.content->setSize(area.getWidth(), area.getHeight());

    options.dialogTitle = "Audio Settings";
    options.dialogBackgroundColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    dialogWindow = options.launchAsync();

    if (dialogWindow != nullptr)
        dialogWindow->centreWithSize(400, 300);
}

void MainComponent::hiResTimerCallback() {
    if (!video.isPlaying()) {
        gaitEventDetector.stop();
    } else if (imuSampleTimeMs >= GaitEventDetectorComponent::IMU_SAMPLE_PERIOD_MS) {
        // Check for gait events...
        gaitEventDetector.processNextSample();

        if (gaitEventDetector.isDoneProcessing()) {
            switchPlayState(PlayState::Stopped);
            return;
        }

        // Try to keep the video in sync.
        if (gaitEventDetector.getElapsedSamples() % 10 == 0) {
            const MessageManagerLock mmLock;
            repaint();
            if (gaitEventDetector.getElapsedSamples() % 2500 == 0) {
                syncVideoToIMU();
            }
        }

        updateSonification();

        imuSampleTimeMs -= GaitEventDetectorComponent::IMU_SAMPLE_PERIOD_MS;
    }

    imuSampleTimeMs += static_cast<float>(TIMER_INCREMENT_MS * playbackSpeed);
}

void MainComponent::switchPlayState(PlayState state) {
    switch (state) {
        case PlayState::Playing:
            playButton.setEnabled(false);
            stopButton.setEnabled(true);
            imuSampleTimeMs = 0.f;
            startTimer(TIMER_INCREMENT_MS);
            video.setAudioVolume(0.25f);
            video.play();
            break;
        case PlayState::Stopped:
            playButton.setEnabled(true);
            stopButton.setEnabled(false);
            stopTimer();
            gaitEventDetector.stop(true);
            video.stop();
            videoOffset = 0.0;
            if (captureFile.existsAsFile()) {
                videoOffset = VIDEO_OFFSETS.getWithDefault(captureFile.getFileNameWithoutExtension(), 30.0);
            }
            video.setPlayPosition(videoOffset);
            synth.stopPlaying();
            synth.setModulationAmount(0.f);
            break;
    }
}

void MainComponent::sliderValueChanged(Slider *slider) {
    if (slider == &strideLookbackSlider) {
        gaitEventDetector.setStrideLookback(slider->getValue());
    }
}

void MainComponent::sliderDragEnded(Slider *slider) {
    if (slider == &playbackSpeedSlider) {
        auto speed = slider->getValue();
        video.setPlaySpeed(speed);
        playbackSpeed = static_cast<float>(speed);
        syncVideoToIMU();
    }
}

void MainComponent::syncVideoToIMU() {
    auto imuTime = gaitEventDetector.getCurrentTime() * .001;
    video.setPlayPosition(videoOffset + VIDEO_NUDGE + imuTime);
}

void MainComponent::updateSonification() {
    // Raw GCT balance, 0 (L) to 1 (R)
    auto balance = gaitEventDetector.getGtcBalance();
    // Asymmetry -50 - +50
    auto asymmetryPct = 100.f * (balance - .5f);
    // Absolute (polarisation-independent) asymmetry
    auto absAsymmetry = fabsf(asymmetryPct);

    switch (sonificationMode) {
        case SonificationMode::SynthConstant:
            break;
        case SonificationMode::SynthRhythmic: {
            auto freq = 200.f + gaitEventDetector.getCadence() * 2.f;
            auto amp = .5f;
            // Check for new note
            if (gaitEventDetector.hasEventNow(GaitEventDetectorComponent::GaitEventType::ToeOff)) {
                synth.startNote(freq, amp);
            }

            synth.setCarrierFrequency(200.f + gaitEventDetector.getCadence() * 2.f);

            auto modAmount = 2.5f * Utils::clamp(absAsymmetry - .75f, 0.f, 100.f);
            synth.setModulationAmount(modAmount);
            break;
        }
        case SonificationMode::AudioFile:
            break;
    }

    auto pan = asymmetryPct * .25f;
    panner.setPan(Utils::clamp(pan, -1.f, 1.f));

    auto reverbParams = reverb.getParameters();
    auto reverbAmount = Utils::clamp(absAsymmetry - 2.5f, 0.f, 100.f);
    reverbParams.wetLevel = reverbAmount;
    reverbParams.dryLevel = 1.f - reverbAmount;
    reverb.setParameters(reverbParams);
}
