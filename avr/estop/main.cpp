
#include <libavr.h>
#include <stddef.h>
#include <UTFT.h>
#include <UTFT.cpp>
#include <nRF24L01.h>

#include "cmds.h"

#include "../libavr/DefaultFonts.cpp"


#define PIN_BUTTON_STOP (16|7)
#define PIN_BUTTON_GO (0|0)
#define PIN_SELECTOR_LEFT (0|1)
#define PIN_SELECTOR_RIGHT (0|2)


// PORTC == 8
//  ITDB02-2.2SP hooked up to C0=RST, C1=SDA, C2=SCL, C3=CS
//  UTFT class specifies RS(==SDA), WR(==SCL), CS, RST
UTFT<HX8340B_S> lcd(8|1, 8|2, 8|3, 8|0);

void debug_blink(bool b)
{
    if (b) {
        lcd.setColor(255, 0, 0);
    }
    else {
        lcd.setColor(0, 0, 0);
    }
    lcd.drawLine(1, 1, 8, 8);
    lcd.drawLine(1, 8, 8, 1);
}

nRF24L01<> rf;

info_MotorPower g_info;
unsigned char g_steer_adjust;
unsigned char g_power_adjust;
unsigned char *adjust_ptr[3] = { 0, &g_steer_adjust, &g_power_adjust };

unsigned char uiMode = 0x80;
unsigned char nIsConnected;
volatile bool g_go_allowed = false;
volatile bool g_adjust_mode = false;

volatile bool g_go_pressed = false;
volatile unsigned char selectorValue = 0;
volatile unsigned char prevQuad = 0;
volatile unsigned char lastQuad = 0;

class RfIsr : public IPinChangeNotify {
    public:
        void pin_change(unsigned char) {
            rf.onIRQ();
        }
};
RfIsr rf_isr;

class StopIsr : public IPinChangeNotify {
    void pin_change(unsigned char val) {
        if (val == 0) {
            g_go_allowed = false;
            //  reset UI back to defaults
            g_adjust_mode = false;
            uiMode = 0;
        }
    }
};
StopIsr stop_isr;

class UiIsr : public IPinChangeNotify {
    void pin_change(unsigned char bitValue) {
        unsigned char pb = PINB;
        if (!(pb & 1)) {
            if (uiMode == 0) {
                g_go_allowed = true;
            }
            else {
                g_go_pressed = true;
            }
        }

        unsigned char quad = ((pb & 4) ? 1 : 0) | ((pb & 2) ? 2 : 0);
        if (quad & 2) quad ^= 1;

        lastQuad = quad;
        if (((quad - 1) & 3) == prevQuad) {
            prevQuad = quad;
            ++selectorValue;
        }
        else if (((quad + 1) & 3) == prevQuad) {
            prevQuad = quad;
            --selectorValue;
        }
    }
};
UiIsr ui_isr;

#define N_ROWS 12
#define N_COLS 27
char lcdChars[N_ROWS][N_COLS];

inline unsigned char color_r(unsigned char y) {
    return 255 - ((y > 128) ? (255 - y) : y);
}
inline unsigned char color_g(unsigned char y) {
    return 255 - ((y > 96) ? (y - 96) : (96 - y));
}
inline unsigned char color_b(unsigned char y) {
    return 255 - ((y > 192) ? (y - 192) : (192 - y));
}

void lcdDrawChars(char *ptr, unsigned char n, unsigned char y, unsigned char x)
{
    lcd.setColor(color_r(y), color_g(y), color_b(y));
    char buf[N_COLS+1];
    memcpy(buf, ptr, n);
    buf[n] = 0;
    lcd.print(buf, x, y, 0);
}

void lcdPrint(unsigned char row, unsigned char col, char const *data)
{
    if (row >= N_ROWS || col >= N_COLS) {
        fatal(FATAL_BAD_PARAM);
    }
    bool diff = false;
    unsigned char startCol = col;
    char *ptr = &lcdChars[row][col];
    while (*data && (col < N_COLS)) {
        char ch = *data;
        ++data;
        if ((ch == *ptr) || ((ch == ' ') && (*ptr == 0))) {
            if (diff) {
                diff = false;
                lcdDrawChars(&lcdChars[row][startCol], col - startCol, row * 14, startCol * 8);
            }
        }
        else {
            *ptr = ch;
            if (!diff) {
                diff = true;
                startCol = col;
            }
        }
        ++col;
        ++ptr;
    }
    if (diff) {
        lcdDrawChars(&lcdChars[row][startCol], col - startCol, row * 14, startCol * 8);
    }
}

