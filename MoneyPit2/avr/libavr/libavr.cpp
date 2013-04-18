#include "libavr.h"
#include "pins_avr.h"
#include <avr/pgmspace.h>

/* global volatile int to avoid optimization of spinloop */
int volatile globalctr;
unsigned long actual_f_cpu = F_CPU;
unsigned long actual_f_cpu_1000 = F_CPU / 1000;
#if HAS_UART
bool _uart_init = false;
#endif


void fatal_no_blink(bool) {}
void (*g_fatal_blink)(bool) = &fatal_no_blink;

void fatal_set_blink(void (*blink)(bool))
{
    if (blink == NULL) {
        blink = &fatal_no_blink;
    }
    g_fatal_blink = blink;
}

/* tell the world about an emergency by flashing indicators */
void fatal(int err)
{
    disable_interrupts();
    //  remember what the last fatal code was
    if (eeprom_read_byte((uint8_t const *)EE_FATALCODE) != err) {
        eeprom_write_byte((uint8_t *)EE_FATALCODE, (unsigned char)err);
    }
    //  sync byte
#if HAS_UART
    if (_uart_init) {
        uart_force_out(0xed);
        uart_force_out(0x02);
        uart_force_out('F');
        uart_force_out(err);
    }
#endif
    unsigned int nIter = actual_f_cpu_1000 / 80;
    if (nIter < 10) {
        nIter = 10;
    }
    else if (nIter > 300) {
        nIter = 300;
    }
    while (true) {
        for (int k = 0; k < (err & 0xf); ++k) {
            (*g_fatal_blink)(true);
            for (unsigned int volatile i = 0; i < nIter; ++i) {
                for (globalctr = 0; globalctr < 350; ++globalctr) {
                }
            }
            (*g_fatal_blink)(false);
            for (unsigned int volatile i = 0; i < 2 * nIter; ++i) {
                for (globalctr = 0; globalctr < 350; ++globalctr) {
                }
            }
        }
        for (unsigned int volatile i = 0; i < 8 * nIter; ++i) {
            for (globalctr = 0; globalctr < 350; ++globalctr) {
            }
        }
    }
}


#if HAVE_16BIT_TIMER1
/* timer API */

unsigned short g_lastTimer1Value;
unsigned short g_lastMillisecondValue;
unsigned short g_lastPhaseValue;
//  PhaseScale is set so that multiplying timer top (65536) by the 
//  scale ends up in (num mulliseconds << 24).
//  Timer increments 1 per 8 clocks, this equals microseconds at 8 MHz.
//  Thus, (65.536 milliseconds << 24) / 65536 which ends up in 
//  (65.536 milliseconds << 8) or about 16777.
//  Oops! This means I can't support 1 MHz clock :-(
unsigned short phaseScale = 16777;

//  This should be called only with interrupts disabled!
unsigned short read_timer1_inner()
{
    unsigned short timer1Value = TCNT1L;
    timer1Value |= ((unsigned int)TCNT1H << 8u);
    //  example timer1Value: 32000
    g_lastPhaseValue += (((unsigned long)((timer1Value - g_lastTimer1Value) & 0xffff) *
                (unsigned long)phaseScale) >> 16);  //  shift 16 means about 4 us per tick
    //  example g_lastPhaseValue: 32000*16000/64000 == 8000
    g_lastTimer1Value = timer1Value;
    //  example g_lastTimer1Value: 32000
    g_lastMillisecondValue += (g_lastPhaseValue >> 8);  //  divide by 256
    //  example g_lastMillisecondValue: 8000 / 256 == 31
    g_lastPhaseValue &= 0xff;
    //  example g_lastPhaseValue: 8000 & 0xff == 64
    return timer1Value;
}

ISR(TIMER1_OVF_vect)
{
    //  this will magically update the timer
    read_timer1_inner();
}

ISR(TIMER1_COMPA_vect)
{
    read_timer1_inner();
}

unsigned short uread_timer()
{
    IntDisable idi;
    return (unsigned long)read_timer1_inner() * 8000 / actual_f_cpu_1000;
}

unsigned short read_timer()
{
    IntDisable idi;
    read_timer1_inner();
    return g_lastMillisecondValue;
}

