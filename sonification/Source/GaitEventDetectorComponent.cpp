//
// Created by Tommy Rushton on 31/05/2022.
//

#include "GaitEventDetectorComponent.h"

GaitEventDetectorComponent::GaitEventDetectorComponent(File &file) :
        captureFile(file),
        imuData(500, {0.f, 0.f}),
        jerkBuffer(3, 0.f) {
}

bool GaitEventDetectorComponent::prepareToProcess() {
    // Open the file
    fileStream = std::make_unique<juce::FileInputStream>(captureFile);

    if (!fileStream->openedOk())
        return false;

    // Get the header lines out of the way.
    for (unsigned int l = 0; l < NUM_HEADER_LINES; ++l) {
        fileStream->readNextLine();
    }

    elapsedTimeMs = 0;
    doneProcessing = false;
    return true;
}

void GaitEventDetectorComponent::advanceClock(unsigned int ms) {
    elapsedTimeMs += ms;
}

void GaitEventDetectorComponent::processNextSample() {
    parseImuLine();

    if (doneProcessing)
        return;

    jerkBuffer.write(
            (imuData.readCurrent().accelY - imuData.readPrevious().accelY) /
            (IMU_SAMPLE_PERIOD_MS * .001f)
    );
}

void GaitEventDetectorComponent::parseImuLine() {
    // Detect end of data.
    if (fileStream->isExhausted()) {
        doneProcessing = true;
        return;
    }

    auto line = fileStream->readNextLine();
    juce::StringArray fields;

    do {
        fields.add(line.upToFirstOccurrenceOf(",", false, true));
        line = line.fromFirstOccurrenceOf(",", false, true);
    } while (line != "");

    if (fields[TRUNK_ACCEL_Y_INDEX] == "") {
        doneProcessing = true;
        return;
    }

    imuData.write({
            std::stof(fields[TRUNK_ACCEL_Y_INDEX].toStdString()),
            std::stof(fields[TRUNK_GYRO_Y_INDEX].toStdString())
    });
}

bool GaitEventDetectorComponent::isDoneProcessing() const {
    return doneProcessing;
}

void GaitEventDetectorComponent::paint(Graphics &g) {
    // Draw accelY

    // Mark events

    // Draw GTC balance
}

void GaitEventDetectorComponent::resized() {
}