char fmtBuf[24];

char const *fmt(char const *text, void const *val, RegType type)
{
    strcpy_P(fmtBuf, text);
    size_t l = strlen(fmtBuf);
    format_value(val, type, 24-l, &fmtBuf[l]);
    return fmtBuf;
}

char const pCmdPower[] PROGMEM =    "Cmd Power  ";
char const pCmdSteer[] PROGMEM =    "Cmd Steer  ";
char const pEAllow[] PROGMEM =      "Allowed    ";
char const pTrimPower[] PROGMEM =   "Trim Power ";
char const pTrimSteer[] PROGMEM =   "Trim Steer ";
char const pActualPower[] PROGMEM = "Act Power  ";
char const pSelfStop[] PROGMEM =    "Self Stop  ";
char const pVoltage[] PROGMEM =     "M Batt (V) ";

void render_info()
{
    lcdPrint(0, 0, fmt(pCmdPower, &g_info.w_cmd_power, RegTypeSchar));
    lcdPrint(1, 0, fmt(pCmdSteer, &g_info.w_cmd_steer, RegTypeSchar));
    lcdPrint(2, 0, fmt(pEAllow, &g_info.w_e_allow, RegTypeUchar));
    lcdPrint(3, 0, fmt(pTrimPower, &g_info.w_trim_power, RegTypeUchar));
    lcdPrint(4, 0, fmt(pTrimSteer, &g_info.w_trim_steer, RegTypeUchar));
    lcdPrint(5, 0, fmt(pActualPower, &g_info.r_actual_power, RegTypeSchar));
    lcdPrint(6, 0, fmt(pSelfStop, &g_info.r_self_stop, RegTypeUchar));
    //  e_conn is a little dumb to waste space on...
    lcdPrint(7, 0, fmt(pVoltage, &g_info.r_voltage, RegTypeUchar16));
}

void dispatch_cmd(unsigned char sz, unsigned char const *n)
{
    if (sz > sizeof(g_info)) {
        sz = sizeof(g_info);
    }
    memcpy(&g_info, n, sz);
    render_info();
}

bool wasConnected = true;

void update_is_connected()
{
    bool isConnected = nIsConnected > 0;
    if (isConnected != wasConnected) {
        if (isConnected) {
            lcd.setColor(0, 255, 128);
        }
        else {
            lcd.setColor(255, 128, 0);
        }
        lcd.print(isConnected ? "OK!" : "n/c", 195, 0, 0);
        wasConnected = isConnected;
    }
}

void transmit_value(void *v)
{
    uint8_t hd = rf.hasData();
    if (hd > 0) {
        nIsConnected = 20;
    }
    else if (nIsConnected > 0) {
        --nIsConnected;
    }
    if (hd) {
        char buf[32];
        rf.readData(hd, buf);
        dispatch_cmd(hd, (unsigned char const *)buf);
    }
    if (!g_adjust_mode && rf.canWriteData()) {
        if (!rf.hasLostPacket()) {
            v = (char *)v + 1;
        }
        char cmd[3];
        cmd[0] = offsetof(info_MotorPower, w_e_allow);
        cmd[1] = 1;
        cmd[2] = g_go_allowed;
        rf.writeData(3, cmd);
    }
    after(100, &transmit_value, v);
    update_is_connected();
}

char const pMonitorBot[] PROGMEM = "Monitor Bot";

char const * uiModeNames[] = {
    pMonitorBot,
    pTrimSteer,
    pTrimPower
};

char const pCanGo[] PROGMEM =    "Can Go   ";
char const pMustStop[] PROGMEM = "Must Stop";