unsigned short read_timer_fast() {
    return g_lastMillisecondValue;
}


//  between 2 and 5 us pre-calculated
static unsigned short amt_table[4] = {
    3, 4, 5, 6
};

static void setup_amt_table()
{
    for (unsigned char i = 0; i != 4; ++i) {
        //  start at 1, not 3, assuming the udelay setup takes a microsecond
        amt_table[i] = ((unsigned long)(i + 1)) * (unsigned long)actual_f_cpu_1000 / 8000ul;
    }
}

void setup_timers(unsigned long f_cpu)
{
    actual_f_cpu = f_cpu;
    actual_f_cpu_1000 = f_cpu / 1000;
    setup_amt_table();
    power_timer1_enable();
    //  make sure to not run out of bits in the math...
    phaseScale = (unsigned short)(16777UL * (8000000 >> 8) / (f_cpu >> 8));
    OCR1AH = 0x7f;  //  midpoint interrupt
    OCR1AL = 0xff;  //  midpoint interrupt
    ICR1H = 0xff;
    ICR1L = 0xff;
    TCCR1A = 0;     //  normal mode
    TCCR1B = 0 | (1 << CS11); //  normal mode, timer 1 at 1/8 clock
    TIMSK1 = (1 << OCIE1A) | (1 << TOIE1);
}

void udelay(unsigned short amt)
{
    if (amt > 8000) {
        fatal(FATAL_TOO_LONG_DELAY);
    }
    if (amt < 3) {
        //  less than two, the function call overhead has you got
        return;
    }
    amt -= 3; /* for table index */
    if (amt < 4) {
        amt = amt_table[amt];
    }
    else {
        amt = (unsigned long)amt * actual_f_cpu_1000 / 8000;
    }
    unsigned short start = TCNT1L;
    start |= ((unsigned short)TCNT1H << 8u);
    while (true) {
        unsigned short val = TCNT1L;
        val |= ((unsigned short)TCNT1H << 8u);
        if ((unsigned short)(val - start) >= amt) {
            break;
        }
    }
}

void delay(unsigned short ms)
{
    if (ms > 8000) {
        fatal(FATAL_TOO_LONG_DELAY);
    }
    unsigned short now = read_timer();
    while ((unsigned short)(read_timer() - now) < ms) {
        /* do nothing */
    }
}
#endif  //  HAVE_16BIT_TIMER1

/* task API */

struct AfterRec
{
    AfterRec *next;
    unsigned short at_time;
    void (*func)(void *data);
    void *data;
};

#define MAX_AFTERS 20
AfterRec g_afters[MAX_AFTERS];
AfterRec *g_curq;
AfterRec *g_freeq;

void at(unsigned short time, void (*func)(void *data), void *data) {
    unsigned char idi = disable_interrupts();
    AfterRec *rec = g_freeq;
    if (!rec) {
        fatal(FATAL_OUT_OF_AFTERS);
    }
    g_freeq = rec->next;
    rec->func = func;
    rec->data = data;
    rec->at_time = time;
    rec->next = g_curq;
    g_curq = rec;
    restore_interrupts(idi);
}

void after(unsigned short delay, void (*func)(void *data), void *data)
{
    if (delay > 32767) {
        fatal(FATAL_TOO_LONG_DELAY);
    }
    at(read_timer() + delay, func, data);
}

void schedule()
{
    wdt_reset();
    unsigned short now = read_timer();
    AfterRec *to_run = 0;
    unsigned char idi;

    idi = disable_interrupts();
    to_run = g_curq;
    g_curq = 0;

    while (to_run) {
        if ((short)(now - to_run->at_time) >= 0) {
            restore_interrupts(idi);
            (*to_run->func)(to_run->data);
            idi = disable_interrupts();
            AfterRec *to_free = to_run;
            to_run = to_run->next;
            wdt_reset();
            unsigned short then = read_timer();
#if defined(MAX_TASK_TIME)
            if (then - now > MAX_TASK_TIME) {
                eeprom_write_word((word *)EE_TOO_LONG_TASK_PTR, (word)func);
                eeprom_write_word((word *)EE_TOO_LONG_TASK_PRE, (word)now);
                eeprom_write_word((word *)EE_TOO_LONG_TASK_POST, (word)then);
                fatal(FATAL_TASK_TOOK_TOO_LONG);
            }
#endif
            to_free->next = g_freeq;
            g_freeq = to_free;
            now = then;
        }
        else {
            AfterRec *to_link = to_run;
            to_run = to_run->next;
            to_link->next = g_curq;
            g_curq = to_link;
        }
    }
    restore_interrupts(idi);
}

