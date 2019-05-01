/** Omniolin

*/

#include <ADC.h>  // Teensy 3.1 uncomment this line and install https://github.com/pedvide/ADC
#include <MozziGuts.h>
#include <Oscil.h> // oscillator template
#include <ADSR.h>
#include <EventDelay.h>
#include <mozzi_midi.h> // mtof
#include <mozzi_fixmath.h>

#include <MIDI.h>

// wave tables for oscillator
#include <tables/sin2048_int8.h>
#include <tables/sin8192_int8.h>
#include <tables/saw8192_int8.h>
#include <tables/triangle2048_int8.h>
#include <tables/square_no_alias_2048_int8.h>

#include "../include/major8192_int8.h"

// #include <tables/smoothsquare8192_int8.h>

#include "pins.h"
#include "midi_pitches.h"
#include "modes.h"

#include "rotEncoder.hpp"
#include "AnalogPin.cpp"

#define CONTROL_RATE 128 // powers of 2 please

// Oscil <table_size, update_rate> oscilName (wavetable), see: <tables/..._int8.h>
// Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin(SIN2048_DATA);
Oscil <SIN8192_NUM_CELLS, AUDIO_RATE> aSin(SIN8192_DATA);
Oscil <SAW8192_NUM_CELLS, AUDIO_RATE> aSaw(SAW8192_DATA);
Oscil <TRIANGLE2048_NUM_CELLS, AUDIO_RATE> aTri(TRIANGLE2048_DATA);
Oscil <2048, AUDIO_RATE> aSquare(SQUARE_NO_ALIAS_2048_DATA);

ADSR <AUDIO_RATE, AUDIO_RATE> adsrEnv;

Oscil <major8192_int8_NUM_CELLS, AUDIO_RATE> majorChord(major8192_int8_DATA);


double * modes[2] = {Maj_mode, pent_mode};
int * modeOffsets[] = {Maj_mode_offsets, pent_mode_offsets, pent_min_mode_offsets};


#define MIDI_CHANNEL (1)
#define MIDI_VELOCITY (100)

#define MAX_ATTACK_T 1000
#define MAX_DECAY_T 1000
#define MAX_SUSTAIN_L 255
#define MAX_RELEASE_T 2000

// can do more, but this is comfortable. TODO refine when in proper place
#define MAX_GRAMS_POSSIBLE_FORCE_RES (1800)

#define MIDI_CC_TREMOLO 92

/* Control Variables */

// frequency set by user, may not be playing this if sliding to it
float setFreq = 0;

uint32_t attack_T = 0;
uint32_t decay_T = 0;
uint8_t sustain_L = 250;
uint32_t release_T = 500;

int32_t encoderChangeSince = 0; // acts kind of like a counting semaphore

// current set scale
enum mode {
    maj, minor, dim, pent
} setMode = maj;

int setTonic = 7;

/* end control vars */

// this is just C2+0, C2+1, C2+2,...
int tonicKeyNote[12] = {midiC2, midiD2b, midiD2, midiE2b, midiE2, midiF2, midiG2b, midiG2, midiA2b, midiA2, midiB2b, midiB2};

float tonicKeyFreqs [] = {130.8128, 138.5913}; // TODO continue...

EventDelay repeatingNoteTest;
EventDelay delayCheckSynthParamPots;

int32_t lastEncoderButtonPressTime = 0;
void encoderButtonRisingEdgeInterupt() {
    // 200 ms "delay" for bouncy interupt
    if (millis() - lastEncoderButtonPressTime > 200 && digitalReadFast(5) == 1) {
        Serial.println("encoder button press");
        lastEncoderButtonPressTime = millis();
    }
}

void setup(){
    Serial.begin(9600);
    setupRotEncoder();
    // randSeed(); // fresh random

    startMozzi(CONTROL_RATE);

    // Initialziation for standard ADSR using 4 pots
    adsrEnv.setADLevels(255, sustain_L); // 255 for max
    adsrEnv.setAttackTime(attack_T);
    adsrEnv.setDecayTime(decay_T);
    adsrEnv.setSustainTime(86400000); // needs a value, else 0
    adsrEnv.setReleaseTime(release_T);
    // adsrEnv.setIdleTime(86400000);
    // end ADSR init

    repeatingNoteTest.set(90);
    delayCheckSynthParamPots.set(100); // 10 Hz for now

    aSin.setFreq(300);

    pinMode(5, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(5), encoderButtonRisingEdgeInterupt, RISING);

    // pinMode(A15, INPUT);
}


