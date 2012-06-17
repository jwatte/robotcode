
#include "libavr.h"
#include "pins_avr.h"
#include <util/twi.h>

extern unsigned long actual_f_cpu;

ITWISlave *_twi_slave;

#if NATIVE_TWI

ITWIMaster *_twi_master;

#define READY_SLAVE ((1 << TWEN) | (1 << TWEA) | (1 << TWIE) | (1 << TWINT))
#define UNREADY_SLAVE ((1 << TWEN) | (1 << TWIE) | (1 << TWINT))
#define READY_MASTER ((1 << TWEN) | (1 << TWEA) | (1 << TWIE) | (1 << TWINT))
#define UNREADY_MASTER ((1 << TWEN) | (1 << TWIE) | (1 << TWINT))


class TWI : public TWIMaster {
    public:
        bool is_busy()
        {
            return pending_op;
        }

        void send_to(unsigned char n, void const *data, unsigned char addr)
        {
            if (!_twi_master) {
                fatal(FATAL_TWI_NO_USER);
            }
            if (n > TWI_MAX_SIZE) {
                fatal(FATAL_TWI_SEND_TOO_BIG);
            }
            IntDisable idi;
            if (pending_op) {
                fatal(FATAL_BAD_USAGE);
            }
            pending_op = true;
            //  I need to grab the bus before I use the shared buffer
            TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
            if (n == 0) {
                buf[0] = 0;
                bufend = 1;
            }
            else {
                memcpy(const_cast<char *>(buf), data, n);
                bufend = n;
            }
            bufptr = 0;
            long l = 0;
            while (!(TWCR & (1 << TWINT))) {
                // wait for the bus
                if (++l > 100000) {
                    fatal(FATAL_BUS_ERROR);
                }
            }
            unsigned char twsr = (TWSR & 0xf8);
            if ((twsr != TW_START) && (twsr != TW_REP_START)) {
                fatal(FATAL_BUS_ERROR);
            }
            TWDR = (addr << 1) | TW_WRITE;
            TWCR = READY_MASTER;
        }

        void request_from(unsigned char addr, unsigned char count)
        {
            if (!_twi_master) {
                fatal(FATAL_TWI_NO_USER);
            }
            IntDisable idi;
            if (pending_op) {
                fatal(FATAL_BAD_USAGE);
            }
            pending_op = true;
            //  I need to grab the bus before I use the shared buffer
            TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
            bufptr = 0;
            bufend = 0;
            long l = 0;
            while (!(TWCR & (1 << TWINT))) {
                // wait for the bus
                if (++l > 100000) {
                    fatal(FATAL_BUS_ERROR);
                }
            }
            unsigned char twsr = (TWSR & 0xf8);
            if ((twsr != TW_START) && (twsr != TW_REP_START)) {
                fatal(FATAL_BUS_ERROR);
            }
            TWDR = (addr << 1) | TW_READ;
            TWCR = READY_MASTER;
        }

        volatile unsigned char bufptr;
        volatile unsigned char bufend;
        volatile unsigned char addr;
        volatile bool pending_op;
        volatile char buf[TWI_MAX_SIZE];
};
TWI _twi;

void _twi_call_callback(void *)
{
    if (_twi_slave) {
        _twi_slave->data_from_master(_twi.bufend, const_cast<char *>(_twi.buf));
        _twi.bufend = 0;
        _twi.pending_op = false;
        TWCR = READY_SLAVE;
    }
    else {
        if (!_twi_master) {
            fatal(FATAL_TWI_NO_USER);
        }
        _twi_master->data_from_slave(_twi.bufend, const_cast<char *>(_twi.buf));
        _twi.bufend = 0;
        _twi.pending_op = false;
        TWCR = READY_MASTER;
    }
}

void _twi_call_request(void *)
{
    if (!_twi_slave) {
        fatal(FATAL_TWI_NO_USER);
    }
    unsigned char n = sizeof(_twi.buf);
    _twi_slave->request_from_master(const_cast<char *>(_twi.buf), n);
    _twi.bufptr = 0;
    if (n == 0) {
        _twi.buf[0] = 0;
        n = 1;
    }
    _twi.bufend = n;
    IntDisable idi;
    TWDR = _twi.buf[_twi.bufptr++];
    TWCR = (_twi.bufptr == _twi.bufend) ? UNREADY_SLAVE : READY_SLAVE;
}

void _twi_call_nack(void *)
{
    if (!_twi_master) {
        fatal(FATAL_TWI_NO_USER);
    }
    _twi.pending_op = false;
    _twi_master->nack();
}

void _twi_call_response(void *)
{
    if (!_twi_master) {
        fatal(FATAL_TWI_NO_USER);
    }
    _twi.pending_op = false;
    _twi_master->data_from_slave(_twi.bufend, const_cast<char *>(_twi.buf));
}