/* watchdog API */

void setup_watchdog()
{
    //  clear interrupt source flag
    unsigned char rsrc = MCUSR;
    MCUSR = 0;
    wdt_disable();
    unsigned char curCode = eeprom_read_byte((uint8_t const *)EE_RESETSOURCE);
    unsigned char prevCode = eeprom_read_byte((uint8_t const *)EE_PREV_RESETSOURCE);
    //  save current reset in curCode
    if (curCode != rsrc) {
        eeprom_write_byte((uint8_t *)EE_RESETSOURCE, rsrc);
    }
    //  save abnormal resets in prevCode
    if ((curCode & ((1 << WDRF) | (1 << BORF))) && (curCode != prevCode)) {
        eeprom_write_byte((uint8_t *)EE_PREV_RESETSOURCE, curCode);
    }
    unsigned short nboot = eeprom_read_word((uint16_t const *)EE_NUM_BOOTS);
    eeprom_write_word((uint16_t *)EE_NUM_BOOTS, ++nboot);
    wdt_reset();
}

#if HAS_UART

/* UART serial API */

/* The interrupt disables in read and write may not actually 
   be needed, because writing advances the "head" and clearing 
   out the write buffer (from interrupt) advances the "tail."
   Similarly, reading advances the "tail" and receiving into 
   the buffer (from interrupt) advances the "head."
 */
struct BrateSetup {
    unsigned char rateKbps;
    unsigned char UBRRn;
};
BrateSetup const rates[] PROGMEM = {
    { 9, 207 },
    { 19, 103 },
    { 38, 51 },
    { 57, 34 },
    { 115, 16 }
};

void (*_usart_rx_vect_func)();
void (*_usart_udre_vect_func)();

ISR(USART_RX_vect) {
    if (_usart_rx_vect_func) {
        _usart_rx_vect_func();
    }
}

unsigned char _uart_txbuf[32];
volatile char _uart_txptr;
volatile char _uart_txend;
unsigned char _uart_rxbuf[32];
volatile char _uart_rxptr;
volatile char _uart_rxend;

static void usart_rx_vect() {
again:
    while (UCSR0A & (1 << RXC0)) {
        if ((unsigned char)(_uart_rxend - _uart_rxptr) < (char)sizeof(_uart_rxbuf)) {
            _uart_rxbuf[_uart_rxend & 0x1f] = UDR0;
            ++_uart_rxend;
            goto again;
        }
        else {
            //  overrun! -- clear it out
            (void)UDR0;
        }
    }
}

ISR(USART_UDRE_vect) {
    if (_usart_udre_vect_func) {
        _usart_udre_vect_func();
    }
}

static void usart_udre_vect() {
    if (UCSR0A & (1 << UDRE0)) {
        if ((char)(_uart_txend - _uart_txptr) > 0) {
            UDR0 = _uart_txbuf[_uart_txptr & 0x1f];
            ++_uart_txptr;
        }
        else {
            //  turn off interrupts -- nothing to send
            UCSR0B &= ~(1 << UDRIE0);
        }
    }
}

static void _usart_setup_ints() {
    _usart_rx_vect_func = usart_rx_vect;
    _usart_udre_vect_func = usart_udre_vect;
}

