//
// Created by Tommy Rushton on 31/05/2022.
//

#ifndef GAIT_SONIFICATION_GAITEVENTDETECTORCOMPONENT_H
#define GAIT_SONIFICATION_GAITEVENTDETECTORCOMPONENT_H

#include <JuceHeader.h>
#include "CircularBuffer.h"

class GaitEventDetectorComponent : public juce::Component {

public:
    static constexpr float IMU_SAMPLE_PERIOD_MS{6.75f};

    enum Foot {
        Left,
        Right
    };

    enum GaitPhase {
        Unknown,
        StanceReversal,
        SwingReversal
    };

    enum GaitEventType {
        ToeOff,
        InitialContact
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
        float time;
        int sample;
        float accelValue;
        float interval;
    };

    explicit GaitEventDetectorComponent(juce::File &file);

    bool prepareToProcess();

    void advanceClock(unsigned int ms);

    void processNextSample();

    bool isDoneProcessing() const;

    void paint(Graphics &g) override;

    void resized() override;

private:
    static constexpr unsigned int NUM_HEADER_LINES{215};
    static constexpr unsigned int TRUNK_ACCEL_X_INDEX{3};
    static constexpr unsigned int TRUNK_ACCEL_Y_INDEX{5};
    static constexpr unsigned int TRUNK_ACCEL_Z_INDEX{7};
    static constexpr unsigned int TRUNK_GYRO_X_INDEX{9};
    static constexpr unsigned int TRUNK_GYRO_Y_INDEX{11};
    static constexpr unsigned int TRUNK_GYRO_Z_INDEX{13};

    void parseImuLine();

    juce::File &captureFile;
    std::unique_ptr<juce::FileInputStream> fileStream;
    bool doneProcessing{false};
    unsigned int elapsedTimeMs{0};
    CircularBuffer<ImuSample> imuData;
    CircularBuffer<float> jerkBuffer;
    GaitPhase gaitPhase{Unknown};
    float lastLocalMinimum{0.f};
};


#endif //GAIT_SONIFICATION_GAITEVENTDETECTORCOMPONENT_H
