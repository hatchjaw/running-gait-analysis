/*
  ==============================================================================

    FMVoice.h
    Created: 1 Jan 2022 8:47:31pm
    Author:  Tommy Rushton

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <utility>
#include "FMOsc.h"
#include "OADEnv.h"

class FMSynth {
public:
    struct Parameters {
        Parameters() = default;

        Parameters(FMOsc::FMMode carrierModeToUse,
                   std::vector<FMOsc::Parameters> modulatorParametersToUse,
                   OADEnv::Parameters envParamsToUse)
                : carrierMode(carrierModeToUse),
                  modulatorParameters(std::move(modulatorParametersToUse)),
                  envParams(envParamsToUse) {
        }

        FMOsc::FMMode carrierMode;
        std::vector<FMOsc::Parameters> modulatorParameters;
        OADEnv::Parameters envParams;
    };

    ~FMSynth();

    void setCarrier(FMOsc newCarrier);

    void prepareToPlay(double sampleRate, int samplesPerBlock, int numOutputChannels);

    void startNote(float freq, float amplitude);

    void renderNextBlock(juce::AudioBuffer<float> &outputBuffer, int startSample, int numSamples);

    void setModulationAmount(float newModAmount);

    void stopPlaying();

    void setCarrierFrequency(float frequency);

protected:
    FMOsc carrier;

    bool isPrepared{false};

private:
    juce::AudioBuffer<float> buffer;
};
