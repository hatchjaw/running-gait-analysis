//
// Created by Tommy Rushton on 02/06/2022.
//

#include <cmath>
#include "SmoothedParameter.h"

template<typename T>
SmoothedParameter<T>::SmoothedParameter(T initialValue, T qVal) {
    target = initialValue;
    current = target;
    q = qVal;
}

template<typename T>
void SmoothedParameter<T>::set(T targetValue, bool skipSmoothing) {
    target = targetValue;
    if (skipSmoothing) {
        current = target;
    }
}

template<typename T>
T SmoothedParameter<T>::getNext() {
    if (current != target) {
        auto delta = target - current;
        auto absDelta = fabsf(delta);
        if (absDelta < THRESHOLD) {
            current = target;
        } else {
            current += q * delta;
        }
    }

    return current;
}

template<typename T>
T &SmoothedParameter<T>::getCurrent() {
    return current;
}

template
class SmoothedParameter<float>;