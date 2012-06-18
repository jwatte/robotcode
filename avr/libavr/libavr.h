#if !defined(libavr_h)
#define libavr_h

#if !defined(F_CPU)
#define F_CPU 8000000
#endif

#include <inttypes.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/atomic.h>
#include <util/twi.h>

#define EE_FATALCODE 0
#define EE_RESETSOURCE 1
#define EE_PREV_RESETSOURCE 2
#define EE_NUM_BOOTS 4
#define EE_TOO_LONG_TASK_PTR 6
#define EE_TOO_LONG_TASK_PRE 8
#define EE_TOO_LONG_TASK_POST 10

/* user-defined tuning constants */
#define EE_TUNING 32

typedef uint8_t byte;
typedef uint8_t volatile regtype;
typedef uint16_t word;
typedef bool boolean;

//  how many milliseconds are allowed per task?
//  Note: the serial-format LCD is super slow!
//#define MAX_TASK_TIME 100

#define TWI_MAX_SIZE 0x20

/* fatal API */

#define F_MISC(x)       (0x00 | (x))
#define F_SERIAL(x)     (0x10 | (x))
#define F_TWI(x)        (0x20 | (x))
#define F_HARDWARE(x)   (0x30 | (x))
#define F_TIMER(x)      (0x40 | (x))
#define F_ADC(x)        (0x50 | (x))
#define F_UI(x)         (0x60 | (x))

#define FE_TOOBIG       0x1
#define FE_BADPARAM     0x2
#define FE_BADCALL      0x3
#define FE_BUSY         0x4
#define FE_UNEXPECTED   0x5

enum
{
    FATAL_PURE_VIRTUAL =        F_MISC(FE_BADCALL),
    FATAL_BAD_ARGS =            F_MISC(FE_BADPARAM),
    FATAL_UNEXPECTED =          F_MISC(FE_UNEXPECTED),

    FATAL_BAD_SERIAL =          F_SERIAL(FE_BADPARAM),

    FATAL_BAD_PIN =             F_HARDWARE(FE_BADPARAM),

    FATAL_TOO_LONG_DELAY =      F_TIMER(FE_BADPARAM),
    FATAL_OUT_OF_AFTERS =       F_TIMER(FE_TOOBIG),

    FATAL_ADC_BADCALL =         F_ADC(FE_BADCALL),
    FATAL_ADC_BUSY =            F_ADC(FE_BUSY),
    FATAL_ADC_BAD_CHANNEL =     F_ADC(FE_BADPARAM),

    FATAL_TWI_BUSY =            F_TWI(FE_BUSY),
    FATAL_TWI_TOO_BIG =         F_TWI(FE_TOOBIG),
    FATAL_TWI_BAD_CALL =        F_TWI(FE_BADCALL),
    FATAL_TWI_UNEXPECTED =      F_TWI(FE_UNEXPECTED),

    FATAL_UI_BAD_PARAM =        F_UI(FE_BADPARAM),
};

/* calling fatal stops the program cold, recording the reason code in eeprom  */
extern void fatal(int err) __attribute__((noreturn));
extern void fatal_set_blink(void (*func)(bool on));

/* timer API */
/* you can call this to re-initialize the timer with appropriate scaling factor for your CPU speed */
extern void setup_timers(unsigned long f_cpu = F_CPU);
extern unsigned short uread_timer();
extern unsigned short read_timer();
extern void udelay(unsigned short us);
extern void delay(unsigned short ms);


/* task API */

extern void at(unsigned short time, void (*func)(void *data), void *data);
extern void after(unsigned short delay, void (*func)(void *data), void *data);

extern int volatile globalctr;

/* TWI interface */

/* Implement this interface to use the TWIMaster functionality. */
class ITWIMaster {
    public:
        virtual void data_from_slave(unsigned char n, void const *data) = 0;
        virtual void xmit_complete() = 0;
        virtual void nack() = 0;
};
/* Implement this interface if you're a TWI slave. */
class ITWISlave {
    public:
        virtual void data_from_master(unsigned char n, void const *data) = 0;
        virtual void request_from_master(void *o_buf, unsigned char &o_size) = 0;
};
/* Use this interface to talk as a TWI master. */
class TWIMaster {
    public:
        virtual bool is_busy() = 0;
        virtual void send_to(unsigned char n, void const *data, unsigned char addr) = 0;
        virtual void request_from(unsigned char addr, unsigned char count) = 0;
};

/*  Call start_twi_master() to become a master. If you were a slave, that's shut down. */
extern TWIMaster *start_twi_master(ITWIMaster *m);
/*  Call start_twi_slave() to become a slave. If you were a master, that's shut down. */
extern void start_twi_slave(ITWISlave *s, unsigned char addr);
/*  Shut down all TWI without becoming slave/master. */
extern void stop_twi();

/* use a serial port */
extern void uart_setup(unsigned long brate, unsigned long f_cpu = F_CPU);
extern unsigned char uart_send(unsigned char n, void const *data);
extern void uart_send_all(unsigned char n, void const *data);
extern unsigned char uart_available();
extern char uart_getch();
extern unsigned char uart_read(unsigned char n, void *data);
extern void uart_force_out(char ch);

/* ADC support */
extern void adc_setup(bool use_aref = true);
extern bool adc_busy();
extern void adc_read(unsigned char channel, void (*cb)(unsigned char val));

/* Pinchange interrupts */
class IPinChangeNotify {
    public:
        virtual void pin_change(unsigned char bitValue) = 0;
};
extern void on_pinchange(unsigned char pin, IPinChangeNotify *pcn);

/* your replacement for "main" -- you implement this, and typically 
   set up tasks to run using after(). */
extern void setup();

#endif  //  libavr_h

