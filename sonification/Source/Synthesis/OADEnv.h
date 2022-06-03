/*
  ==============================================================================

    OADEnv.h
    Created: 6 Jan 2022 7:09:42pm
    Author:  Tommy Rushton

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

/**
 * Onset-Attack-Decay envelope
 * TODO: support exponential curves
 */
class OADEnv {
public:
    struct Parameters {
        Parameters() = default;

        Parameters(float onsetAmplitude,
                   float attackTimeSeconds,
                   float decayTimeSeconds)
                : onset(onsetAmplitude),
                  attack(attackTimeSeconds),
                  decay(decayTimeSeconds) {
        }

        float onset = 0.0f, attack = 0.1f, decay = 0.1f;
    };

    void setParameters(const Parameters &);

    /** Starts the attack phase of the envelope. */
    void noteOn() noexcept;

    /** Returns true if the envelope is in its attack or decay stage. */
    bool isActive() const noexcept { return state != State::idle; }

    /** Returns the next sample value for an OADEnv object.

        @see applyEnvelopeToBuffer
    */
    float getNextSample() noexcept;

    /** This method will conveniently apply the next numSamples number of envelope values
        to an AudioBuffer.

        @see getNextSample
    */
    void applyEnvelopeToBuffer(juce::AudioBuffer<float> &buffer, int startSample, int numSamples);

    //==============================================================================
    /** Sets the sample rate that will be used for the envelope.

        This must be called before the getNextSample() or setParameters() methods.
    */
    void setSampleRate(double newSampleRate) noexcept;

private:
    void recalculateRates() noexcept;

    //==============================================================================
    enum class State {
        idle, attack, decay
    };
    State state = State::idle;

    double sampleRate{44100.0};
    float envelopeVal{0.0f};

    Parameters parameters;

    float attackRate{0.0f};
    float decayRate{0.0f};
};
