
#include "my32u4.h"
#include "Ada1306.h"
#include "font.h"
#include <LUFA/Drivers/Peripheral/TWI.h>

#define I2C_WRITE 0x7a
#define I2C_READ 0x7b

#define WIDTH 21
#define HEIGHT 4

enum {
    SSD1306_DISPLAYOFF = 0xae,
    SSD1306_SETDISPLAYCLOCKDIV = 0xd5,
    SSD1306_SETMULTIPLEX = 0xa8,
    SSD1306_SETDISPLAYOFFSET = 0xd3,
    SSD1306_SETSTARTLINE = 0x40,
    SSD1306_SETCHARGEPUMP = 0x8d,
    SSD1306_MEMORYMODE = 0x20,
    SSD1306_SEGREMAP = 0xa0,
    SSD1306_COMSCANDEC = 0xc8,
    SSD1306_SETCOMPINS = 0xda,
    SSD1306_SETCONTRAST = 0x81,
    SSD1306_SETPRECHARGE = 0xd9,
    SSD1306_SETVCOMDETECT = 0xdb,
    SSD1306_DISPLAYALLONRESUME = 0xa4,
    SSD1306_NORMALDISPLAY = 0xa6,
    SSD1306_DISPLAYON = 0xaf,
    SSD1306_SETLOWCOLUMN = 0x00,
    SSD1306_SETHIGHCOLUMN = 0x10,
};

static unsigned char display[WIDTH*HEIGHT/8];
//  nil clipping rect
static unsigned char d_left = WIDTH;
static unsigned char d_top = HEIGHT;
static unsigned char d_right = 0;
static unsigned char d_bottom = 0;

static unsigned char begun = 0;

static void _Reset(void) {
    PORTF |= (1 << 1);
    MY_DelayUs(10);
    PORTF &= ~(1 << 1);
    MY_DelayUs(10);
    PORTF |= (1 << 1);
    MY_DelayUs(10);
}

static void _Begin(void) {
    if (TWI_StartTransmission(I2C_WRITE, 1) == TWI_ERROR_NoError) {
        begun = 1;
    }
}

static void _End(void) {
    if (begun) {
        TWI_StopTransmission();
        begun = 0;
    }
}

static void _Cmd(unsigned char cmd) {
    if (!begun) {
        return;
    }
    TWI_SendByte(0);
    TWI_SendByte(cmd);
}

static void _Write(unsigned char cmd) {
    if (!begun) {
        return;
    }
    TWI_SendByte(cmd);
}

void LCD_Setup(void) {
    TWI_Init(TWI_BIT_PRESCALE_1, TWI_BITLENGTH_FROM_FREQ(1, 400000));

    DDRF |= (1 << 1);   //  reset pin
    _Reset();

    _Begin();
    _Cmd(SSD1306_DISPLAYOFF);
    _Cmd(SSD1306_SETDISPLAYCLOCKDIV);
    _Cmd(0x80);
    _Cmd(SSD1306_SETMULTIPLEX);
    _Cmd(0x1f);
    _Cmd(SSD1306_SETDISPLAYOFFSET);
    _Cmd(0x0);
    _Cmd(SSD1306_SETSTARTLINE | 0);
    _Cmd(SSD1306_SETCHARGEPUMP);
    _Cmd(0x14);
    _Cmd(SSD1306_MEMORYMODE);
    _Cmd(0);
    _Cmd(SSD1306_SEGREMAP | 1);
    _Cmd(SSD1306_COMSCANDEC);
    _Cmd(SSD1306_SETCOMPINS);
    _Cmd(0x02);
    _Cmd(SSD1306_SETCONTRAST);
    _Cmd(0x8f);
    _Cmd(SSD1306_SETPRECHARGE);
    _Cmd(0xf1);
    _Cmd(SSD1306_SETVCOMDETECT);
    _Cmd(0x40);
    _Cmd(SSD1306_DISPLAYALLONRESUME);
    _Cmd(SSD1306_NORMALDISPLAY);
    _Cmd(SSD1306_DISPLAYON);
    _End();

    LCD_Clear();
    LCD_Flush();
}

void LCD_Clear(void) {
    d_left = 0;
    d_right = WIDTH;
    d_top = 0;
    d_bottom = HEIGHT;
    memset(display, 0, sizeof(display));
}

void LCD_Flush(void) {
    unsigned char buf[5];
    if (d_top < d_bottom && d_left < d_right) {
        for (unsigned char y = d_top; y < d_bottom; ++y) {
            _Begin();
            _Cmd(SSD1306_SETLOWCOLUMN | ((d_left * 6) & 0xf));
            _Cmd(SSD1306_SETHIGHCOLUMN | (((d_left * 6) >> 4) & 0xf));
            _Cmd(SSD1306_SETSTARTLINE | y);
            _Write(0x40);
            //  write a line
            for (unsigned char x = d_left; x < d_right; ++x) {
                _Write(0);
                unsigned char ch = display[x + y * WIDTH];
                if (ch < 32 || ch > 126) {
                    ch = '?';
                }
                unsigned char const *cp = &font[(ch-32)*5];
                memcpy_P(buf, cp, 5);
                _Write(buf[0]);
                _Write(buf[1]);
                _Write(buf[2]);
                _Write(buf[3]);
                _Write(buf[4]);
            }
            _End();
        }
    }
    d_left = WIDTH;
    d_top = HEIGHT;
    d_right = 0;
    d_bottom = 0;
}

