//
// Created by Tommy Rushton on 25/04/2022.
//

#ifndef GAIT_SONIFICATION_SMOOTHEDPARAMETER_H
#define GAIT_SONIFICATION_SMOOTHEDPARAMETER_H

template<typename T>
class SmoothedParameter {
public:
    explicit SmoothedParameter(T initialValue, T qVal = DEFAULT_Q);

    void set(T targetValue, bool skipSmoothing = false);

    T getNext();

    T &getCurrent();

private:
    static constexpr T DEFAULT_Q{.01f}, THRESHOLD{1e-6};
    T current, target, q;
};

#endif //GAIT_SONIFICATION_SMOOTHEDPARAMETER_H
