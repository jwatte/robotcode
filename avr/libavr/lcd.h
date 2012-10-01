/* Functions for using LCD controllers without Arduino support. */
/* Copyright 2012 Jon Watte (http://github.com/jwatte/) */

#if !defined(avr_lcd_h)
#define avr_lcd_h

#include <avr/pgmspace.h>

/*
   This interface uses the 8-bit bus, and maps the signals as
   follows:

   DB8-15:   PORTD0-7   //  this eats the UART
   RS:       PORTB0
   WR:       PORTB1
   CS:       PORTB2
   RD:       Tie high

   RST:      Tie to AVR /RESET pin

   The idea is to keep the following pins free:
   - XTAL (PB6/7)
   - SPI  (PB3/4/5)
   - I2C  (PC4/5)

   All the analog pins are free.
   SPI can be used for both TFT and Touch, if suitable chip select pins
   are allocated.
   Also, the data bus can be used for something else while LCD_CS is high.

*/

enum {
    LCD_RS = 1,
    LCD_WR = 2,
    LCD_CS = 4
};
class Mega32Interface8bit {
public:
    static inline void init() {
        DDRD = 0xff;
        DDRB = (DDRB & 0xf8) | 7;
    }
    static inline void set_bus(unsigned char val) {
        PORTD = val;
    }
    static inline void ctl(unsigned char flags) {
        PORTB = (PORTB & 0xfc) | flags;
    }
};

class SystemLibAVR {
public:
    static inline void delay(unsigned int ms) {
        ::delay(ms);
    }
};

template<typename Interface, typename System>
class Controller : protected Interface, protected System {
public:
    static void init() {
        Interface::init();
        Interface::ctl(LCD_CS | LCD_WR | LCD_RS);
        System::delay(20);

        static unsigned short const PROGMEM initcd[] = {
            0x0011,0x2004,
            0x0013,0xCC00,
            0x0015,0x2600,
            0x0014,0x252A,
            0x0012,0x0033,
            0x0013,0xCC04,
            0x0013,0xCC06,
            0x0013,0xCC4F,
            0x0013,0x674F,
            0x0011,0x2003,
            0x0030,0x2609,
            0x0031,0x242C,
            0x0032,0x1F23,
            0x0033,0x2425,
            0x0034,0x2226,
            0x0035,0x2523,
            0x0036,0x1C1A,
            0x0037,0x131D,
            0x0038,0x0B11,
            0x0039,0x1210,
            0x003A,0x1315,
            0x003B,0x3619,
            0x003C,0x0D00,
            0x003D,0x000D,
            0x0016,0x0007,
            0x0002,0x0013,
            0x0003,0x0003,
            0x0001,0x0127,
            0x0008,0x0303,
            0x000A,0x000B,
            0x000B,0x0003,
            0x000C,0x0000,
            0x0041,0x0000,
            0x0050,0x0000,
            0x0060,0x0005,
            0x0070,0x000B,
            0x0071,0x0000,
            0x0078,0x0000,
            0x007A,0x0000,
            0x0079,0x0007,
            0x0007,0x0051,
            0x0007,0x0053,
            0x0079,0x0000,
        };
        for (unsigned short i = 0; i < sizeof(initcd); i += 4) {
            unsigned short pcmd = pgm_read_word((char *)initcd + i);
            unsigned short pdat = pgm_read_word((char *)initcd + i + 2);
            cmd_data(pcmd, pdat);
        }
        cmd(0x0022);
    }
    static void set_window(
        unsigned short left, unsigned short right,
        unsigned short top, unsigned short bottom) {
        cmd_data(0x0046, (right << 8) | left);
        cmd_data(0x0047, bottom);
        cmd_data(0x0048, top);
        cmd_data(0x0020, left);
        cmd_data(0x0021, top);
        cmd(0x0022);
    }
    static void cmd(unsigned short c) {
        Interface::ctl(LCD_WR);
        Interface::set_bus(c >> 8);
        Interface::ctl(0);
        Interface::ctl(LCD_WR);
        Interface::set_bus(c & 0xff);
        Interface::ctl(0);
        Interface::ctl(LCD_WR | LCD_CS);
    }
    static void data(unsigned short d) {
        Interface::ctl(LCD_WR | LCD_RS);
        Interface::set_bus(d >> 8);
        Interface::ctl(LCD_RS);
        Interface::ctl(LCD_RS | LCD_WR);
        Interface::set_bus(d & 0xff);
        Interface::ctl(LCD_RS);
        Interface::ctl(LCD_RS | LCD_WR | LCD_CS);
    }
    static inline void cmd_data(unsigned short c, unsigned short d) {
        cmd(c);
        data(d);
    }
};

