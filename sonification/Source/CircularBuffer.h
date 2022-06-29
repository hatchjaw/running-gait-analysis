//
// Created by Tommy Rushton on 31/05/2022.
//

#ifndef GAIT_SONIFICATION_CIRCULARBUFFER_H
#define GAIT_SONIFICATION_CIRCULARBUFFER_H

#include <vector>

template<typename T>
class CircularBuffer {
public:
    explicit CircularBuffer(unsigned int bufferLength, T init);

    void clear();

    void write(T);

    T getCurrent();

    T getPrevious(unsigned int delay = 1);

    std::vector<T> getCircle();

    std::vector<T> getSamples(unsigned int samplesToGet = 1);

    void setReadIndex(unsigned int readPosition);

    void incrementReadIndex();

private:
    unsigned int length{0};
    std::vector<T> buffer;
    T defaultValue;
    unsigned int writeIndex{0};
};


#endif //GAIT_SONIFICATION_CIRCULARBUFFER_H
