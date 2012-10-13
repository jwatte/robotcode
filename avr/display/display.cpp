
#define F_CPU 16000000

#include <libavr.h>
#include <lcd.h>
#include <stdio.h>

#if HAS_UTFT

info_Display g_state;

#define NUM_TEXT_ROWS 17
#define NUM_TEXT_COLS 32

struct text_row {
    char            text[NUM_TEXT_COLS];
};
text_row rows[NUM_TEXT_ROWS];

void init_rows() {
    memset(rows, 32, sizeof(rows));
}

extern unsigned char const Droid_Sans_16_ascii_data[] PROGMEM;
Font TheFont(Droid_Sans_16_ascii_data);

unsigned char old_old_btns = 0;
unsigned char old_btns = 0;
unsigned char debounce = 0;
unsigned short front_color = 0xffff;
unsigned short back_color = 0x0;

void change_btns() {
    old_old_btns = old_btns;
    g_state.counter++;
    g_state.buttons = old_btns;
}

void read_ui(void *) {
    after(1, &read_ui, 0);
    unsigned char btns = PINC & 0xf;
    if (btns != old_btns) {
        debounce = 10;
        old_btns = btns;
    }
    if (debounce > 0) {
        --debounce;
        if (debounce == 0) {
            change_btns();
        }
    }
}

void set_colors(cmd_SetColors const &cmd) {
    front_color = cmd.front;
    back_color = cmd.back;
}

void draw_text(cmd_DrawText const &cmd) {
    LCD::text(cmd.x, (cmd.y << 1) | ((cmd.len >> 7) & 1),
        front_color, back_color, cmd.text, cmd.len, TheFont);
}

void fill_rect(cmd_FillRect const &cmd) {
    LCD::fill_rect(cmd.x, (cmd.y << 1) | ((cmd.flags >> 7) & 1),
        cmd.w, (cmd.h << 1) | ((cmd.flags >> 6) & 1),
        (cmd.flags & fillFlagFront) ? front_color : back_color);
}

void dispatch_cmd(unsigned char sz, unsigned char const *d) {
    switch (d[0]) {
        case cmdSetColors:  set_colors((cmd_SetColors const &)*d);  break;
        case cmdDrawText:   draw_text((cmd_DrawText const &)*d);    break;
        case cmdFillRect:   fill_rect((cmd_FillRect const &)*d);    break;
        default:            fatal(FATAL_UI_BAD_PARAM);              break;
    }
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
    init_rows();
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

