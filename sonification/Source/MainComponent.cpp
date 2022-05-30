#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent() {
    // Make sure you set the size of the component after
    // you add any child components.
    setSize(800, 600);

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
}

void MainComponent::buttonClicked(Button *button) {
    if (button == &openBrowserButton) {
        fileChooser->launchAsync(
                FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                [this](const FileChooser &chooser) {
                    auto file = chooser.getResult();
                    if (file.getFileName() != "") {
                        captureFile = file;
                        selectedFileLabel.setText("File: " + captureFile.getFileName(),
                                                  NotificationType::dontSendNotification);
                    }
                }
        );
    }
}
