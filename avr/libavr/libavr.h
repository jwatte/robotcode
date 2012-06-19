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

#include "fatal.h"

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

