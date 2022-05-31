#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent() {
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

    openBrowserButton.setBounds(bounds.getX() + 5, bounds.getY() + 5, 150, 30);
    selectedFileLabel.setBounds(openBrowserButton.getRight(), bounds.getY() + 5, 200, 30);
    playButton.setBounds(bounds.getX() + 5, openBrowserButton.getBottom() + 5, 100, 30);
    stopButton.setBounds(playButton.getRight() + 5, openBrowserButton.getBottom() + 5, 100, 30);
    video.setBounds(bounds.getX() + 5, playButton.getBottom() + 20, 360, 640);
}

void MainComponent::buttonClicked(Button *button) {
    if (button == &openBrowserButton) {
        fileChooser->launchAsync(
                FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                [this](const FileChooser &chooser) {
                    auto file = chooser.getResult();
                    if (file.getFileName() != "" && file.existsAsFile()) {
                        // Try to load associated video.
                        auto videoFile = File{file.getParentDirectory().getFullPathName() +
                                              "/../videos/" +
                                              file.getFileNameWithoutExtension() +
                                              "_480.mov"};
                        if (videoFile.existsAsFile()) {
                            captureFile = file;

                            video.load(videoFile);
                            video.setVisible(true);

                            selectedFileLabel.setText("File: " + captureFile.getFileName(),
                                                      NotificationType::dontSendNotification);

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
        // Open the file
        fileStream = std::make_unique<juce::FileInputStream>(captureFile);

        if (!fileStream->openedOk())
            return;

        // Get the header lines out of the way.
        for (unsigned int l = 0; l < NUM_HEADER_LINES; ++l) {
            fileStream->readNextLine();
        }
        switchPlayState(true);
    } else if (button == &stopButton) {
        switchPlayState(false);
    }
}

void MainComponent::hiResTimerCallback() {
    if (currentTimeMs >= IMU_SAMPLE_PERIOD_MS) {
        auto data = parseLine();

        // Detect end of data.
        if (data[TRUNK_ACCEL_Y_INDEX] == "") {
            switchPlayState(false);
            return;
        }

        // Check for gait events...

        currentTimeMs -= IMU_SAMPLE_PERIOD_MS; //0 - (currentTimeMs - IMU_SAMPLE_PERIOD)
    }

    currentTimeMs += static_cast<float>(TIMER_INCREMENT_MS);
}

juce::StringArray MainComponent::parseLine() {
    if (fileStream->isExhausted())
        return {""};

    auto line = fileStream->readNextLine();
    juce::StringArray fields;

    do {
        fields.add(line.upToFirstOccurrenceOf(",", false, true));
        line = line.fromFirstOccurrenceOf(",", false, true);
    } while (line != "");

    return fields;
}

void MainComponent::switchPlayState(bool isPlaying) {
    playButton.setEnabled(!isPlaying);
    stopButton.setEnabled(isPlaying);

    if (isPlaying) {
        currentTimeMs = 0.f;
        startTimer(TIMER_INCREMENT_MS);
        video.play();
    }
    else {
        stopTimer();
        video.stop();
        video.setPlayPosition(VIDEO_OFFSETS.getWithDefault(
                captureFile.getFileNameWithoutExtension(), 30.0
        ));
    }
}