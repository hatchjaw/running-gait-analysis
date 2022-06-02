//
// Created by Tommy Rushton on 31/05/2022.
//

#include "GaitEventDetectorComponent.h"
#include "Utils.h"

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

        // ...but try to treat the gyro as authoritative.
        if (nextFoot == prevFoot) {
            if (canSwapFeet) {
                nextFoot = nextFoot == Foot::Left ? Foot::Right : Foot::Left;
                canSwapFeet = false;
            } else {
                canSwapFeet = true;
            }
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

        // Toe off marks the end of a ground contact. Register a ground contact
        // if there's a preceding initial contact.
        if (lastInitialContact.type == GaitEventType::InitialContact) {
            auto groundContactTime = lastToeOff.timeStampMs - lastInitialContact.timeStampMs;
//            if (groundContactTime < MAX_GCT_MS) {
            groundContacts.write({lastInitialContact, lastToeOff, groundContactTime, nextFoot});
//            }
        }
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
           isInflection(jerk.getSamples(3), InflectionType::Maximum) &&
           (elapsedTimeMs - IMU_SAMPLE_PERIOD_MS) - lastInitialContact.timeStampMs > IC_TO_INTERVAL_MS;
}

bool GaitEventDetectorComponent::isInitialContact(float currentAccelY) {
//    // Check for basic initial contact criteria -- if no previous TOs or ICs
//    // have been recorded, this could be the first IC.
//    bool couldBeInitialContact = elapsedTimeMs > 50.f &&
//                                 jerk.getCurrent() < IC_JERK_THRESH &&
//                                 currentAccelY < IC_ACCEL_THRESH;
//
//    // If a toe-off or initial-contact has been registered, add the rest of the
//    // conditions.
//    if (lastToeOff.type == GaitEventType::ToeOff || lastInitialContact.type == GaitEventType::InitialContact) {
//        couldBeInitialContact = couldBeInitialContact &&
//                                gaitPhase == GaitPhase::SwingReversal &&
//                                (elapsedTimeMs - IMU_SAMPLE_PERIOD_MS) - lastToeOff.timeStampMs > TO_IC_INTERVAL_MS;
//    }
//
//    return couldBeInitialContact;

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
    gctBalance.set(.5f, true);
    cadence.set(0.f, true);
    doneProcessing = false;
}

void GaitEventDetectorComponent::paint(Graphics &g) {
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);   // draw an outline around the component

    plotAccelerometerData(g);

    // List ground contact times
    displayGctList(g);
    // Draw GCT balance
    displayGctBalance(g);
}

void GaitEventDetectorComponent::plotAccelerometerData(Graphics &g) {
    // Draw an x-axis for the plot.
    g.drawHorizontalLine(floor(static_cast<float>(getHeight()) * ACCEL_PLOT_Y_ZERO_POSITION), 0.f,
                         static_cast<float>(getRight()));

    if (isTimerRunning()) {
        // Draw accelY
        g.setColour(Colours::lightgrey);
        g.strokePath(generateAccelYPath(), PathStrokeType(2.0f));

        // Mark events
        markEvents(g);
    }
}

juce::Path GaitEventDetectorComponent::generateAccelYPath() {
    // Get the accelerometer data.
    auto data = imuData.getSamples(PLOT_LOOKBACK);

    auto width = static_cast<float>(getWidth());
    auto height = static_cast<float>(getHeight());
    auto right = static_cast<float>(getRight()) - static_cast<float>(getX());

    // Y-axis is in the vertical middle of the component
    auto yZero = height * ACCEL_PLOT_Y_ZERO_POSITION;

    // Initialise path
    juce::Path stringPath;

    // Start path
    stringPath.startNewSubPath(right, yZero - data[0].accelY * PLOT_Y_SCALING);
    auto N = data.size();

    // Visual spacing between points.
    auto spacing = width / static_cast<float>(N - 1);
    auto x = right - spacing;

    for (unsigned long n = 1; n < N; ++n) {
        auto newY = yZero - data[n].accelY * PLOT_Y_SCALING;

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
                colour = LEFT_COLOUR;
                break;
            case Foot::Right:
                text = "R";
                colour = RIGHT_COLOUR;
                break;
            case Foot::Unknown:
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
            case GaitEventType::Unknown:
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
        g.setColour(gc.foot == Foot::Left ? LEFT_COLOUR : RIGHT_COLOUR);
        g.drawText(juce::String{gc.duration} + " ms",
                   x, top + 10, w, 20, juce::Justification::centredRight);
        gc = groundContacts.getPrevious(++z);
    }
}

