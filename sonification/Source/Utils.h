//
// Created by Tommy Rushton on 02/06/2022.
//

#ifndef GAIT_SONIFICATION_UTILS_H
#define GAIT_SONIFICATION_UTILS_H


class Utils {
public:
    static float modulo(float b, unsigned int a);
    static float clamp(float input, float min, float max);
};


#endif //GAIT_SONIFICATION_UTILS_H
