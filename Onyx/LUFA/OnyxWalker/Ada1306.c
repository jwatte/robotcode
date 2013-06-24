
#include "my32u4.h"
#include "Ada1306.h"
#include "font.h"
#include <LUFA/Drivers/Peripheral/TWI.h>

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
    SSD1306_SETPAGE = 0xB0,
};

static unsigned char display[WIDTH*HEIGHT];
//  nil clipping rect
static unsigned char d_left = WIDTH;
static unsigned char d_top = HEIGHT;
static unsigned char d_right = 0;
static unsigned char d_bottom = 0;

static unsigned char begun = 0;

static void _Reset(void) {
    //  The data sheet talks about bringing reset down and up and down again.
    //  This may just be poor grammar, but do it nevertheless.
    PORTD |= (1 << 5);
    MY_DelayUs(10);
    PORTD &= ~(1 << 5);
    MY_DelayUs(10);
    PORTD |= (1 << 5);
    MY_DelayUs(10);
    PORTD &= ~(1 << 5);
    MY_DelayUs(10);
    PORTD |= (1 << 5);
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
    TWI_SendByte(0x80);
    TWI_SendByte(cmd);
}

static void _Cmd2(unsigned char cmd, unsigned char arg) {
    if (!begun) {
        return;
    }
    TWI_SendByte(0x80);
    TWI_SendByte(cmd);
    TWI_SendByte(arg);
}

static void _Write(unsigned char cmd) {
    if (!begun) {
        return;
    }
    TWI_SendByte(cmd);
}

static unsigned char buf[5];

static void LCD_Flush_Row(unsigned char y, unsigned char const *display) {
    _Begin();
    _Cmd(SSD1306_SETPAGE | ((y + 4) & 0x7));
    _Cmd(SSD1306_SETLOWCOLUMN | ((d_left * 6) & 0xf));
    _Cmd(SSD1306_SETHIGHCOLUMN | (((d_left * 6) >> 4) & 0xf));
    _Write(0x40);   //  all that follows is data
    //  write a line
    for (unsigned char x = d_left; x < d_right; ++x) {
        unsigned char ch = display[x];
        if (ch < 32 || ch > 127) {
            ch = '?';
        }
        unsigned char const *cp = &font[(ch - 32) * 5];
        _Write(0);
        memcpy_P(buf, cp, 5);
        _Write(buf[0]);
        _Write(buf[1]);
        _Write(buf[2]);
        _Write(buf[3]);
        _Write(buf[4]);
    }
    _End();
}

void LCD_Setup(void) {
    TWI_Init(TWI_BIT_PRESCALE_1, TWI_BITLENGTH_FROM_FREQ(1, 400000));

    DDRD |= (1 << 5);   //  reset pin
    _Reset();

    _Begin();
    _Cmd(SSD1306_DISPLAYOFF);
    _Cmd2(SSD1306_SETDISPLAYCLOCKDIV, 0x80);
    _Cmd2(SSD1306_SETMULTIPLEX, 0x1f);
    _Cmd2(SSD1306_SETDISPLAYOFFSET, 0x0);
    _Cmd(SSD1306_SETSTARTLINE | 0);
    _Cmd2(SSD1306_SETCHARGEPUMP, 0x14);
    _Cmd2(SSD1306_MEMORYMODE, 2);
    _Cmd(SSD1306_SEGREMAP | 1);
    _Cmd(SSD1306_COMSCANDEC);
    _Cmd2(SSD1306_SETCOMPINS, 0x02);
    _Cmd2(SSD1306_SETCONTRAST, 0x8f);
    _Cmd2(SSD1306_SETPRECHARGE, 0xf1);
    _Cmd2(SSD1306_SETVCOMDETECT, 0x40);
    _Cmd(SSD1306_DISPLAYALLONRESUME);
    _Cmd(SSD1306_NORMALDISPLAY);
    _Cmd(SSD1306_DISPLAYON);
    _End();

    memset(display, 32, sizeof(display));
    d_left = 0;
    d_right = WIDTH;
    for (unsigned char r = 0; r != 8; ++r) {
        LCD_Flush_Row(r, display);
    }
    LCD_Clear();
}

void LCD_Clear(void) {
    d_left = 0;
    d_right = WIDTH;
    d_top = 0;
    d_bottom = HEIGHT;
    memset(display, 32, sizeof(display));
}

void LCD_Flush(void) {
    if (d_top < d_bottom && d_left < d_right) {
        for (unsigned char y = d_top; y < d_bottom; ++y) {
            LCD_Flush_Row(y, &display[WIDTH * y]);
        }
    }
    d_left = WIDTH;
    d_top = HEIGHT;
    d_right = 0;
    d_bottom = 0;
}

void LCD_DrawChar(unsigned char ch, unsigned char x, unsigned char y) {
    if (x >= WIDTH || y >= HEIGHT) {
        return;
    }
    display[x + y*WIDTH] = ch;
    if (x < d_left) d_left = x;
    if (x >= d_right) d_right = x+1;
    if (y < d_top) d_top = y;
    if (y >= d_bottom) d_bottom = y+1;
}

void LCD_DrawString(char const *str, unsigned char x, unsigned char y, unsigned char wrap) {
    while (*str) {
        if (wrap) {
            if (x >= WIDTH) {
                x = 0;
                y += 1;
            }
            if (y >= HEIGHT) {
                y = 0;
            }
        }
        LCD_DrawChar((unsigned char)*str, x, y);
        x += 1;
        ++str;
    }
}

void LCD_DrawFrac(unsigned short w, unsigned char ndig, unsigned char x, unsigned char y) {
    int n = x+5;
    for (unsigned char i = 0; i < 5; ++i) {
        if (i == ndig) {
            LCD_DrawChar('.', n, y);
            --n;
        }
        unsigned char val;
        if (w == 0 && i != 0) {
            val = ' ';
        }
        else {
            val = '0' + (w % 10);
            w = w / 10;
        }
        LCD_DrawChar(val, n, y);
        --n;
    }
}

void LCD_DrawUint(unsigned short w, unsigned char x, unsigned char y) {
    for (unsigned char i = 0; i < 5; ++i) {
        unsigned char val;
        if (w == 0 && i != 0) {
            val = ' ';
        }
        else {
            val = '0' + (w % 10);
            w = w / 10;
        }
        LCD_DrawChar(val, x+4-i, y);
    }
}

unsigned char hexdig(unsigned char val) {
    if (val < 10) {
        return '0' + val;
    }
    return 'A' + (val - 10);
}

void LCD_DrawHex(unsigned char const *buf, unsigned char sz, unsigned char x, unsigned char y) {
    for (unsigned char ch = 0; ch < sz; ++ch) {
        LCD_DrawChar(hexdig(buf[0] >> 4), x, y);
        LCD_DrawChar(hexdig(buf[0] & 0xf), x+1, y);
        LCD_DrawChar(' ', x+2, y);
        x += 3;
        if (x >= WIDTH-1) {
            break;
        }
        buf += 1;
    }
}
