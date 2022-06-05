/*
  ==============================================================================

    AllpassFilter.h
    Created: 23 Feb 2022 2:29:20pm
    Author:  Tommy Rushton

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AllpassFilter {
public:
    explicit AllpassFilter(unsigned int numChannelsToAllocate);

    void setGain(float newGain);

    void setOrder(uint newOrder);

    void processBlock(juce::dsp::AudioBlock<float> &block);

    float processSample(int channel, float inputSample);

private:
    unsigned int numChannels;
    float gain{0.f};
    unsigned int order{0};
    unsigned int maxOrder{0};
    // Use even numbered channels for feedforward buffers, odd numbered for feedback.
    juce::AudioBuffer<float> buffer;
    std::vector<unsigned int> writeIndices;
};