void GaitEventDetectorComponent::displayGctList(Graphics &g) {
    auto x{static_cast<float>(getX())},
            y{10.f},
            h{static_cast<float>(getHeight())},
            padding{10.f},
            right{static_cast<float>(getRight())},
            columnWidth{90.f};
    // List recent contact times
    g.setColour(juce::Colours::lightgrey);
    g.drawText("GCTs", x + padding, y, columnWidth * 2, 20, juce::Justification::centred);
    y += 20;
    g.setColour(LEFT_COLOUR);
    g.drawText("Left", x + padding, y, columnWidth, 20, juce::Justification::centred);
    g.setColour(RIGHT_COLOUR);
    g.drawText("Right", x + padding + columnWidth, y, columnWidth, 20, juce::Justification::centred);
    y += 20;
    g.setColour(juce::Colours::grey);
    g.drawHorizontalLine(y, x + padding, x + padding + columnWidth * 2);
    y += 10;
    auto n = 0;
    for (auto gc: currentGroundContactInfo.groundContacts) {
        if (gc.foot != Foot::Unknown) {
            g.setColour(gc.foot == Foot::Left ? LEFT_COLOUR : RIGHT_COLOUR);
            g.drawText(juce::String{gc.duration, 2} + " ms",
                       x + 10 + (gc.foot == Foot::Left ? 0.f : columnWidth),
                       y,
                       columnWidth,
                       20,
                       juce::Justification::centred);
            y += ++n % 2 == 0 ? 15 : 0;
        }
    }

    // Display averages
    if (currentGroundContactInfo.rightAvgMs > 0 && currentGroundContactInfo.leftAvgMs > 0) {
        y = 230.f;
        g.setColour(juce::Colours::grey);
        g.drawHorizontalLine(y, x + padding, x + padding + columnWidth * 2);
        y += 10;
        g.setColour(juce::Colours::lightgrey);
        g.drawText("Mean", x + padding, y, columnWidth * 2, 20, juce::Justification::centred);
        y += 15;
        g.setColour(LEFT_COLOUR);
        g.drawText(juce::String{currentGroundContactInfo.leftAvgMs, 2} + " ms",
                   x + padding, y, columnWidth, 20,
                   juce::Justification::centred);
        g.setColour(RIGHT_COLOUR);
        g.drawText(juce::String{currentGroundContactInfo.rightAvgMs, 2} + " ms",
                   x + padding + columnWidth, y, columnWidth, 20,
                   juce::Justification::centred);
    }
}

