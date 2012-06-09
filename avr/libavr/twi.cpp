
#include "libavr.h"
#include "pins_avr.h"
#include <util/twi.h>

extern unsigned long actual_f_cpu;

ITWIMaster *_twi_master;
ITWISlave *_twi_slave;

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

        void request_from(unsigned char addr)
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
                                TWCR = UNREADY_MASTER;
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