void read_ui(void *)
{
    lcdPrint(9, 0, strcpy_P(fmtBuf, g_go_allowed ? pCanGo : pMustStop));
    unsigned char oldUiMode = uiMode;
    {
        IntDisable idi;
        if (selectorValue < 0xfd && selectorValue > 3) {
            char d = -1;
            if (selectorValue < 0x80) {
                d = 1;
            }
            if (!g_adjust_mode) {
                uiMode += d;
            }
            else {
                *(adjust_ptr[uiMode]) += d;
            }
            selectorValue = 0;
        }
    }
    if (uiMode > 0xfc) {
        uiMode = sizeof(uiModeNames)/sizeof(uiModeNames[0]) - 1;
    }
    else if (uiMode >= sizeof(uiModeNames)/sizeof(uiModeNames[0])) {
        uiMode = 0;
    }
    if (uiMode != oldUiMode) {
        g_adjust_mode = false;
        lcdPrint(10, 0, strcpy_P(fmtBuf, uiModeNames[uiMode]));
        oldUiMode = uiMode;
    }
    if (g_go_pressed) {
        g_go_pressed = false;
        g_adjust_mode = !g_adjust_mode;
    }
    //  in case the trim value was negative, clear the trailing digit
    if (uiMode == 1) {
        lcdPrint(11, 0, fmt(pTrimSteer, &g_steer_adjust, RegTypeSchar));
        if (g_adjust_mode && rf.canWriteData()) {
            if (g_steer_adjust >= 0) {
                lcdPrint(11, 13, " ");
            }
            fmtBuf[0] = offsetof(info_MotorPower, w_trim_steer);
            fmtBuf[1] = 1;
            fmtBuf[2] = g_steer_adjust;
            rf.writeData(3, fmtBuf);
        }
    }
    else if (uiMode == 2) {
        lcdPrint(11, 0, fmt(pTrimPower, &g_power_adjust, RegTypeUchar));
        if (g_adjust_mode && rf.canWriteData()) {
            if (g_power_adjust >= 0) {
                lcdPrint(11, 13, " ");
            }
            fmtBuf[0] = offsetof(info_MotorPower, w_trim_power);
            fmtBuf[1] = 1;
            fmtBuf[2] = g_power_adjust;
            rf.writeData(3, fmtBuf);
        }
    }
    else {
        memset(fmtBuf, 32, 15);
        fmtBuf[15] = 0;
        lcdPrint(11, 0, fmtBuf);
        g_adjust_mode = false;
        g_power_adjust = g_info.w_trim_power;
        g_steer_adjust = g_info.w_trim_steer;
    }
    if (g_adjust_mode) {
        lcdPrint(11, 14, " <->");
    }
    else {
        lcdPrint(11, 14, "    ");
    }
    g_go_pressed = false;
    after(30, &read_ui, 0);
}

bool wasRadioConnected = true;

void reset_radio(void *)
{
    if (!nIsConnected) {
        if (!wasRadioConnected) {
            lcdPrint(0, 0, "Reset Radio");
            rf.teardown();
            delay(150);
            rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);
            memset(fmtBuf, 32, 23);
            fmtBuf[23] = 0;
            lcdPrint(0, 0, fmtBuf);
            lcdPrint(1, 20, fmtBuf);
        }
        else {
            wasRadioConnected = false;
        }
    }
    else {
        wasRadioConnected = true;
    }
    after(10000, &reset_radio, 0);
}

void setup()
{
    DDRB |= (1 << PB5);
    PORTB &= ~(1 << PB5);
    DDRC |= 0xf;
    DDRD |= (1 << PD3);
    PORTD |= (1 << PD3);
    DDRB &= ~0x7;   //  go, selector
    PORTB |= 0x7;   //  pull-up
    DDRD &= ~0x80;  //  stop
    PORTD |= 0x80;  //  pull-up

    PCMSK0 |= 0x7;  //  go, selector
    PCICR |= 0x1;   //  port B pin change

    lcd.InitLCD(LANDSCAPE);
    fatal_set_blink(debug_blink);
    lcd.setBackColor(0, 0, 0);
    lcd.setColor(0, 0, 0);
    lcd.fillRect(0, 0, lcd.getDisplayXSize(), lcd.getDisplayYSize());
    lcd.setFont(SmallFont);
    lcd.setColor(0, 255, 0);
    lcdPrint(0, 0, __DATE__ " " __TIME__);

    on_pinchange(rf.getPinIRQ(), &rf_isr);
    on_pinchange(PIN_BUTTON_STOP, &stop_isr);
    on_pinchange(0, &ui_isr);
    on_pinchange(1, &ui_isr);
    on_pinchange(2, &ui_isr);
    delay(100); //  wait for radio to boot
    rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);

    transmit_value(0);
    read_ui(0);
    after(10000, &reset_radio, 0);
}




