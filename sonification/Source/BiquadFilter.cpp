//
// Created by Tommy Rushton on 31/05/2022.
//

#include "BiquadFilter.h"

BiquadFilter::BiquadFilter(double b0, double b1, double b2, double a1, double a2) {
    setCoefficients(b0, b1, b2, a1, a2);
}

void BiquadFilter::setCoefficients(double b0, double b1, double b2, double a1, double a2) {
    B0 = b0;
    B1 = b1;
    B2 = b2;
    A1 = a1;
    A2 = a2;
}

float BiquadFilter::processSample(float inSample) {
    // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] + a1*y[n-1] + a2*y[n-2]
    auto outSample = static_cast<float>(
            B0 * inSample +
            B1 * ffBuffer.getCurrent() +
            B2 * ffBuffer.getPrevious() +
            A1 * fbBuffer.getCurrent() +
            A2 * fbBuffer.getPrevious()
    );

    ffBuffer.write(inSample);
    fbBuffer.write(outSample);

    return outSample;
}

void BiquadFilter::reset() {
    ffBuffer.clear();
    fbBuffer.clear();
}
