//
// Created by Tommy Rushton on 31/05/2022.
//

#ifndef GAIT_SONIFICATION_GAITEVENTDETECTORCOMPONENT_H
#define GAIT_SONIFICATION_GAITEVENTDETECTORCOMPONENT_H

#include <JuceHeader.h>
#include "CircularBuffer.h"
#include "BiquadFilter.h"

class GaitEventDetectorComponent : public juce::Component, juce::Timer {

public:
    static constexpr float IMU_SAMPLE_PERIOD_MS{6.75f};

    enum class Foot {
        Unknown,
        Left,
        Right
    };

    enum class GaitPhase {
        Unknown,
        // Phase between an initial contact and the next toe off.
        StanceReversal,
        // Phase between a toe off and the next initial contact.
        SwingReversal
    };

    enum class GaitEventType {
        Unknown,
        ToeOff,
        InitialContact
    };

    enum class InflectionType {
        Minimum,
        Maximum
    };

    struct ImuSample {
        float accelY, gyroY;
    };

    struct GroundContact {
        float duration;
        Foot foot;
    };

    struct GaitEvent {
        GaitEventType type;
        Foot foot;
        float timeStampMs;
        unsigned int sample;
        float accelValue;
        float interval;
    };

    explicit GaitEventDetectorComponent(juce::File &file);

    bool prepareToProcess();

    void advanceClock(float ms);

    void processNextSample();

    bool isDoneProcessing() const;

    void paint(Graphics &g) override;

    void resized() override;

    void timerCallback() override;

    void stop();

private:
    static constexpr unsigned int NUM_HEADER_LINES{215};
    static constexpr unsigned int TRUNK_ACCEL_X_INDEX{3};
    static constexpr unsigned int TRUNK_ACCEL_Y_INDEX{5};
    static constexpr unsigned int TRUNK_ACCEL_Z_INDEX{7};
    static constexpr unsigned int TRUNK_GYRO_X_INDEX{9};
    static constexpr unsigned int TRUNK_GYRO_Y_INDEX{11};
    static constexpr unsigned int TRUNK_GYRO_Z_INDEX{13};
    // AccelY must be in this window to detect stance reversal.
    const std::pair<float, float> STANCE_REVERSAL_WINDOW{-1.f, -.5f};
    // Jerk threshold for initial contact detection.
    static constexpr float IC_JERK_THRESH{-12.5f};
    // Acceleration threshold for initial contact detection.
    static constexpr float IC_ACCEL_THRESH{-1.f};
    // Minimum time interval between a toe-off and the following initial contact.
    static constexpr float TO_IC_INTERVAL_MS{75.f};
    // Based one the above, initial contact happened this many samples ago:
    static constexpr int IC_LOOKBACK_SAMPS = 4;

    void parseImuLine();

    static bool isInflection(std::vector<float> v, InflectionType type);
    bool isToeOff();
    bool isInitialContact(float currentAccelY);
    void reset();
    juce::Path generateAccelYPath();

    juce::File &captureFile;
    std::unique_ptr<juce::FileInputStream> fileStream;
    bool doneProcessing{false};
    float elapsedTimeMs{0};
    unsigned int elapsedSamples{0};
    CircularBuffer<ImuSample> imuData;
    CircularBuffer<float> jerk;
    CircularBuffer<GaitEvent> gaitEvents;
    GaitEvent lastToeOff;
    GaitEvent lastInitialContact;
    CircularBuffer<GroundContact> groundContacts;
    GaitPhase gaitPhase{GaitPhase::Unknown};
    float lastLocalMinimum{0.f};
    BiquadFilter gyroFilter{0.002943989366965, 0.005887978733929, 0.002943989366965, 1.840758682071433, -0.852534639539291};
};


#endif //GAIT_SONIFICATION_GAITEVENTDETECTORCOMPONENT_H
