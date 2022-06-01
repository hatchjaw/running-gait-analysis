//
// Created by Tommy Rushton on 31/05/2022.
//

#ifndef GAIT_SONIFICATION_BIQUADFILTER_H
#define GAIT_SONIFICATION_BIQUADFILTER_H

#include "CircularBuffer.h"

class BiquadFilter {
public:
    BiquadFilter(double b0, double b1, double b2, double a1, double a2);

    void setCoefficients(double b0, double b1, double b2, double a1, double a2);

    float processSample(float inSample);

    void reset();

private:
    CircularBuffer<float> ffBuffer{3, 0.f};
    CircularBuffer<float> fbBuffer{3, 0.f};

    double B0{}, B1{}, B2{}, A1{}, A2{};
};


#endif //GAIT_SONIFICATION_BIQUADFILTER_H
