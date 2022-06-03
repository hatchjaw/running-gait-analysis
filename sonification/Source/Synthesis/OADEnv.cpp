/*
  ==============================================================================

    OADEnv.cpp
    Created: 6 Jan 2022 7:09:42pm
    Author:  Tommy Rushton

  ==============================================================================
*/

#include "OADEnv.h"

float OADEnv::getNextSample() noexcept {
    if (state == State::idle)
        return 0.0f;

    if (state == State::attack) {
        envelopeVal += attackRate;

        if (envelopeVal >= 1.0f) {
            envelopeVal = 1.0f;
            state = State::decay;
        }
    } else if (state == State::decay) {
        envelopeVal -= decayRate;

        if (envelopeVal <= 0.0f) {
            envelopeVal = 0.0f;
            state = State::idle;
        }
    }

    return envelopeVal;
}

void OADEnv::applyEnvelopeToBuffer(juce::AudioBuffer<float> &buffer, int startSample, int numSamples) {
    jassert (startSample + numSamples <= buffer.getNumSamples());

    if (state == State::idle) {
        buffer.clear(startSample, numSamples);
        return;
    }

    auto numChannels = buffer.getNumChannels();

    while (--numSamples >= 0) {
        auto env = getNextSample();

        for (int i = 0; i < numChannels; ++i)
            buffer.getWritePointer(i)[startSample] *= env;

        ++startSample;
    }
}

void OADEnv::noteOn() noexcept {
    if (attackRate > 0.0f) {
        envelopeVal = parameters.onset;
        state = State::attack;
    } else if (decayRate > 0.0f) {
        envelopeVal = 1.0f;
        state = State::decay;
    }
}

void OADEnv::recalculateRates() noexcept {
    auto getRate = [](float distance, float timeInSeconds, double sr) {
        return timeInSeconds > 0.0f ? (float) (distance / (timeInSeconds * sr)) : -1.0f;
    };

    attackRate = getRate(1.0f, parameters.attack, sampleRate);
    decayRate = getRate(1.0f, parameters.decay, sampleRate);

    if ((state == State::attack && attackRate <= 0.0f)
        || (state == State::decay && (decayRate <= 0.0f || envelopeVal <= 0.0f))) {
        state = State::idle;
    }
}

void OADEnv::setParameters(const Parameters &newParameters) {
    jassert (sampleRate > 0.0);
    parameters = newParameters;
    recalculateRates();
}

void OADEnv::setSampleRate(double newSampleRate) noexcept {
    jassert (newSampleRate > 0.0);
    sampleRate = newSampleRate;
    recalculateRates();
}