void GaitEventDetectorComponent::displayGctBalance(Graphics &g) {
    auto w{static_cast<float>(getWidth())},
            h{static_cast<float>(getHeight())},
            padding{10.f},
            right{static_cast<float>(getRight())},
            indicatorLeft{225.f},
            indicatorWidth{right - indicatorLeft - 2.f * padding},
            indicatorRight{indicatorLeft + indicatorWidth},
            indicatorVcentre{floor(h * .25f)},
            indicatorHcentre{indicatorLeft + indicatorWidth * .5f};

    // Draw line to represent 55L, 50:50, and 55R, plus a y-axis.
    g.setColour(juce::Colours::grey);
    g.drawHorizontalLine(indicatorVcentre, indicatorLeft, indicatorRight);
    g.drawVerticalLine(indicatorLeft, indicatorVcentre - 30, indicatorVcentre + 30);
    g.drawVerticalLine(indicatorHcentre, indicatorVcentre - 40, indicatorVcentre + 40);
    g.drawVerticalLine(indicatorRight, indicatorVcentre - 30, indicatorVcentre + 30);
    g.setFont(10.);
    g.drawText("50%", indicatorHcentre - 20, indicatorVcentre - 50, 40, 10, juce::Justification::centredTop);
    g.drawText("55% L", indicatorLeft - 20, indicatorVcentre - 40, 40, 10, juce::Justification::centredTop);
    g.drawText("55% R", indicatorRight - 20, indicatorVcentre - 40, 40, 10, juce::Justification::centredTop);

    // Draw a marker to represent the GCT balance
    auto balance = gctBalance.getNext();
    auto colour = juce::Colours::lightgrey;
    if (balance > .51) {
        colour = RIGHT_COLOUR.brighter();
    } else if (balance < .49) {
        colour = LEFT_COLOUR.brighter();
    }
    // Outside 'acceptable' range.
    if (balance > .515 || balance < .485) {
        colour = colour.withSaturation(1.5);
    }
    g.setColour(colour);
    auto markerProportion = 10.f * Utils::clamp(balance - .45f, 0.f, .1f);
    g.fillRect((indicatorLeft + markerProportion * indicatorWidth) - 2.5f, indicatorVcentre - 30.f, 6.f, 60.f);

    // Display GCT as text
    juce::String foot = balance > .5f ? "R" : balance < .5f ? "L" : "";
    auto balanceToDisplay = 100.f * (fabsf(balance - .5f) + .5f);
    g.setFont(20.f);
    g.drawText(
            juce::String{balanceToDisplay, 2} + "% " + foot,
            indicatorHcentre - 50, indicatorVcentre - 100, 100, 20,
            juce::Justification::centredTop);

    // Display cadence.
    g.setColour(juce::Colours::lightgrey);
    g.setFont(14.f);
    if (isTimerRunning()) {
        g.drawText(
                "Cadence: " + juce::String{cadence.getNext(), 2} + " steps/min",
                indicatorRight - 200, 10, 200, 20,
                juce::Justification::centredRight);
    }
}

void GaitEventDetectorComponent::resized() {}

void GaitEventDetectorComponent::timerCallback() {
    currentGroundContactInfo = getGroundContactInfo();
    gctBalance.set(currentGroundContactInfo.balance);
    cadence.set(calculateCadence());
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

GaitEventDetectorComponent::GroundContactInfo GaitEventDetectorComponent::getGroundContactInfo() {
    auto nl{0}, nr{0};
    auto tl{0.f}, tr{0.f};
    auto gcs = groundContacts.getSamples(strideLookback * 2);
    // Update GCT balance
    for (auto gc: gcs) {
        if (gc.duration > 0) {
            switch (gc.foot) {
                case Foot::Left:
                    ++nl;
                    tl += gc.duration;
                    break;
                case Foot::Right:
                    ++nr;
                    tr += gc.duration;
                    break;
                case Foot::Unknown:
                    break;
            }
        }
    }

    tl = nl == 0 ? 0 : tl / nl;
    tr = nr == 0 ? 0 : tr / nr;

    return {gcs, tl, tr, tl == 0 || tr == 0 ? .5f : tr / (tl + tr)};
}

void GaitEventDetectorComponent::setStrideLookback(int numStrides) {
    strideLookback = static_cast<unsigned int>(numStrides);
}

float GaitEventDetectorComponent::getGtcBalance() {
    return gctBalance.getCurrent();
}

float GaitEventDetectorComponent::getCadence() {
    return cadence.getCurrent();
}

float GaitEventDetectorComponent::calculateCadence() {
    auto numEvents{0};
    auto totalTimeMs{0.f};
    for (auto event: gaitEvents.getSamples(strideLookback * 4)) {
        if (event.type == GaitEventType::ToeOff) {
            ++numEvents;
            totalTimeMs += event.interval;
        }
    }

    if (numEvents == 0) {
        return 0.f;
    }

    auto mean = (totalTimeMs / static_cast<float>(numEvents));
    return 60000.f / mean;
}
