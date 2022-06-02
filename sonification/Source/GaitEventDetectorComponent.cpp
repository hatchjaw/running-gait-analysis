//
// Created by Tommy Rushton on 31/05/2022.
//

#include "GaitEventDetectorComponent.h"

GaitEventDetectorComponent::GaitEventDetectorComponent(File &file) :
        captureFile(file),
        imuData(500, {0.f, 0.f}),
        jerk(3, 0.f),
        gaitEvents(50, {GaitEventType::Unknown, Foot::Unknown, 0.f, 0, 0.f, 0.f}),
        groundContacts(50, {{}, {}, 0.f, Foot::Unknown}) {
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
    startTimerHz(30);

    return true;
}

void GaitEventDetectorComponent::processNextSample() {
    parseImuLine();

    if (doneProcessing) {
        stopTimer();
        return;
    }

    ++elapsedSamples;
    elapsedTimeMs += IMU_SAMPLE_PERIOD_MS;

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
                elapsedTimeMs - IMU_SAMPLE_PERIOD_MS,
                elapsedSamples - 1,
                imuData.getPrevious().accelY,
                elapsedTimeMs - IMU_SAMPLE_PERIOD_MS - lastToeOff.timeStampMs
        };

        gaitEvents.write(toeOff);
        lastToeOff = toeOff;

        gaitPhase = GaitPhase::SwingReversal;

        // Toe off marks the end of a ground contact, so register a ground
        // contact time if possible. Should be greater than zero and (probably)
        // less than three-quarters of a second.
        auto groundContactTime = lastToeOff.timeStampMs - lastInitialContact.timeStampMs;
        if (groundContactTime < MAX_GCT_MS) {
            groundContacts.write({lastInitialContact, lastToeOff, groundContactTime, nextFoot});

            auto l{0.f}, r{0.f};
            // Update GCT balance
            for (auto gc: groundContacts.getSamples(strideLookback * 2)) {
                switch (gc.foot) {
                    case Foot::Left:
                        l += gc.duration;
                        break;
                    case Foot::Right:
                        r += gc.duration;
                        break;
                }
            }

            l /= l + r;
            r = 1.f - l;
            jassert(l <= 1);
        }
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
//            end
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
}

bool GaitEventDetectorComponent::isInflection(std::vector<float> v, InflectionType type) {
    // Expect most recent first...
    switch (type) {
        case InflectionType::Minimum:
            return (v[0] > 0 && v[1] < 0) ||
                   (v[0] > 0 && v[1] == 0 && v[2] < 0);
        case InflectionType::Maximum:
            return (v[0] < 0 && v[1] > 0) ||
                   (v[0] < 0 && v[1] == 0 && v[2] > 0);
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
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);   // draw an outline around the component

    g.drawHorizontalLine(floor(static_cast<float>(getHeight()) * Y_AXIS_POSITION), 0.f,
                         static_cast<float>(getRight()));

    if (isTimerRunning()) {
        // Draw accelY
        g.setColour(Colours::lightgrey);
        g.strokePath(generateAccelYPath(), PathStrokeType(2.0f));

        // Mark events
        markEvents(g);

        // Draw GCT balance
        displayGctBalance(g);
    }
}

juce::Path GaitEventDetectorComponent::generateAccelYPath() {
    // Get the accelerometer data.
    auto data = imuData.getSamples(PLOT_LOOKBACK);

    auto width = static_cast<float>(getWidth());
    auto height = static_cast<float>(getHeight());
    auto right = static_cast<float>(getRight()) - static_cast<float>(getX());

    // Y-axis is in the vertical middle of the component
    auto yaxis = height * Y_AXIS_POSITION;

    // Initialise path
    juce::Path stringPath;

    // Start path
    stringPath.startNewSubPath(right, yaxis - data[0].accelY * PLOT_V_SCALING);
    auto N = data.size();

    // Visual spacing between points.
    auto spacing = width / static_cast<float>(N - 1);
    auto x = right - spacing;

    for (unsigned long n = 1; n < N; ++n) {
        auto newY = yaxis - data[n].accelY * PLOT_V_SCALING;

        // Prevent NaN values throwing an exception.
        if (isnan(newY)) {
            newY = 0;
        }

        stringPath.lineTo(x, newY);
        x -= spacing;
    }

    return stringPath;
}

void GaitEventDetectorComponent::markEvents(Graphics &g) {
    unsigned int z = 0;
    auto event = gaitEvents.getCurrent();
    auto width = static_cast<float>(getWidth());
    auto top = static_cast<float>(getHeight()) * .5f;
    auto bottom = static_cast<float>(getBottom());
    auto spacing = width / static_cast<float>(PLOT_LOOKBACK - 1);
    auto plotSampleLimit = static_cast<int>(elapsedSamples) - PLOT_LOOKBACK;
    while (event.type != GaitEventType::Unknown &&
           static_cast<int>(event.sampleIndex) > plotSampleLimit) {
        auto x = width - (elapsedSamples - event.sampleIndex) * spacing;
        juce::String text = "";
        juce::Colour colour;
        switch (event.foot) {
            case Foot::Left:
                text = "L";
                colour = Colours::skyblue;
                break;
            case Foot::Right:
                text = "R";
                colour = Colours::lightgreen;
                break;
        }

        switch (event.type) {
            case GaitEventType::ToeOff:
                g.setColour(colour.brighter(.2f));
                text = "TO-" + text;
                break;
            case GaitEventType::InitialContact:
                g.setColour(colour.darker(.2f));
                text = "IC-" + text;
                break;
            default:
                break;
        }
        g.drawVerticalLine(x, top, bottom);
        g.drawText(text, x + 3, top + 30, 50, 20, juce::Justification::centredLeft);
        event = gaitEvents.getPrevious(++z);
    }

    // Mark ground contact region.
    z = 0;
    auto gc = groundContacts.getCurrent();
    while (gc.duration > 0 &&
           static_cast<int>(gc.toeOff.sampleIndex) > plotSampleLimit) {
        auto x = std::max(0.f, width - (elapsedSamples - gc.initialContact.sampleIndex) * spacing);
        auto w = std::max(0.f, width - x - (elapsedSamples - gc.toeOff.sampleIndex) * spacing);
        g.setColour(juce::Colours::white.withAlpha(.025f));
        g.fillRect(x, top, w, bottom - top);
        g.setColour(gc.foot == Foot::Left ? juce::Colours::skyblue : juce::Colours::lightgreen);
        g.drawText(juce::String{gc.duration} + " ms",
                   x, top + 10, w, 20, juce::Justification::centredRight);
        gc = groundContacts.getPrevious(++z);
    }
}

void GaitEventDetectorComponent::displayGctBalance(Graphics &g) {

}

void GaitEventDetectorComponent::resized() {
}

void GaitEventDetectorComponent::timerCallback() {
    this->repaint();
}

void GaitEventDetectorComponent::stop(bool andReset) {
    stopTimer();
    if (andReset) {
        reset();
    }
}

float GaitEventDetectorComponent::getCurrentTime() const {
    return elapsedTimeMs;
}

int GaitEventDetectorComponent::getElapsedSamples() const {
    return static_cast<int>(elapsedSamples);
}
