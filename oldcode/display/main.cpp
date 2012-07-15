
#define F_CPU 20000000L

#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include "libavr.h"


#define PORT_CTL PORTD
#define DDR_CTL DDRD
#define PORT_DATA PORTC
#define DDR_DATA DDRC

#define PIN_RS 0x1      //  PD0
#define PIN_LED 0x20    //  PD5
#define ROW0 0x2        //  PD1
#define ROW1 0x8        //  PD3

#define LCD_SD 2
#define LCD_CMDD 50
#define MODE_WAIT 150
#define RESET_WAIT 4500
#define CLEAR_WAIT 1600

static unsigned char _lcd_rows = 1;
static unsigned char _lcd_cols = 8;
#define UC unsigned char

static inline void lcd_rs(unsigned char val) {
    if (val) {
        PORT_CTL |= PIN_RS;
    }
    else {
        PORT_CTL &= ~PIN_RS;
    }
}

static inline void lcd_e(unsigned char val, bool on) {
    if (on) {
        PORT_CTL |= val;
    }
    else {
        PORT_CTL &= ~val;
    }
}

static void lcd_4bits(unsigned char e, unsigned char val) {
    PORT_DATA = (PORT_DATA & 0xf0) | (val & 0x0f);
    lcd_e(e, true);
    udelay(LCD_SD);
    lcd_e(e, false);
    udelay(LCD_SD);
}

static void lcd_cmd(unsigned char e, unsigned char rs, unsigned char cmd) {
    lcd_rs(rs);
    lcd_4bits(e, cmd >> 4);
    lcd_4bits(e, cmd);
    udelay(LCD_CMDD);
}

static unsigned char lcd_calc_addr(unsigned char row, unsigned char col, unsigned char &e) {
    if (_lcd_cols > 24) {
        e = (row & 2) ? ROW1 : ROW0;
        return ((row & 1) ? 0x40 : 0) + col;
    }
    e = ROW0;
    if (_lcd_rows > 2) {
        return ((row & 1) ? _lcd_cols : 0) + ((row & 2) ? 0x40 : 0) + col;
    }
    return (row ? 0x40 : 0) + col;
}

void lcd_output(unsigned char row, unsigned char col, char const *text, size_t len = -1) {
    if (len == (size_t)-1) {
        len = strlen(text);
    }
    unsigned char e;
    unsigned char addr = lcd_calc_addr(row, col, e);
    lcd_cmd(e, 0, 0x80 | (addr & 0x7f));
    while (len > 0) {
        lcd_cmd(e, 1, *text);
        ++text;
        --len;
    }
}

unsigned char _lcd_stroke = ROW0;

void lcd_enable(bool on) {
    lcd_cmd(_lcd_stroke, 0, on ? 0x0C : 0x08);
}

void lcd_clear() {
    lcd_cmd(_lcd_stroke, 0, 0x01);
    udelay(CLEAR_WAIT);
}

void lcd_init(unsigned char rows, unsigned char cols) {
    _lcd_rows = rows;
    _lcd_cols = cols;
    
    if (_lcd_cols > 24) {
        _lcd_stroke = ROW0 + ROW1;
    }

    lcd_rs(0);

    lcd_4bits(_lcd_stroke, 0x03);    //  first, reset to 8 bits
    udelay(RESET_WAIT);

    lcd_4bits(_lcd_stroke, 0x03);    //  re-write 8 bits interface
    udelay(RESET_WAIT);        //  wait for more than 100 us

    lcd_4bits(_lcd_stroke, 0x03);    //  third  time's the charm, says the data sheet
    udelay(RESET_WAIT);        //  wait for more than 100 us

    lcd_4bits(_lcd_stroke, 0x02);    //  re-write 4 bits interface using 8 bits interface
    udelay(RESET_WAIT);        //  wait for more than 100 us

    lcd_cmd(_lcd_stroke, 0, rows > 1 ? 0x28 : 0x20); //  mode, with correct row count
    lcd_cmd(_lcd_stroke, 0, 0xC);    //  display off
    lcd_clear();
    lcd_cmd(_lcd_stroke, 0, 0x6);    //  cursor increment, no scroll
}

void boop(unsigned char n) {
    while (n > 0) {
        PORT_CTL |= PIN_LED;
        delay(10);
        PORT_CTL &= ~PIN_LED;
        delay(100);
        --n;
    }
    delay(200);
}

void fatal_blink(bool on) {
    DDR_CTL |= PIN_LED;
    if (on) {
        PORT_CTL |= PIN_LED;
    }
    else {
        PORT_CTL &= ~PIN_LED;
    }
}

class MySlave : public ITWISlave {
public:
    virtual void data_from_master(unsigned char sz, void const *ptr) {
        lcd_output(1, 0, "X");
        if (sz < 3) {
            //  actually, could be different commands
            lcd_clear();
        }
        else if (sz < 32) {
            unsigned char const *s = (unsigned char const *)ptr;
            lcd_output(s[0], s[1], (char const *)ptr + 2, sz-2);
        }
        lcd_output(2, 0, "Y");
    }
    virtual void request_from_master(void *o_buf, unsigned char &o_size) {
        fatal(FATAL_TWI_UNEXPECTED);
    }
};

MySlave twiSlave;

void unflicker(void *) {
    PORT_DATA &= ~PIN_LED;
}

void flicker(void *) {
    after(3000, flicker, 0);
    after(5, unflicker, 0);
    PORT_DATA |= PIN_LED;
}

void lcd_clear2(void *) {
    lcd_clear();
}

void setup() {
    fatal_set_blink(fatal_blink);
    setup_timers(F_CPU);
    DDR_CTL |= (PIN_LED | PIN_RS | ROW0 | ROW1);
    PORT_CTL &= ~(PIN_LED | PIN_RS | ROW0 | ROW1);
    DDR_DATA |= 0xf;
    PORT_DATA &= ~0xf;
    boop(2);
    delay(100);
    lcd_init(4, 40);
    lcd_enable(true);
    start_twi_slave(&twiSlave, 0x11); /* teletext decoder -- won't collide */
    after(3000, flicker, 0);
    lcd_output(0, 0, "F/w " __DATE__ " " __TIME__);
    after(5000, lcd_clear2, 0);
    boop(3);
}
