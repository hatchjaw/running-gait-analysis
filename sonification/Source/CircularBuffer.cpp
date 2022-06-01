//
// Created by Tommy Rushton on 31/05/2022.
//

#include "CircularBuffer.h"
#include "GaitEventDetectorComponent.h"

template<typename T>
CircularBuffer<T>::CircularBuffer(unsigned int bufferLength, T init) {
    length = bufferLength;
    buffer.resize(length, init);
    defaultValue = init;
}

template<typename T>
void CircularBuffer<T>::clear() {
    std::fill(buffer.begin(), buffer.end(), defaultValue);
}

template<typename T>
void CircularBuffer<T>::write(T valueToWrite) {
    ++writeIndex;
    if (writeIndex >= length) {
        writeIndex = 0;
    }
    buffer[writeIndex] = valueToWrite;
}

template<typename T>
T CircularBuffer<T>::getCurrent() {
    return buffer[writeIndex];
}

template<typename T>
T CircularBuffer<T>::getPrevious(unsigned int delay) {
    int readIndex = static_cast<int>(writeIndex) - static_cast<int>(delay);
    // Return something sensible even if the specified delay wasn't.
    while (readIndex < 0) {
        readIndex += length;
    }
    return buffer[readIndex];
}

template<typename T>
std::vector<T> CircularBuffer<T>::getCircle() {
//    std::vector<T> out(length);
//    auto readIndex = writeIndex + 1;
//    for (unsigned int i = 0; i < length; ++i) {
//        if (readIndex >= length) {
//            readIndex = 0;
//        }
//        out[i] = buffer[readIndex];
//        ++readIndex;
//    }
    return getSamples(length);
}

template<typename T>
std::vector<T> CircularBuffer<T>::getSamples(unsigned int samplesToGet) {
    samplesToGet = std::min(samplesToGet, length);
    std::vector<T> out(samplesToGet);

    // Get samples, newest first.
    auto readIndex = writeIndex;

    for (unsigned int i = 0; i < samplesToGet; ++i) {
        out[i] = buffer[readIndex];

        if (readIndex == 0) {
            readIndex = length;
        }

        --readIndex;
    }
    return out;
}

template
class CircularBuffer<GaitEventDetectorComponent::ImuSample>;

template
class CircularBuffer<GaitEventDetectorComponent::GroundContact>;

template
class CircularBuffer<GaitEventDetectorComponent::GaitEvent>;

template
class CircularBuffer<float>;