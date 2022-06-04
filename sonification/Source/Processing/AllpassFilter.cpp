/*
  ==============================================================================

    AllpassFilter.cpp
    Created: 23 Feb 2022 2:29:20pm
    Author:  Tommy Rushton

  ==============================================================================
*/

#include "AllpassFilter.h"
#include "../Utils.h"

AllpassFilter::AllpassFilter(unsigned int numChannelsToAllocate) :
        numChannels(numChannelsToAllocate),
        buffer(static_cast<int>(numChannelsToAllocate * 2), 0) {
}

void AllpassFilter::setGain(float newGain) {
    this->gain = newGain;
}

void AllpassFilter::setOrder(uint newOrder) {
    if (newOrder != this->order) {
        this->order = newOrder;
        if (newOrder > maxOrder) {
            this->buffer.setSize(static_cast<int>(numChannels * 2), static_cast<int>(newOrder), true, true, true);
            maxOrder = newOrder;
            this->writeIndex %= this->order; // maybe don't need this anymore...
        }
    }
}

float AllpassFilter::processSample(int channel, float inputSample) {
    float outSample;

    if (this->order == 0) {
        outSample = inputSample;
        this->writeIndex = 0;
    } else {
        auto readIndex = static_cast<uint>(Utils::modulo(this->writeIndex - this->order, this->order));

        // y[n] = gx[n] + x[n-N] - gy[n-N]
        outSample = this->gain * inputSample +
                    this->buffer.getSample(2 * channel, static_cast<int>(readIndex)) -
                    this->gain * this->buffer.getSample(2 * channel + 1, static_cast<int>(readIndex));

        this->buffer.setSample(2 * channel, static_cast<int>(this->writeIndex), inputSample);
        this->buffer.setSample(2 * channel + 1, static_cast<int>(this->writeIndex), outSample);
        ++this->writeIndex;
        // This really oughtn't happen due to the condition above. Race condition, maybe.
        if (this->order > 0) {
            this->writeIndex %= this->order;
        }
    }

    return outSample;
}

void AllpassFilter::processBlock(juce::dsp::AudioBlock<float> &block) {
    for (unsigned long channel = 0; channel < block.getNumChannels(); ++channel) {
        for (unsigned long sample = 0; sample < block.getNumSamples(); ++sample) {
            auto filteredSample = processSample(channel, block.getSample(channel, sample));
            block.addSample(channel, sample, filteredSample);
        }
    }
}