ISR(TWI_vect)
{
    //  what state am I in?
    unsigned char status = TWSR & ~0x07;
    switch (status) {
        case TW_SR_SLA_ACK: {
                                TWCR = READY_SLAVE;  //  to other end: go ahead and send the byte
                                _twi.pending_op = true;
                            }
                            break;
        case TW_SR_DATA_ACK:
        case TW_SR_DATA_NACK: {
                                  if (_twi.bufend < sizeof(_twi.buf)) {
                                      _twi.buf[_twi.bufend] = TWDR;
                                      ++_twi.bufend;
                                      //  else I'm full -- can't take any more!
                                  }
                                  TWCR = READY_SLAVE;
                              }
                              break;
        case TW_SR_STOP: {
                             after(0, _twi_call_callback, 0);
                             TWCR = UNREADY_SLAVE; //  ack! can't receive more until the user has responded
                         }
                         break;
        case TW_ST_SLA_ACK: {
                                _twi.pending_op = true;
                                after(0, _twi_call_request, 0);
                                //  turn off interrupts, but don't ack yet
                                TWCR = TWCR & ~((1 << TWINT) | (1 << TWIE));
                            }
                            break;
        case TW_ST_DATA_ACK: {
                                 TWDR = _twi.buf[_twi.bufptr++];
                                 TWCR = (_twi.bufptr >= _twi.bufend) ? UNREADY_SLAVE : READY_SLAVE;
                             }
                             break;
        case TW_ST_LAST_DATA:
        case TW_ST_DATA_NACK: {
                                  _twi.bufptr = 0;
                                  _twi.bufend = 0;
                                  //  I am done!
                                  _twi.pending_op = false;
                                  TWCR = READY_SLAVE;
                              }
                              break;
        case TW_MT_SLA_NACK:
        case TW_MT_DATA_NACK:
        case TW_MR_SLA_NACK:
                              _twi.bufptr = 0;
                              _twi.bufend = 0;
                              after(0, _twi_call_nack, 0);
                              TWCR = READY_MASTER;
                              break;
        case TW_MT_DATA_ACK:
        case TW_MT_SLA_ACK: {
                                if (_twi.bufptr == _twi.bufend) {
                                    _twi.pending_op = false;
                                    TWCR = READY_MASTER | (1 << TWSTO);
                                    //  I am done!
                                    _twi.pending_op = false;
                                }
                                else {
                                    TWDR = _twi.buf[_twi.bufptr++];
                                    TWCR = READY_MASTER;
                                }
                            }
                            break;
        case TW_MR_SLA_ACK:
                            TWCR = READY_MASTER;
                            break;
        case TW_MR_DATA_ACK:
                            if (_twi.bufend < sizeof(_twi.buf)) {
                                _twi.buf[_twi.bufend++] = TWDR;
                                TWCR = (_twi.bufend == sizeof(_twi.buf)) ? UNREADY_MASTER : READY_MASTER;
                            }
                            else {
                                (void)TWDR;
                                TWCR = UNREADY_MASTER | (1 << TWSTO);
                            }
                            break;
        case TW_MR_DATA_NACK:
                            if (_twi.bufend < sizeof(_twi.buf)) {
                                _twi.buf[_twi.bufend++] = TWDR;
                            }
                            else {
                                (void)TWDR;
                            }
                            TWCR = UNREADY_MASTER | (1 << TWSTO);
                            //  I am done!
                            after(0, _twi_call_response, 0);
                            break;
        case TW_NO_INFO:
                            fatal(FATAL_TWI_NO_INFO);
                            /*
                               case TW_SR_ARB_LOST_SLA_ACK:  //  I will not lose arbitration, because there is only one master
                               case TW_SR_ARB_LOST_GCALL_ACK:
                               case TW_SR_GCALL_ACK:
                               case TW_SR_GCALL_DATA_ACK:
                               case TW_SR_GCALL_DATA_NACK:
                               case TW_ST_ARB_LOST_SLA_ACK:
                               case TW_MT_ARB_LOST:
                               case TW_MR_ARB_LOST:
                               case TW_BUS_ERROR:
                             */
        default:
                            /* these should not happen */
                            fatal(status ? status : FATAL_BUS_ERROR);
                            break;
    }
}

TWIMaster *start_twi_master(ITWIMaster *m)
{
    stop_twi();
    _twi_master = m;
    power_twi_enable();
    TWCR = (1 << TWEN) | (1 << TWIE);
    if (actual_f_cpu < 8000000) {
        TWBR = 2; //  400 kHz at 8 Mhz CPU
    }
    else {
        TWBR = (actual_f_cpu / 400000 - 16) / 2;
    }
    return &_twi;
}