void uart_setup(unsigned long brate, unsigned long f_cpu) {
    IntDisable idi;
    unsigned long scale = ((f_cpu / 10) << 8) / 1600000;
    UCSR0B = 0;
    if (brate == 0) {
        return;
    }
    BrateSetup bsu;
    unsigned char kbps = brate / 1000;
    for (unsigned char ch = 0; ch != sizeof(rates)/sizeof(rates[0]); ++ch) {
        memcpy_P(&bsu, &rates[ch], sizeof(bsu));
        if (bsu.rateKbps == kbps) {
            _usart_setup_ints();
            _uart_rxptr = 0;
            _uart_rxend = 0;
            _uart_txptr = 0;
            _uart_txend = 0;
            unsigned long lv = ((unsigned long)bsu.UBRRn * scale) >> 8;
            UBRR0H = (lv >> 8) & 0xff;
            UBRR0L = lv & 0xff;
            UCSR0A = (1 << U2X0);
            UCSR0C = (3 << UCSZ00) | (1 << USBS0); //  two stop bits
            UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
            _uart_init = true;
            return;
        }
    }
    fatal(FATAL_BAD_SERIAL);
}

unsigned char uart_send(unsigned char n, void const *data) {
    IntDisable idi; //  this may not be needed
    unsigned char xmit = 0;
    while ((char)(_uart_txend - _uart_txptr) < (char)sizeof(_uart_txbuf) && xmit < n) {
        _uart_txbuf[_uart_txend & 0x1f] = ((char const *)data)[xmit];
        ++xmit;
        ++_uart_txend;
    }
    //  turn on interrupt handler (if not already on) -- will
    //  start sending whatever is in the buffer
    UCSR0B |= (1 << UDRIE0);
    return xmit;
}

unsigned char uart_send_space() {
    char ret = (char)sizeof(_uart_txbuf) - (char)(_uart_txend - _uart_txptr);
    if (ret < 0 || ret > (char)sizeof(_uart_txbuf)) {
        fatal(FATAL_UNEXPECTED_SERIAL);
    }
    return (unsigned char)ret;
}

void uart_force_out(char ch) {
    IntDisable idi;
    while (!(UCSR0A & (1 << UDRE0))) {
        //  wait
    }
    UDR0 = ch;
}

void uart_send_all(unsigned char ch, void const *data)
{
    while (ch > 0) {
        unsigned char sent = uart_send(ch, data);
        data = (char const *)data + sent;
        ch -= sent;
        if (ch > 0) {
            //  wait for some time
            udelay(100);
        }
    }
}

unsigned char uart_available() {
    IntDisable idi; //  this may not be needed
    return (unsigned char)(_uart_rxend - _uart_rxptr);
}

char uart_getch() {
    IntDisable idi; //  this may not be needed
    char ret = 0;
    if ((char)(_uart_rxend - _uart_rxptr) > 0) {
        ret = _uart_rxbuf[_uart_rxptr & 0x1f];
        ++_uart_rxptr;
    }
    return ret;
}

unsigned char uart_read(unsigned char n, void *data) {
    IntDisable idi; //  this may not be needed
    unsigned char avail = _uart_rxend - _uart_rxptr;
    if (avail > n) {
        avail = n;
    }
    for (unsigned char i = 0; i != avail; ++i) {
        ((char *)data)[i] = _uart_rxbuf[(_uart_rxptr + i) & 0x1f];
    }
    _uart_rxptr += avail;
    return avail;
}

#endif



/* pinchange interrupts */

#if defined(PIN_PCINT0)
IPinChangeNotify *_pcn[24] = { 0 };
unsigned char _pcint_last[3] = { 0 };

void (*_pcint0_vect_func)();

ISR(PCINT0_vect) {
    if (_pcint0_vect_func) {
        _pcint0_vect_func();
    }
}
#define DISPATCH(x) \
    if (((last ^ val) & (1 << ((x) & 7))) && _pcn[(x)]) { \
        _pcn[(x)]->pin_change(val & (1 << ((x) & 7))); \
    }
static void pcint0_vect() {
    unsigned char val = PIN_PCINT0;
    unsigned char last = _pcint_last[0];
    _pcint_last[0] = val;
    DISPATCH(0);
    DISPATCH(1);
    DISPATCH(2);
    DISPATCH(3);
    DISPATCH(4);
    DISPATCH(5);
    DISPATCH(6);
    DISPATCH(7);
}
#endif

#if defined(PIN_PCINT1)
void (*_pcint1_vect_func)();

