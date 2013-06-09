
#include "my32u4.h"
#include <avr/builtins.h>
#include <avr/cpufunc.h>
#include <avr/io.h>



void MY_Setup(void) {
    TCCR1B = 0;
    TCCR1A = 0;                             //  Normal mode
    TCCR1C = 0;
    TCNT1 = 0;
    TCCR1B = (1 << CS12);                   //  fCPU/256 clock
}

void MY_DelayUs(unsigned short us) {
    while (us > 0) {
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        --us;
    }
}

unsigned short MY_GetTicks(void) {
    return TCNT1;
}