void start_twi_slave(ITWISlave *s, unsigned char addr)
{
    stop_twi();
    _twi_slave = s;
    power_twi_enable();
    _twi.addr = addr;
    TWAR = (addr << 1);
    TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
}

void stop_twi()
{
    TWCR = 0;
    power_twi_disable();
    _twi.bufptr = 0;
    _twi.bufend = 0;
    _twi.pending_op = false;
    _twi.addr = 0;
    _twi_master = 0;
    _twi_slave = 0;
}

#else // emulated TWI

static void twis_nop() {
    //  just clear the interrupt
    USISR |= (1 << USIOIF);
    fatal(FATAL_TWI_NO_INFO);
}

void (*_twi_ovf_func)() = &twis_nop;
unsigned char _twi_addr_write;
unsigned char _twi_ptr;
unsigned char _twi_buf_a[TWI_MAX_SIZE];
unsigned char _twi_buf_b[TWI_MAX_SIZE];
unsigned char _twi_sched_ptr;
unsigned char *_twi_buf = _twi_buf_a;

static inline void twi_look_for_start() {
    _twi_ovf_func = &twis_nop;
    USICR = (1 << USISIE) |
        //  TWI mode, hold-low on overflow
        (1 << USIWM1) | (1 << USIWM0) |
        //  Data sampled on positive edge
        (1 << USICS1);
}

static inline void twi_send_ack(unsigned char val) {
    USIDR = val;
    DDRA |= (1 << PA6);
    PORTA &= ~(1 << PA6);
    USISR = 0xe;
}

static void twis_begin_receive();

static void twis_ack_receive() {
    _twi_ovf_func = &twis_begin_receive;
    if (_twi_ptr < sizeof(_twi_buf)) {
        _twi_buf[_twi_ptr] = USIDR;
        _twi_ptr++;
        if (_twi_ptr < sizeof(_twi_buf)) {
            twi_send_ack(0);
        }
    }
    //  else don't send ack
}

static void twis_begin_receive() {
    //  have sent ack -- wait for 8 bits of data
    _twi_ovf_func = &twis_ack_receive;
    DDRA &= ~(1 << PA6);
    USISR = 0;
}

static void twis_address() {
    if (USIDR == _twi_addr_write) {
        //  matches the address to write to me
        _twi_ovf_func = &twis_begin_receive;
        twi_send_ack(0);
    }
    else {
        //  not me!
        twi_look_for_start();
    }
}

static void twi_packet_complete(void *buf) {
    _twi_slave->data_from_master(_twi_sched_ptr, buf);
    _twi_sched_ptr = 0;
}

static void packet_complete() {
    //  don't overwrite a packet that's not yet picked up
    if (_twi_ptr > 0 && _twi_sched_ptr == 0) {
        _twi_sched_ptr = _twi_ptr;
        after(0, twi_packet_complete, _twi_buf);
        _twi_buf = (_twi_buf == _twi_buf_a) ? _twi_buf_b : _twi_buf_a;
    }
    _twi_ptr = 0;
}

volatile bool _twi_has_after;

static void twi_after(void *) {
    IntDisable idi;
    if (USISR & (1 << USIPF)) {
        _twi_has_after = false;
        packet_complete();
        USISR |= (1 << USIPF);
    }
    else {
        after(0, twi_after, 0);
    }
}

static void twi_schedule_after() {
    IntDisable idi;
    if (!_twi_has_after) {
        _twi_has_after = true;
        after(0, twi_after, 0);
    }
}

ISR(USI_START_vect) {
    if (USISR & (1 << USIPF)) {
        //  stop mode detected
        USISR |= (1 << USIPF);
        twi_look_for_start();
    }
    else {
        //  no stop mode -- good to go!
        _twi_ovf_func = &twis_address;
        //  not looking for start mode here
        USICR = (1 << USIOIE) |
            //  TWI mode, hold-low on overflow
            (1 << USIWM1) | (1 << USIWM0) |
            //  Data sampled on positive edge
            (1 << USICS1);
    }
    USISR |= (1 << USISIF);
}

ISR(USI_OVF_vect) {
    if (USICR & (1 << USIPF)) {
        //  stop condition -- cancel what I'm doing
        USICR |= (1 << USIPF);
        packet_complete();
        twi_look_for_start();
    }
    else {
        twi_schedule_after();
        (*_twi_ovf_func)();
    }
    //  clear interrupt
    USISR |= (1 << USIOIF);
}

void start_twi_slave(ITWISlave *s, unsigned char addr)
{
    IntDisable idi;
    _twi_slave = s;
    _twi_addr_write = addr << 1;
    _twi_ptr = 0;
    power_usi_enable();
    DDRA &= ~((1 << PA4) | (1 << PA6));
    //  clear some flags
    USISR = (1 << USISIF) | (1 << USIOIF) | (1 << USIPF) |
        (1 << USIDC);
    twi_look_for_start();
}

#endif

