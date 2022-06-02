//
// Created by Tommy Rushton on 31/05/2022.
//

#ifndef GAIT_SONIFICATION_GAITEVENTDETECTORCOMPONENT_H
#define GAIT_SONIFICATION_GAITEVENTDETECTORCOMPONENT_H

#include <JuceHeader.h>
#include "CircularBuffer.h"
#include "BiquadFilter.h"
#include "SmoothedParameter.h"

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

    struct GaitEvent {
        GaitEventType type;
        Foot foot;
        float timeStampMs;
        unsigned int sampleIndex;
        float accelValue;
        float interval;
    };

    struct GroundContact {
        GaitEvent initialContact;
        GaitEvent toeOff;
        float duration;
        Foot foot;
    };

    struct GroundContactInfo {
        std::vector<GroundContact> groundContacts;
        float leftAvgMs;
        float rightAvgMs;
        float balance;
    };

    explicit GaitEventDetectorComponent(juce::File &file);

    bool prepareToProcess();

    void processNextSample();

    void stop(bool andReset = false);

    void timerCallback() override;

    bool isDoneProcessing() const;

    float getCurrentTime() const;

    int getElapsedSamples() const;

    GroundContactInfo getGroundContactInfo();

    void paint(Graphics &g) override;

    void resized() override;

    void setStrideLookback(int numStrides);

    float getGtcBalance();

    float getCadence();

private:
    enum class InflectionType {
        Minimum,
        Maximum
    };

    struct ImuSample {
        float accelY, gyroY;
    };

    static constexpr unsigned int NUM_HEADER_LINES{215};
    static constexpr unsigned int TRUNK_ACCEL_X_INDEX{3};
    static constexpr unsigned int TRUNK_ACCEL_Y_INDEX{5};
    static constexpr unsigned int TRUNK_ACCEL_Z_INDEX{7};
    static constexpr unsigned int TRUNK_GYRO_X_INDEX{9};
    static constexpr unsigned int TRUNK_GYRO_Y_INDEX{11};
    static constexpr unsigned int TRUNK_GYRO_Z_INDEX{13};
    // AccelY must be in this window to detect stance reversal.
    const std::pair<float, float> STANCE_REVERSAL_WINDOW{-1.2f, -.5f};
    // Jerk threshold for initial contact detection.
    static constexpr float IC_JERK_THRESH{-12.5f};
    // Acceleration threshold for initial contact detection.
    static constexpr float IC_ACCEL_THRESH{-1.f};
    // Minimum time interval between a toe-off and the following initial contact.
    static constexpr float TO_IC_INTERVAL_MS{75.f};
    // Minimum time interval between an initial contact and the following toe-off.
    static constexpr float IC_TO_INTERVAL_MS{125.f};
    // Based one the above, initial contact happened this many samples ago:
    static constexpr int IC_LOOKBACK_SAMPS{4};
    // The number of samples to plot, and to inspect for events to plot.
    static constexpr int PLOT_LOOKBACK{150};
    static constexpr float PLOT_Y_SCALING{30.f};
    static constexpr float ACCEL_PLOT_Y_ZERO_POSITION{.66f};
    // Ground contact probably won't exceed this duration.
    static constexpr float MAX_GCT_MS{750};

    const juce::Colour LEFT_COLOUR{juce::Colours::skyblue};
    const juce::Colour RIGHT_COLOUR{juce::Colours::palegoldenrod};

    void parseImuLine();

    static bool isInflection(std::vector<float> v, InflectionType type);

    bool isToeOff();

    bool isInitialContact(float currentAccelY);

    void reset();

    void plotAccelerometerData(Graphics &g);

    juce::Path generateAccelYPath();

    void markEvents(Graphics &g);

    void displayGctList(Graphics &g);

    void displayGctBalance(Graphics &g);

    float calculateCadence();

    juce::File &captureFile;
    std::unique_ptr<juce::FileInputStream> fileStream;

    bool doneProcessing{false};
    float elapsedTimeMs{0};
    unsigned int elapsedSamples{0};

    CircularBuffer<ImuSample> imuData;
    CircularBuffer<float> jerk;

    unsigned int strideLookback{4};

    GaitPhase gaitPhase{GaitPhase::Unknown};
    float lastLocalMinimum{0.f};

    CircularBuffer<GaitEvent> gaitEvents;
    bool canSwapFeet{true};
    GaitEvent lastToeOff;
    GaitEvent lastInitialContact;
    CircularBuffer<GroundContact> groundContacts;
    GroundContactInfo currentGroundContactInfo;
    SmoothedParameter<float> gctBalance{.5f, .1f};
    SmoothedParameter<float> cadence{0.f, .1f};

    BiquadFilter gyroFilter{0.002943989366965, 0.005887978733929, 0.002943989366965,
                            1.840758682071433, -0.852534639539291};
};

#endif //GAIT_SONIFICATION_GAITEVENTDETECTORCOMPONENT_H
