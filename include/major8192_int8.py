## generates a Major chord sine wave table (values 0-255)


import array
import os
import textwrap
import math

PI = math.pi

def generate(outfile, tablename, tablelength, samplerate):
    fout = open(os.path.expanduser(outfile), "w")
    fout.write('#ifndef ' + tablename + '_H_' + '\n')
    fout.write('#define ' + tablename + '_H_' + '\n \n')
    fout.write('#if ARDUINO >= 100'+'\n')
    fout.write('#include "Arduino.h"'+'\n')
    fout.write('#else'+'\n')
    fout.write('#include "WProgram.h"'+'\n')
    fout.write('#endif'+'\n')
    fout.write('#include "mozzi_pgmspace.h"'+'\n \n')
    fout.write('#define ' + tablename + '_NUM_CELLS '+ str(tablelength)+'\n')
    fout.write('#define ' + tablename + '_SAMPLERATE '+ str(samplerate)+'\n \n')
    outstring = 'CONSTTABLE_STORAGE(int8_t) ' + tablename + '_DATA [] = {'
    halftable = tablelength/2
    try:
        for num in range(tablelength):
            ## range between 0 and 4 first
            x = (4*float(num)/tablelength)
            # x = (float(num)/tablelength)

            # t_x = math.sin(2*PI*x) + math.sin(2*PI*x*1.25) + math.sin(2*PI*x*1.5) + 3
            # t_x /= 3 * 2 # get range between 0 and 1

            t_x = math.sin(2*PI*x) + 1.25*math.sin(2*PI*x*1.25) + 1.5*math.sin(2*PI*x*1.5) + (1.25*1.5*2)
            t_x /= (1.25*1.5*2) * 2 # get range between 0 and 1

            scaled = int(math.floor(t_x*255.999))-128 # scale 0 and 1 to fit in int8_t

            outstring += str(scaled) + ', '
    finally:
        outstring = textwrap.fill(outstring, 80)
        outstring += '\n }; \n \n #endif /* ' + tablename + '_H_ */\n'
        fout.write(outstring)
        fout.close()
        print "wrote " + outfile

# generate("./major8192_int8.h", "major8192_int8", 8192, "8192")
generate("./major8192_int8.h", "major8192_int8", 16384, "16384")
