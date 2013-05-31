
#include "my32u4.h"
#include <avr/builtins.h>
#include <avr/cpufunc.h>

void MY_DelayUs(unsigned short us) {
    while (us > 0) {
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        --us;
    }
}
