//
// Created by Tommy Rushton on 02/06/2022.
//

#include "Utils.h"
#include <cmath>

/**
 * Calculate the modulo of a signed double against an unsigned integer; works like MATLAB for negative numbers.
 * @param a Index to check against the table size
 * @param b Size of the table
 * @return
 */
float Utils::modulo(float a, unsigned int b) {
    auto floatB = static_cast<float>(b);
    return fmodf(floatB + fmodf(a, floatB), floatB);
}

float Utils::clamp(float input, float min, float max) {
    if (input < min) {
        input = min;
    } else if (input > max) {
        input = max;
    }

    return input;
}