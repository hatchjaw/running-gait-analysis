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

    void write(T);

    T readCurrent();

    T readPrevious(unsigned int delay = 1);

    std::vector<T> readCircle();

private:
    unsigned int length{0};
    std::vector<T> buffer;
    unsigned int writeIndex{0};
};


#endif //GAIT_SONIFICATION_CIRCULARBUFFER_H
