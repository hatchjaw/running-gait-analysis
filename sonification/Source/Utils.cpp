//
// Created by Tommy Rushton on 02/06/2022.
//

#include "Utils.h"

float Utils::clamp(float input, float min, float max) {
    if (input < min) {
        input = min;
    } else if (input > max) {
        input = max;
    }

    return input;
}