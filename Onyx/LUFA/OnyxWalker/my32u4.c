
#include "my32u4.h"
#include "Ada1306.h"
#include <avr/builtins.h>
#include <avr/cpufunc.h>
#include <avr/io.h>


void MY_SetLed(unsigned char led, unsigned char state) {
    if (state) {
        PORTF |= (1 << (5 + led));
    }
    else {
        PORTF &= ~(1 << (5 + led));
    }
}

void MY_SetLedAll(unsigned char state) {
    if (state) {
        PORTF |= 0xe0;
    }
    else {
        PORTF &= ~0xe0;
    }
}

void MY_Setup(void) {
    DDRF |= 0xe0;                           //  status LEDs

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

unsigned short MY_DelayTicks(unsigned short togo) {
    unsigned short start = TCNT1;
    while (1) {
        unsigned short d = TCNT1 - start;
        if (d >= togo) {
            break;
        }
        start += d;
        togo -= d;
    }
}

void MY_Failure(char const *txt, unsigned char a, unsigned char b) {
    LCD_Clear();
    LCD_DrawString("FAILURE", 1, 0, 0);
    LCD_DrawString(txt, 1, 1, 0);
    LCD_DrawUint(a, 1, 2);
    LCD_DrawUint(b, 10, 2);
    LCD_Flush();

    //  after a while, watchdog will clean this up
    unsigned char st = 1;
    while (1) {
        MY_SetLedAll(st);
        MY_DelayTicks(10000);
        st = !st;
    }
}

