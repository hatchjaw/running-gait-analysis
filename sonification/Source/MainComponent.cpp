#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent() : gaitEventDetector(captureFile) {
    // Make sure you set the size of the component after
    // you add any child components.
    setSize(1000, 800);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
        && !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio)) {
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
                                          [&](bool granted) { setAudioChannels(granted ? 2 : 0, 2); });
    } else {
        // Specify the number of input and output channels that we want to open
        setAudioChannels(2, 2);
    }

    fileChooser = std::make_unique<FileChooser>("Select a capture file",
                                                File("~/src/matlab/smc/ei"),
                                                "*.csv");

    addAndMakeVisible(openBrowserButton);
    openBrowserButton.setButtonText("Choose capture file");
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

    addChildComponent(video);
}

MainComponent::~MainComponent() {
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
    openBrowserButton.removeListener(this);
    playButton.removeListener(this);
    stopButton.removeListener(this);
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) {
    // Your audio-processing code goes here!

    // For more details, see the help for AudioProcessor::getNextAudioBlock()

    // Right now we are not producing any data, in which case we need to clear the buffer
    // (to prevent the output of random noise)
    bufferToFill.clearActiveBufferRegion();
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
}

void MainComponent::resized() {
    auto bounds = getLocalBounds();

    auto videoWidth = 360;

    openBrowserButton.setBounds(bounds.getX() + 5, bounds.getY() + 5, 150, 30);
    selectedFileLabel.setBounds(openBrowserButton.getRight(), bounds.getY() + 5, 200, 30);
    playButton.setBounds(bounds.getX() + 5, openBrowserButton.getBottom() + 5, 100, 30);
    stopButton.setBounds(playButton.getRight() + 5, openBrowserButton.getBottom() + 5, 100, 30);
    video.setBounds(bounds.getRight() - videoWidth - 5, playButton.getBottom() + 20, videoWidth, 640);
}

void MainComponent::buttonClicked(Button *button) {
    if (button == &openBrowserButton) {
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

                            switchPlayState(false);
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
        switchPlayState(true);
    } else if (button == &stopButton) {
        switchPlayState(false);
    }
}

void MainComponent::hiResTimerCallback() {
    if (imuSampleTimeMs >= GaitEventDetectorComponent::IMU_SAMPLE_PERIOD_MS) {
        auto overshoot = imuSampleTimeMs - GaitEventDetectorComponent::IMU_SAMPLE_PERIOD_MS;
        gaitEventDetector.advanceClock(-overshoot);
        // Check for gait events...
        gaitEventDetector.processNextSample();

        if (gaitEventDetector.isDoneProcessing()) {
            switchPlayState(false);
            return;
        }

        gaitEventDetector.advanceClock(static_cast<float>(getTimerInterval()) + overshoot);
        imuSampleTimeMs -= GaitEventDetectorComponent::IMU_SAMPLE_PERIOD_MS;
    } else {
        gaitEventDetector.advanceClock(static_cast<float>(getTimerInterval()));
    }

    imuSampleTimeMs += static_cast<float>(TIMER_INCREMENT_MS);
}

void MainComponent::switchPlayState(bool isPlaying) {
    playButton.setEnabled(!isPlaying);
    stopButton.setEnabled(isPlaying);

    if (isPlaying) {
        imuSampleTimeMs = 0.f;
        startTimer(TIMER_INCREMENT_MS);
        video.play();
    } else {
        stopTimer();
        video.stop();
        video.setPlayPosition(VIDEO_OFFSETS.getWithDefault(
                captureFile.getFileNameWithoutExtension(), 30.0
        ));
    }
}