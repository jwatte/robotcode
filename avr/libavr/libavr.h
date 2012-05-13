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

//  how many milliseconds are allowed per task?
//  Note: the LCD is super slow!
#define MAX_TASK_TIME 2500

#define TWI_MAX_SIZE 0x10

typedef uint8_t byte;
typedef uint8_t volatile regtype;
typedef uint16_t word;
typedef bool boolean;

/* fatal API */

enum
{
  FATAL_OUT_OF_AFTERS = 1,
  FATAL_TASK_TOOK_TOO_LONG = 2,
  FATAL_BAD_PIN_ARG = 3,
  FATAL_BAD_DELAY_TIME = 4,
  FATAL_TOO_LONG_DELAY = 5,
  FATAL_BAD_SERIAL = 6,
  FATAL_BAD_PARAM = 7,
  FATAL_BAD_USAGE = 8,
  FATAL_TWI_NO_INFO = 9,
  FATAL_TWI_SEND_TOO_BIG = 10,
  FATAL_TWI_NO_USER = 11,
  FATAL_PURE_VIRTUAL = 12,
  FATAL_TWI_ERROR_BASE = 0x18
};

/* calling fatal stops the program cold, recording the reason code in eeprom  */
void fatal(int err) __attribute__((noreturn));
void fatal_set_blink(void (*func)(bool on));

/* timer API */
/* you can call this to re-initialize the timer with appropriate scaling factor for your CPU speed */
void setup_timers(unsigned long f_cpu = F_CPU);
unsigned short uread_timer();
unsigned short read_timer();
void udelay(unsigned short us);
void delay(unsigned short ms);


/* task API */

void at(unsigned short time, void (*func)(void *data), void *data);
void after(unsigned short delay, void (*func)(void *data), void *data);

extern int volatile globalctr;

/* TWI interface */

class ITWIMaster {
public:
  virtual void data_from_slave(unsigned char n, void const *data) = 0;
  virtual void nack() = 0;
};
class ITWISlave {
public:
  virtual void data_from_master(unsigned char n, void const *data) = 0;
  virtual void request_from_master(void *o_buf, unsigned char &o_size) = 0;
};
class TWIMaster {
public:
  virtual bool is_busy() = 0;
  virtual void send_to(unsigned char n, void const *data, unsigned char addr) = 0;
  virtual void request_from(unsigned char addr) = 0;
};
/*  Call start_twi_master() to become a master. If you were a slave, that's shut down. */
TWIMaster *start_twi_master(ITWIMaster *m);
/*  Call start_twi_slave() to become a slave. If you were a master, that's shut down. */
void start_twi_slave(ITWISlave *s, unsigned char addr);
/*  Shut down all TWI without becoming slave/master. */
void stop_twi();

/* use a serial port */
void uart_setup(unsigned long brate, unsigned long f_cpu = F_CPU);
unsigned char uart_send(unsigned char n, void const *data);
void uart_send_all(unsigned char n, void const *data);
unsigned char uart_available();
char uart_getch();
unsigned char uart_read(unsigned char n, void *data);

/* your replacement for "main" */
void setup();

#endif  //  libavr_h