/** Sets frequency and activates ADSR envelope. */
void noteOn(int midiNote, float freq) {
    usbMIDI.sendNoteOn(midiNote, MIDI_VELOCITY, MIDI_CHANNEL);

    aSin.setFreq(freq);

    adsrEnv.noteOn();
}

/** Alternate note on that sets frequency based off the midi note. */
void noteOn(int midiNote) {
    Q16n16 note = midiNote;
    note <<= 16; // make left most 16 bits // TODO test this
    Q16n16 freq_fixed = Q16n16_mtof(note); // fast, still mostly accurate
    float freq = Q16n16_to_float(freq_fixed);

    noteOn(midiNote, freq);
}

/** Transmits midi note off and turns switches ADSR to release. */
void noteOff(int midiNote) {
    usbMIDI.sendNoteOff(midiNote, MIDI_VELOCITY, MIDI_CHANNEL);

    Serial.println("note off");

    adsrEnv.noteOff();
}

/** custom helper for setting standard ADSR envelope */
void updateADSRValues() {
    adsrEnv.setAttackTime(attack_T);
    adsrEnv.setDecayTime(decay_T);
    adsrEnv.setDecayLevel(sustain_L); // Mozzi calls decay level == sustain level
    adsrEnv.setReleaseTime(release_T);
}

byte adsrGain = 0; // active ADSR gain, not a target

float calcGramsOfForce(int rawAnalogValue) {
    const float VCC = 3.26; // measured voltage
    const float FSR_PULLDOWN_RESISTANCE = 3900; // measured resistance of pull down

    // Use ADC reading to calculate voltage:
    float fsrV = rawAnalogValue * VCC / 1023.0;
    // Use voltage and static resistor value to
    // calculate FSR resistance:
    float fsrR = FSR_PULLDOWN_RESISTANCE * (VCC / fsrV - 1.0);
    float fsrG = 1.0 / fsrR; // Calculate conductance
    // Break parabolic curve down into two linear slopes:

    float force;
    if (fsrR <= 600)
      force = (fsrG - 0.00075) / 0.00000032639;
    else
      force =  fsrG / 0.000000642857;

     return force;
}

AnalogPin fr (A14, true); /// force force resistor
AnalogPin sp (A15, true); /// soft potentiometer

