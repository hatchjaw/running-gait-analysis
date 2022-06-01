//
// Created by Tommy Rushton on 31/05/2022.
//

#include "GaitEventDetectorComponent.h"

GaitEventDetectorComponent::GaitEventDetectorComponent(File &file) :
        captureFile(file),
        imuData(500, {0.f, 0.f}),
        jerk(3, 0.f),
        gaitEvents(50, {GaitEventType::Unknown, Foot::Unknown, 0.f, 0, 0.f, 0.f}),
        groundContacts(50, {0.f, Foot::Unknown}) {
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

    reset();

    return true;
}

void GaitEventDetectorComponent::advanceClock(float ms) {
    elapsedTimeMs += ms;
}

void GaitEventDetectorComponent::processNextSample() {
    parseImuLine();

    if (doneProcessing)
        return;

    // Calculate jerk.
    auto currentAccelY = imuData.getCurrent().accelY;
    jerk.write(
            (currentAccelY - imuData.getPrevious().accelY) /
            (IMU_SAMPLE_PERIOD_MS * .001f)
    );

    // Filter the gyro data.
    auto currentGyroY = gyroFilter.processSample(imuData.getCurrent().gyroY);

    auto j = jerk.getSamples(3);

    // Looking for the last local minimum before a toe-off...
    if (isInflection(j, InflectionType::Minimum)) {
        lastLocalMinimum = currentAccelY;
    }

    // Detect stance reversal phase -- approaching a toe-off.
    if (gaitPhase != GaitPhase::StanceReversal &&
        jerk.getCurrent() > 0 &&
        currentAccelY > STANCE_REVERSAL_WINDOW.first &&
        currentAccelY < STANCE_REVERSAL_WINDOW.second &&
        lastLocalMinimum < -1.5) {
        gaitPhase = GaitPhase::StanceReversal;
    }

    if (isToeOff()) {
        // Use sign of gyro for detecting L (+ve) vs R (-ve) foot.
        // Make sure it's not the same foot again.
        auto prevFoot = lastToeOff.foot;
        auto nextFoot = currentGyroY > 0 ? Foot::Left : Foot::Right;

        if (nextFoot == prevFoot) {
            nextFoot = nextFoot == Foot::Left ? Foot::Right : Foot::Left;
        }

        auto toeOff = GaitEvent{
                GaitEventType::ToeOff,
                nextFoot,
                elapsedTimeMs,
                elapsedSamples,
                currentAccelY,
                elapsedTimeMs - lastToeOff.timeStampMs
        };

        gaitEvents.write(toeOff);
        lastToeOff = toeOff;

        gaitPhase = GaitPhase::SwingReversal;

//        % Toe off marks the end of a ground contact, so register a ground
//        % contact time if possible. Should be greater than zero and (probably)
//                                                                    % less than three-quarters of a second.
//                groundContactTime = TOs(end, 1) - ICs(end, 1);
//        gtGCT = TOs(end, 1) - gtICs(end, 1);
//        if groundContactTime > 0 && groundContactTime < .75
//        GCTs(end+1, :) = [groundContactTime; TOs(end, 3)];
//        gtGCTs(end+1, :) = [gtGCT, TOs(end, 3)];
//
//        % Calculate GTC balance.
//        if size(GCTs, 1) > 1
//        if size(GCTs, 1) > 2*strideLookback-1
//        gcts = GCTs(end-(2*strideLookback-1):end, :);
//        gtgcts = gtGCTs(end-(2*strideLookback-1):end, :);
//        L = mean(gcts(gcts(:, 2) == 1, 1));
//        gtL = mean(gtgcts(gtgcts(:, 2) == 1, 1));
//        R = mean(gcts(gcts(:, 2) == -1, 1));
//        gtR = mean(gtgcts(gtgcts(:, 2) == -1, 1));
//        else
//        L = mean(GCTs(GCTs(:, 2) == 1, 1));
//        gtL = mean(gtGCTs(gtGCTs(:, 2) == 1, 1));
//        R = mean(GCTs(GCTs(:, 2) == -1, 1));
//        gtR = mean(gtGCTs(gtGCTs(:, 2) == -1, 1));
//        end
//                BL = L/(L+R);
//        gtBL = gtL/(gtL+gtR);
//        BR = R/(L+R);
//        gtBR = gtR/(gtL+gtR);
//
//        if doPlot
//                    subplot(236);
//        scatter(BR*100, -.1, 100), ...
//        xlim([45, 55]), ...
//        ylim([-.5, .5]), ...
//        xticks(46:54), ...
//        yticks([]), ...
//        grid on;
//        title("GCT balance, " + num2str(2*strideLookback) + " step average");
//        hold on;
//        text(50, -.2, ...
//        "Trunk only: $" + num2str(BR * 100) + "\%$", ...
//        'interpreter', 'latex', ...
//        'FontSize', 16 ...
//        );
//        scatter(gtBR*100,.1, 100);
//        text(50, .2, ...
//        "Shank ICs: $" + num2str(gtBR * 100) + "\%$", ...
//        'interpreter', 'latex', ...
//        'FontSize', 16 ...
//        );
//        xline(50);
//        hold off;
//        end
//                end
//        end
    } else if (isInitialContact(currentAccelY)) {
        auto timestamp = elapsedTimeMs - IC_LOOKBACK_SAMPS * IMU_SAMPLE_PERIOD_MS;

        auto initialContact = GaitEvent{
                GaitEventType::InitialContact,
                lastToeOff.foot == Foot::Right ? Foot::Left : Foot::Right,
                timestamp,
                elapsedSamples - IC_LOOKBACK_SAMPS,
                imuData.getPrevious(IC_LOOKBACK_SAMPS).accelY,
                timestamp - lastInitialContact.timeStampMs
        };

        gaitEvents.write(initialContact);
        lastInitialContact = initialContact;

        gaitPhase = GaitPhase::Unknown;
    }

    ++elapsedSamples;
}

