/** Rotary Encoder using interupts.

Usage:
- modify encoderPinA/B for which pins to read
- `setupRotEncoder(A, B);`
- implement: `encoderChange`

Modified based off: https://www.circuitsathome.com/mcu/rotary-encoder-interrupt-service-routine-for-avr-micros/
By: Keely Hill
28 Jan 2019

(I decided against a class because `attachInterrupt` needs a static func, and only would need one instance anyway.)
*/

#include <Arduino.h>

#include "pins.h"

static const int encoderPinA = ROTENC_A;
static const int encoderPinB = ROTENC_B;

/** Passes -1/1 `direction` for anti-clockwise and clockwise respectively. */
extern void encoderChange(int direction);

void setupRotEncoder();

/** Modulo that 'loops' negitive numbers around `modulo`. Like a wall clock. */
inline int32_t positive_modulo(int32_t number, int32_t modulo) {
    return (number + ((int64_t)modulo << 32)) % modulo;
}
