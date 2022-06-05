/*
  ==============================================================================

    FMOsc.cpp
    Created: 2 Jan 2022 11:59:09am
    Author:  Tommy Rushton

  ==============================================================================
*/

#include "FMOsc.h"

FMOsc::FMOsc(FMOsc::FMMode modeToSet) : mode(modeToSet) {}

void FMOsc::addModulator(FMOsc modulatorToAdd) {
    modulators.push_back(modulatorToAdd);
}

void FMOsc::prepareToPlay(juce::dsp::ProcessSpec &spec) {
    sampleRate = spec.sampleRate;
    reset();
    envelope.setSampleRate(spec.sampleRate);

    for (auto &modulator: modulators) {
        modulator.prepareToPlay(spec);
    }
}

float FMOsc::computeNextSample() {
    auto sample = 0.f;
    auto modulation = 0.f;

    // Calculate modulation amount.
    // For 0 modulators this is an unmodulated oscillator.
    // For 1 modulator this will either be basic FM or, if the modulator has modulators of its own, series MMFM.
    // For >1 modulators this will be parallel MMFM.
    for (auto &modulator: modulators) {
        modulation += modulator.computeNextSample();
    }

    switch (mode) {
        case LINEAR:
            sample = (float) (this->amplitude *
                              std::sin(currentAngle + modulation + (feedback * modulationAmount * prevSample)));
            break;
        case EXPONENTIAL:
            sample = (float) (this->amplitude *
                              std::sin(currentAngle * pow(2, modulation) +
                                       (feedback * modulationAmount * prevSample)));
            break;
        default:
            jassertfalse;
    }

    currentAngle += angleDelta;
    prevSample = sample;
    return (envelopeEnabled ? this->envelope.getNextSample() : 1.f) * sample;
}

void FMOsc::computeNextBlock(juce::AudioBuffer<float> &buffer, int startSample, int numSamples) {
    if (angleDelta != 0.0) {
        while (--numSamples >= 0) {
            auto currentSample = this->computeNextSample();

            for (auto i = (int) buffer.getNumChannels(); --i >= 0;) {
                buffer.addSample((int) i, startSample, currentSample);
            }

            ++startSample;
        }
    }
}

void FMOsc::reset() {
    currentAngle = 0.0;

    for (auto &modulator: modulators) {
        modulator.reset();
    }
}

void FMOsc::setupNote(double frequency, float noteAmplitude) {
    amplitude = noteAmplitude;

    setFrequency(frequency);

    if (envelopeEnabled) {
        envelope.noteOn();
    }
}

bool FMOsc::isActive() {
    return !envelopeEnabled || envelope.isActive();
}

void FMOsc::setEnvelope(OADEnv::Parameters &newParams, bool forceUpdateModulators) {
    envelope.setParameters(newParams);
    for (auto &modulator: modulators) {
        if (!modulator.envelopeIsSet || forceUpdateModulators) {
            modulator.setEnvelope(newParams);
        }
    }
}

void FMOsc::setModulationAmount(float newModulationAmount) {
    modulationAmount = newModulationAmount;
    for (auto &m: modulators) {
        m.setModulationAmount(newModulationAmount);
    }
}

void FMOsc::setFrequency(float newFreq) {
    auto cyclesPerSample = newFreq / sampleRate;
    angleDelta = cyclesPerSample * juce::MathConstants<double>::twoPi;

    for (auto &m: modulators) {
        auto modFreq = m.modMode == PROPORTIONAL ? newFreq * m.modulationFrequencyRatio : m.modulationFrequency;
        // modulation index, I = d/m; d, peak deviation; m, modulation frequency;
        auto modulationIndex = amplitude * modulationAmount * (m.peakDeviation / modFreq);
        m.setupNote(modFreq, static_cast<float>(modulationIndex));
    }
}

void FMOsc::stopNote() {
    angleDelta = 0.f;
    envelope.noteOff();
    for (auto &m: modulators) {
        m.stopNote();
    }
}

void FMOsc::enableEnvelope(bool shouldEnable) {
    envelopeEnabled = shouldEnable;
    for (auto &m: modulators) {
        m.enableEnvelope(shouldEnable);
    }
}
