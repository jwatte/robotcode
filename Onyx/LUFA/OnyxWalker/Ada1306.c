
#define I2C_WRITE 0x7a
#define I2C_READ 0x7b


#define WIDTH 128
#define HEIGHT 32

static unsigned char display[WIDTH*HEIGHT/8];
//  nil clipping rect
static unsigned char d_left = WIDTH;
static unsigned char d_top = HEIGHT;
static unsigned char d_right = 0;
static unsigned char d_bottom = 0;

static unsigned char begun = 0;

static void _Reset(void) {
}

static void _Begin(void) {
    begun = 1;
    //  todo: address I2C
}

static void _End(void) {
    begun = 0;
    //  todo: release I2C
}

static void _Cmd(unsigned char cmd) {
    if (!begun) {
        return;
    }
    //  todo: write 0 an cmd
}

static void _Write(unsigned char cmd) {
    if (!begun) {
        return;
    }
    //  todo: write byte
}

void LCD_Setup(void) {
    //  crap; I need a reset sequence!
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
    if (d_top < d_bottom && d_left < d_right) {
        for (unsigned char y = d_top; y < d_bottom; ++y) {
            _Begin();
            _Cmd(SSD1306_SETLOWCOLUMN | (left & 0xf));
            _Cmd(SSD1306_SETHIGHCOLUMN | ((left >> 4) & 0xf));
            _Cmd(SSD1306_SETSTARTLINE | y);
            _Write(0x40);
            //  write a line
            for (unsigned char x = d_left; x < d_right; ++x) {
                _Write(display[x + y * 128]);
            }
            _End();
        }
    }
    d_left = WIDTH;
    d_top = HEIGHT;
    d_right = 0;
    d_bottom = 0;
}

