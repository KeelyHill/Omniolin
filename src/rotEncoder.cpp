#include "rotEncoder.hpp"

#define ENCODER_READ ( (digitalReadFast(encoderPinA)<<1) | digitalReadFast(encoderPinB) )

void handelRotaryEncoderChangeInterupt() {
    static uint8_t old_AB = 3;  //lookup table index
    static int8_t encval = 0;   //encoder value
    static const int8_t enc_states [16] PROGMEM = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};  //encoder lookup table, 0==bouncing/middle

    old_AB <<=2;  //remember previous state

    // this is the key reading of the two encoding pins
    old_AB |= ENCODER_READ; // bits 2 and 3 are pins 2 and 3

    encval += pgm_read_byte(&(enc_states[( old_AB & 0x0f )]));

    /* forward/reverse events detect */
    if( encval > 3 ) {  //four state steps
        encoderChange(-1);

        encval = 0;
    }
    else if( encval < -3 ) {  //four state steps
        encoderChange(1);
        encval = 0;
    } else if ( (old_AB & 0x3) == 3 ) {
        encval = 0; // bring in sync incase of missed transations
    } else {
        // Serial.println("nothing");
    }

}

void setupRotEncoder() {
    pinMode(encoderPinA, INPUT_PULLUP);
    pinMode(encoderPinB, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(encoderPinA), handelRotaryEncoderChangeInterupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderPinB), handelRotaryEncoderChangeInterupt, CHANGE);
}

/* Encode state lookup table
0000: 0
0001: -1
0010: 1
0011: 0
0100: 1
0101: 0
0110: 0
0111: -1
1000: -1
1001: 0
1010: 0
1011: 1
1100: 0
1101: 1
1110: -1
1111: 0
*/
