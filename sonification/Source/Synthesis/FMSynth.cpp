
#include "FMSynth.h"

#include <utility>

FMSynth::~FMSynth() {
}

void FMSynth::setCarrier(FMOsc newCarrier) {
    this->carrier = std::move(newCarrier);
}

void FMSynth::prepareToPlay(double sampleRate, int samplesPerBlock, int numOutputChannels) {
    juce::dsp::ProcessSpec spec{sampleRate, static_cast<uint32>(samplesPerBlock), static_cast<uint32>(numOutputChannels)};

    this->carrier.prepareToPlay(spec);

    this->isPrepared = true;
}

void FMSynth::renderNextBlock(juce::AudioBuffer<float> &outputBuffer, int startSample, int numSamples) {
    jassert(this->isPrepared);

    // Prevent discontinuities by writing to a temp buffer...
    this->buffer.setSize(outputBuffer.getNumChannels(), numSamples, false, false, true);
    this->buffer.clear();

    this->carrier.computeNextBlock(this->buffer, 0, this->buffer.getNumSamples());

    for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel) {
        outputBuffer.addFrom(channel, startSample, this->buffer, channel, 0, numSamples);

        if (!carrier.isActive()) {
            carrier.reset();
        }
    }
}

void FMSynth::setModulationAmount(float newModAmount) {
    carrier.setModulationAmount(newModAmount);
}

void FMSynth::startNote(float freq, float amplitude) {
    carrier.setupNote(freq, amplitude);
}

void FMSynth::stopPlaying() {
    carrier.reset();
}

void FMSynth::setCarrierFrequency(float frequency) {
    carrier.setFrequency(frequency);
}
