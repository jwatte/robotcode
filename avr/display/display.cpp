
#define F_CPU 16000000

#include <libavr.h>
#include <lcd.h>
#include <stdio.h>

#if HAS_UTFT

info_Display g_state;

#define NUM_TEXT_ROWS 17
#define NUM_TEXT_COLS 32

struct text_row {
    unsigned char   label_size;
    unsigned char   color_scheme;
    char            text[NUM_TEXT_COLS];
};
text_row rows[NUM_TEXT_ROWS];

char const txt_Screen[] PROGMEM = "Screen";

void init_rows() {
    strcpy_p(rows[NUM_HEADER_ROWS].text, txt_Screen);
    rows[NUM_HEADER_ROWS].color_scheme = 1;
    for (unsigned char ch = 0; ch < NUM_HEADER_ROWS; ++ch) {
        rows[ch].color_scheme = 2;
    }
}

extern unsigned char const Droid_Sans_16_ascii_data[] PROGMEM;
Font TheFont(Droid_Sans_16_ascii_data);

unsigned char old_old_btns = 0;
unsigned char old_btns = 0;
unsigned char debounce = 0;

void change_btns() {
    unsigned char new_btns = old_btns & ~old_old_btns;
    if (new_btns & 1) {
    }
    if (new_btns & 2) {
    }
    if (new_btns & 4) {
    }
    if (new_btns & 8) {
    }
    old_old_btns = old_btns;
}

void read_ui(void *) {
    after(1, &read_ui, 0);
    unsigned char btns = PINC & 0xf;
    if (btns != old_btns) { debounce = 10;
        old_btns = btns;
    }
    if (debounce > 0) {
        --debounce;
        if (debounce == 0) {
            change_btns();
        }
    }
}

void dispatch_cmd(unsigned char sz, unsigned char const *d)
{
    Cmd const &cmd = *(Cmd const *)d;
    for (unsigned char off = 0; off < cmd.reg_count; ++off) {
        unsigned char p = off + cmd.reg_start;
        if (p > sizeof(info_MotorPower)) {
            fatal(FATAL_UNEXPECTED);
        }
        ((unsigned char *)&g_write_state)[p] = d[2 + off];
    }
    apply_state();
}

class MySlave : public ITWISlave {
    public:
        virtual void data_from_master(unsigned char n, void const *data) {
            dispatch_cmd(n, (unsigned char const *)data);
        }
        virtual void request_from_master(void *o_buf, unsigned char &o_size) {
            if (o_size > sizeof(g_state)) {
                o_size = sizeof(g_state);
            }
            memcpy(o_buf, &g_state, o_size);
        }
};

void setup() {
    setup_timers(F_CPU);
    //  5 is blinky light (digital 13)
    //  3 is reset (digital 11)
    DDRB |= (1 << 5) + (1 << 3);
    PORTB |= (1 << 5) + (1 << 3);
    udelay(100);
    PORTB &= ~(1 << 3);
    udelay(100);
    PORTB |= (1 << 3);
    LCD::init();
    PORTB &= ~(1 << 5);
    LCD::clear(0);

    //  portc0-3 used for input switches
    DDRC &= 0x0f;

    read_ui(0);
    start_twi_slave(&twiSlave, NodeDisplay);
}

#endif