ISR(PCINT1_vect) {
    if (_pcint1_vect_func) {
        _pcint1_vect_func();
    }
}

static void pcint1_vect() {
    unsigned char val = PIN_PCINT1;
    unsigned char last = _pcint_last[1];
    _pcint_last[1] = val;
    DISPATCH(8);
    DISPATCH(9);
    DISPATCH(10);
    DISPATCH(11);
    DISPATCH(12);
    DISPATCH(13);
    DISPATCH(14);
    DISPATCH(15);
}
#endif

#if defined(PIN_PCINT2)

void (*_pcint2_vect_func)();

ISR(PCINT2_vect) {
    if (_pcint2_vect_func) {
        _pcint2_vect_func();
    }
}

static void pcint2_vect() {
    unsigned char val = PIN_PCINT2;
    unsigned char last = _pcint_last[2];
    _pcint_last[2] = val;
    DISPATCH(16);
    DISPATCH(17);
    DISPATCH(18);
    DISPATCH(19);
    DISPATCH(20);
    DISPATCH(21);
    DISPATCH(22);
    DISPATCH(23);
}
#endif

#if defined(PIN_PCINT0)
void on_pinchange(unsigned char pin, IPinChangeNotify *pcn)
{
    if (pin > 23) {
        fatal(FATAL_BAD_PIN);
    }
    IntDisable idi;
    _pcint0_vect_func = pcint0_vect;
#if defined(PIN_PCINT1)
    _pcint1_vect_func = pcint1_vect;
#endif
#if defined(PIN_PCINT2)
    _pcint2_vect_func = pcint2_vect;
#endif
    _pcn[pin] = pcn;
    if (pcn) {
        pcMaskReg(pin) |= pcMaskBit(pin);
    }
    else {
        pcMaskReg(pin) &= ~pcMaskBit(pin);
    }
    if (pcMaskReg(pin)) {
        pcCtlReg(pin) |= pcCtlBit(pin);
    }
    else {
        pcCtlReg(pin) &= ~pcCtlBit(pin);
    }
}

void register_pcint(unsigned char ix, unsigned char mask, void (*func)()) {
    IntDisable idi;
    switch (ix) {
    case 0:
        _pcint0_vect_func = func;
        if (func && mask) {
            pcMaskReg(0) = mask;
            pcCtlReg(0) |= pcCtlBit(0);
        }
        else {
            pcMaskReg(0) = 0;
            pcCtlReg(0) &= ~pcCtlBit(0);
        }
        break;
#if NUM_PCINTS > 1
    case 1:
        _pcint1_vect_func = func;
        if (func && mask) {
            pcMaskReg(8) = mask;
            pcCtlReg(0) |= pcCtlBit(8);
        }
        else {
            pcMaskReg(8) = 0;
            pcCtlReg(0) &= ~pcCtlBit(8);
        }
        break;
#endif
#if NUM_PCINTS > 2
    case 2:
        _pcint2_vect_func = func;
        if (func && mask) {
            pcMaskReg(16) = mask;
            pcCtlReg(0) |= pcCtlBit(16);
        }
        else {
            pcMaskReg(16) = 0;
            pcCtlReg(0) &= ~pcCtlBit(16);
        }
        break;
#endif
    default:
        fatal(FATAL_BAD_ARGS);
    }
}
#endif

#if defined(HAVE_16BIT_TIMER1)
void setup_after() {
    for (unsigned char ch = 0; ch != MAX_AFTERS; ++ch) {
        g_afters[ch].next = g_freeq;
        g_freeq = &g_afters[ch];
    }
}
#endif

/* boot stuff */

void setup_boot_code() {
    disable_interrupts();
    setup_watchdog();
    setup_after();
    setup_timers();
    //  force interrupts on
    restore_interrupts(0x80);
}


void setup(void) __attribute__((weak));

void setup(void) {}

int main(void) __attribute__((weak));

int main(void) {
    setup_boot_code();
    setup();

    wdt_reset();
    wdt_enable(WDTO_8S);

    while (true) {
        schedule();
    }
    return 0;
}

extern "C" {
    void __cxa_pure_virtual() {
        fatal(FATAL_PURE_VIRTUAL);
    }
}