struct font_desc {
    unsigned char magic;
    unsigned char first_char;
    unsigned char num_chars;
    unsigned char height;
    unsigned short offset[/*num_chars*/];
    //unsigned char bytes[];
};

class Font {
public:
    //  the Font should be pointed at PROGMEM data
    Font(void const *data);
    unsigned char min_char() const;
    unsigned char max_char() const;
    unsigned char height() const;
    bool get_char(unsigned char val, void const *&oPtr, unsigned char &ow) const;
private:
    void const *data_;
};

template<
    typename Controller = Controller<Mega32Interface8bit, SystemLibAVR>,
    unsigned short Width = 240,
    unsigned short Height = 320>
class LCDImpl :
    protected Controller {
public:
    static inline unsigned short width() { return Width; }
    static inline unsigned short height() { return Height; }

    static void init() {
        Controller::init();
        clear();
    }

    static inline void clear(unsigned short color = 0x0000) {
        fill_rect(0, 0, Width, Height, color);
    }

    static void fill_rect(
        unsigned short left, unsigned short top,
        unsigned short right, unsigned short bottom,
        unsigned short color) {

        //  clip to the screen
        if (right > Width) {
            right = Width;
        }
        if (bottom > Height) {
            bottom = Height;
        }
        if (left >= right || top >= bottom) {
            return;
        }

        //  now, fill with pixels
        Controller::set_window(left, right - 1, top, bottom - 1);

        for (unsigned short y = top; y != bottom; ++y) {
            for (unsigned char x = left; x != right; ++x) {
                Controller::data(color);
            }
        }
    }

    static void text(unsigned short left, unsigned short top,
        unsigned short tcolor, unsigned short bcolor,
        char const *str, unsigned char len,
        Font const &f) {

        unsigned char h = f.height();
        unsigned short oleft = left;
        set_window(left, top, Controller::width(), top + h);
        while (len > 0) {
            void const *ptr;
            unsigned char w;
            if (f.get_char(*str, ptr, w)) {
                unsigned char w2 = Controller::width() - left;
                if (w > w2) {
                    w = w2;
                }
                unsigned char h2 = Controller::height() - top;
                if (h2 > h) {
                    h2 = h;
                }
                fling_bits(ptr, w, h2, h, tcolor, bcolor);
                left += w;
                if (left >= Controller::width()) {
                    break;
                }
            }
            else if (*str == '\n') {
                top += h;
                if (top >= Controller::width()) {
                    break;
                }
                left = oleft;
                set_window(left, top, Controller::width(), top + h);
            }
            // else if (*str == '\t') {
            //     do tabs
            // }
            --len;
            ++str;
        }
    }

    /* Draw tightly packed bits in two color format. The bits 
       come with high bit first, and come in columns, each of 
       which is h bits hight. There are w columns to draw. Note 
       that each column is clipped to hlim height (for bottom 
       of screen).
       */
    static void fling_bits(void const *ptr, unsigned char w, unsigned char h,
        unsigned char hlim, unsigned short tcolor, unsigned short bcolor) {
        unsigned char bits = 0;
        unsigned char data = 0;
        while (w > 0) {
            for (unsigned char c = 0; c < h; ++c) {
                if (!bits) {
                    data = *(unsigned char const *)ptr;
                    ptr = (unsigned char const *)ptr + 1;
                    bits = 8;
                }
                if (c < hlim) {
                    if (data & 0x80) {
                        Controller::data(tcolor);
                    }
                    else {
                        Controller::data(bcolor);
                    }
                }
                data = data << 1;
            }
            w -= 1;
        }
    }
};

typedef LCDImpl<> LCD;

#endif  //  avr_lcd_h
