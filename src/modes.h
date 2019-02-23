#ifndef MODES_H
#define MODES_H

double Maj_mode[16] = {
    1, 1.125, 1.266, 1.352, 1.5, 1.688, 1.898,
    2, 2*1.125, 2*1.266, 2*1.352, 2*1.5, 2*1.688, 2*1.898,
    4
};

int Maj_mode_offsets[16] = {
    0, 2, 4, 5, 7, 9, 11,
    12, 14, 16, 17, 19, 21, 23,
    24
};

/////////

double pent_mode[16] = {
    1, 1.167, 1.333, 1.5, 1.75,
    2, 2*1.167, 2*1.333, 2*1.5, 2*1.75,
    4, 4*1.167, 4*1.333, 4*1.5, 4*1.75, 8
};

int pent_mode_offsets[16] = {
    0, 2, 4, 7, 9,
    12, 14, 16, 19, 21,
    0+24, 2+24, 4+24, 7+24, 9+24
};

/////////

int pent_min_mode_offsets[16] = { // TODO i think this is right
    0, 3, 5, 7, 10,
    0+12, 3+12, 5+12, 7+12, 10+12,
    0+24, 3+24, 5+24, 7+24, 10+24,
};

#endif