bool GaitEventDetectorComponent::isInflection(std::vector<float> v, InflectionType type) {
    auto end = v.size() - 1;
    switch (type) {
        case InflectionType::Minimum:
            return (v[end] > 0 && v[end - 1] < 0) ||
                   (v[end] > 0 && v[end - 1] == 0 && v[end - 2] < 0);
        case InflectionType::Maximum:
            return (v[end] < 0 && v[end - 1] > 0) ||
                   (v[end] < 0 && v[end - 1] == 0 && v[end - 2] > 0);
    }
}

bool GaitEventDetectorComponent::isToeOff() {
    // Detect toe-off via acceleration local maximum.
    return gaitPhase == GaitPhase::StanceReversal &&
           isInflection(jerk.getSamples(3), InflectionType::Maximum);
}

bool GaitEventDetectorComponent::isInitialContact(float currentAccelY) {
    // Detect initial contact. First high negative jerk event an arbitrary
    // interval after last toe off.
    return (elapsedTimeMs - IMU_SAMPLE_PERIOD_MS) - lastToeOff.timeStampMs > TO_IC_INTERVAL_MS &&
           jerk.getCurrent() < IC_JERK_THRESH &&
           gaitPhase == GaitPhase::SwingReversal &&
           currentAccelY < IC_ACCEL_THRESH;
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

    imuData.write({std::stof(fields[TRUNK_ACCEL_Y_INDEX].toStdString()),
                   std::stof(fields[TRUNK_GYRO_Y_INDEX].toStdString())});
}

bool GaitEventDetectorComponent::isDoneProcessing() const {
    return doneProcessing;
}

void GaitEventDetectorComponent::reset() {
    gyroFilter.reset();
    elapsedTimeMs = 0.f;
    elapsedSamples = 0;
    imuData.clear();
    jerk.clear();
    gaitEvents.clear();
    lastToeOff = GaitEvent{};
    lastInitialContact = GaitEvent{};
    groundContacts.clear();
    gaitPhase = GaitPhase::Unknown;
    lastLocalMinimum = 0.f;
    doneProcessing = false;
}

void GaitEventDetectorComponent::paint(Graphics &g) {
    // Draw accelY

    // Mark events

    // Draw GTC balance
}

void GaitEventDetectorComponent::resized() {
}
