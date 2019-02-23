/** AnalogPin: for easier change detection

Example usage:

    AnalogPin apin(4);

    if (apin.checkChanged()) {
        ... = apin.value; // use
    }

*/

#include <mozzi_analog.h>

#ifndef ANALOG_PIN_CPP
#define ANALOG_PIN_CPP

struct AnalogPin {

private:
    unsigned int pinNum = 0;

public:
    int value = 0;

    boolean simpleNoiseReduction = false;
    unsigned int noiseBuffer = 2;

    AnalogPin(const unsigned int pinNum, boolean simpleNoiseReduce=false) {
        this->pinNum = pinNum;
        this->simpleNoiseReduction = simpleNoiseReduce;
    }

    boolean checkChanged() {
        int newVal = mozziAnalogRead(pinNum);
        boolean isDifferent = newVal != value;

        if (simpleNoiseReduction && abs(newVal-value) < noiseBuffer) {
            isDifferent = false;
        } else {
            value = newVal;
        }

        return isDifferent;
    }

};

#endif