/** Where controlls (and the changes) affect the outout*/
void updateControl(){
    fr.noiseBuffer = 10;

    fr.checkChanged();
    sp.checkChanged();

    // Debug Prints
    // int forceResistorRaw = analogRead(A14);
    // int softPotRaw = analogRead(A15); // mozzi version has trouble on this pin for some reason
    int forceResistorRaw = fr.value;
    int softPotRaw = sp.value;
    // Serial.print(forceResistorRaw);
    // Serial.print(" ");
    // Serial.println(softPotRaw);
    // Serial.println("Force: " + String(force) + " g");

    static int quanta = -1;
    static int prevQuanta = -1;

    static int force = 0;

    static int flip = 0;

    static int curMidiNote = 0;

    force = (int) calcGramsOfForce(forceResistorRaw);

    /* Soft pot quanta determine */
    softPotRaw -= 20;
    if (softPotRaw <= 0 || force < 0) {
        if (prevQuanta != -1) noteOff(curMidiNote);
        prevQuanta = -1;
    } else {
        quanta = map(softPotRaw, 0, 1023, 0, 14);
        // Serial.println(quanta);

        if (prevQuanta != quanta) {
            noteOff(curMidiNote);

            curMidiNote = tonicKeyNote[setTonic]+modeOffsets[0][quanta] + 12;
            noteOn(curMidiNote);

            prevQuanta = quanta;

        }
    }

    // TODO affect Mozzi too based on this
    int cappedForce = force > MAX_GRAMS_POSSIBLE_FORCE_RES ? MAX_GRAMS_POSSIBLE_FORCE_RES : force;

    usbMIDI.sendControlChange(MIDI_CC_TREMOLO, map(cappedForce, 0, MAX_GRAMS_POSSIBLE_FORCE_RES, 0, 127), MIDI_CHANNEL); // midi attack

    // Serial.println(map(cappedForce, 0, MAX_GRAMS_POSSIBLE_FORCE_RES, 0, 127));

    // Automatic Testing, plays up the scale
    /*
    if (repeatingNoteTest.ready()) {
        curMidiNote = tonicKeyNote[setTonic]+modeOffsets[0][quanta];
        if (flip == 0) noteOn(
                            curMidiNote
                        //    tonicKeyFreqs[0]*modes[1][quanta]
                        );
        else if (flip == 1) {
            noteOff(curMidiNote);
            quanta++; quanta %= 16;
            // aSin.setFreq((float)());
        }
        flip++;
        if (flip >= 2) flip = 0;

        repeatingNoteTest.start(); // restart repeating countdown for testing
    }
    */

    // do analog reads for the 8 pots less freqently (since they wont be changed much)
    // TODO this delay may not be needed since the teensy is probably fast enough.
    if (delayCheckSynthParamPots.ready() && true) {

        // potentiometer matrix (PM)
        static AnalogPin PM0(POT_A, true), PM1(POT_D, true), PM2(POT_S, true), PM3(POT_R, true),
                         PM4(A3, true), PM5(A2, true), PM6(A1, true), PM7(A0, true);
         // midi control codes: http://www.nortonmusic.com/midi_cc.html

         PM0.noiseBuffer = 4;
         PM1.noiseBuffer = 3;

        boolean adsrChanged = false; // stays false if no change so we can avoid extra unneeded calls

        if (PM0.checkChanged()) {
            attack_T = map(PM0.value, 0, 1023, 0, MAX_ATTACK_T);
            adsrChanged = true;
            usbMIDI.sendControlChange(73, map(PM0.value, 0, 1023, 0, 127), MIDI_CHANNEL); // midi attack
        }

        if (PM1.checkChanged()) {
            decay_T = map(PM1.value, 0, 1018, 0, MAX_DECAY_T); // for some reason this pot max is 1017
            adsrChanged = true;
            usbMIDI.sendControlChange(75, map(PM1.value, 0, 1018, 0, 127), MIDI_CHANNEL); // generic
        }

        if (PM2.checkChanged()) {
            sustain_L = map(PM2.value, 0, 1023, 0, MAX_SUSTAIN_L);
            adsrChanged = true;
            usbMIDI.sendControlChange(76, map(PM2.value, 0, 1023, 0, 127), MIDI_CHANNEL); // generic
        }

        if (PM3.checkChanged()) {
            release_T = map(PM3.value, 0, 1023, 0, MAX_RELEASE_T);
            adsrChanged = true;
            usbMIDI.sendControlChange(72, map(PM3.value, 0, 1023, 0, 127), MIDI_CHANNEL); // midi release
        }


        if (adsrChanged && true) {
            updateADSRValues(); // update envelope

            // Serial.print(attack_T); Serial.print("  ");
            // Serial.print(decay_T); Serial.print("  ");
            // Serial.print(sustain_L); Serial.print("  ");
            // Serial.print(release_T);Serial.println("  ");
        }

        // other 4 'generic' potentiometers
        if (PM4.checkChanged()) {
            usbMIDI.sendControlChange(74, map(PM4.value, 0, 1023, 0, 127), MIDI_CHANNEL);
        }

        if (PM5.checkChanged()) {
            usbMIDI.sendControlChange(77, map(PM5.value, 0, 1023, 0, 127), MIDI_CHANNEL);
        }

        if (PM6.checkChanged()) {
            usbMIDI.sendControlChange(78, map(PM6.value, 0, 1023, 0, 127), MIDI_CHANNEL);
        }

        if (PM7.checkChanged()) {
            usbMIDI.sendControlChange(79, map(PM7.value, 0, 1023, 0, 127), MIDI_CHANNEL);
        }

        delayCheckSynthParamPots.start();
    }


    /* Check and handle rotary encoder change */
    if (encoderChangeSince != 0) {

        setTonic = positive_modulo(setTonic + encoderChangeSince, 12); // (setTonic + encoderChangeSince) % 12;

        encoderChangeSince = 0; // race condition, not criticaly important here
    }

    if (adsrGain != 0) { // ADSR debug. tip: use Arduino serial plotter
        // Serial.println(gain);
    }
}


int updateAudio(){
    adsrEnv.update();

    adsrGain = adsrEnv.next(); // doing this here yields much smoother movement.
    if (adsrGain < 5) adsrGain = 0;

    int64_t ret = aSin.next() * adsrGain;
    ret >>= 8; // gain is a byte, so shift back

    // if (ret != 0) Serial.println((int)ret);
    // if (ret != 0) Serial.println((int)adsrGain);

    return (int)(ret); // return an int signal centred around 0
}

void encoderChange(int direction) {
    // called from interupt, so want to get out fast
    // actuall changed handeled outside of this
    encoderChangeSince += direction;
}


void loop(){
    // while (usbMIDI.read()){} // ignore incoming messages

    audioHook(); // required here
}
