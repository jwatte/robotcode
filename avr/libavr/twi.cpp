
#include "libavr.h"
#include "pins_avr.h"
#include <util/twi.h>
#include <avr/io.h>
#include <stdio.h>

extern unsigned long actual_f_cpu;

ITWISlave *_twi_slave;

#if NATIVE_TWI

ITWIMaster *_twi_master;


#define READY_SLAVE ((1 << TWEN) | (1 << TWEA) | (1 << TWIE) | (1 << TWINT))
#define UNREADY_SLAVE ((1 << TWEN) | (1 << TWIE) | (1 << TWINT))
#define READY_MASTER ((1 << TWEN) | (1 << TWEA) | (1 << TWIE) | (1 << TWINT))
#define UNREADY_MASTER ((1 << TWEN) | (1 << TWIE) | (1 << TWINT))


static void send_nak(void *);
static void got_data(void *);
static void done_sending(void *);


class TWI : public TWIMaster {
public:
    TWI() :
        s_ptr(0),
        s_end(0),
        m_ptr(0),
        m_end(0),
        m_addr(0) {
    }
    unsigned char s_ptr;
    unsigned char s_end;
    unsigned char m_ptr;
    unsigned char m_end;
    unsigned char m_addr;
    unsigned char s_buf[TWI_MAX_SIZE];
    unsigned char m_buf[TWI_MAX_SIZE];

    /* this is the master interface */

    bool is_busy() {
        return m_addr || (m_ptr > 0);
    }

    void send_to(unsigned char n, void const *data, unsigned char addr) {
        IntDisable idi;
        if (is_busy()) {
            fatal(FATAL_TWI_BUSY);
        }
        if (n > TWI_MAX_SIZE) {
            fatal(FATAL_TWI_TOO_BIG);
        }
        memcpy(m_buf, data, n);
        m_addr = addr << 1;
        m_ptr = 0;
        m_end = n;
        //  please let me know when you have a start condition
        TWCR |= (1 << TWSTA) | (1 << TWIE) | (1 << TWEA);
    }

    void request_from(unsigned char addr, unsigned char count) {
        IntDisable idi;
        if (is_busy()) {
            fatal(FATAL_TWI_BUSY);
        }
        if (count > TWI_MAX_SIZE) {
            fatal(FATAL_TWI_TOO_BIG);
        }
        m_addr = (addr << 1) | 1;
        m_end = count;
        //  please let me know when you have a start condition
        TWCR |= (1 << TWSTA) | (1 << TWIE);
    }

    uint8_t on_nak(uint8_t twcr) {
        if (m_addr) {
            //  master seeing NAK should stop
            twcr &= ~(1 << TWSTA);
            twcr |= (1 << TWSTO);
            after(0, send_nak, 0);
        }
        return twcr;
    }

    uint8_t on_start(uint8_t twcr) {
        twcr |= (1 << TWEN);
        twcr &= ~(1 << TWSTA);
        TWDR = m_addr;
        return twcr;
    }

    uint8_t on_mt_addr(uint8_t twcr) {
        //  write request
        return on_mt_data(twcr);
    }

    uint8_t on_mt_data(uint8_t twcr) {
        if (m_ptr < m_end) {
            TWDR = m_buf[m_ptr++];
        }
        else {
            twcr |= (1 << TWSTO);
            after(0, done_sending, 0);
        }
        return twcr;
    }

    uint8_t on_mr_addr(uint8_t twcr) {
        twcr |= (1 << TWEA);
        return twcr;
    }

    uint8_t on_mr_data(uint8_t twcr) {
        m_buf[m_ptr++] = TWDR;
        twcr &= ~(1 << TWEA);
        if (m_ptr == m_end) {
            twcr |= (1 << TWSTO);
            after(0, got_data, 0);
        }
        else if (m_ptr < m_end - 1) {
            twcr |= (1 << TWEA);
        }
        return twcr;
    }

    uint8_t on_sr_addr(uint8_t twcr) {
        return twcr;
    }

    uint8_t on_sr_data(bool ack, uint8_t twcr) {
        return twcr;
    }

    uint8_t on_sr_end(uint8_t twcr) {
        return twcr;
    }
};

static TWI _twi;

static void send_nak(void *) {
    {
        IntDisable idi;
        _twi.m_addr = 0;
        _twi.m_ptr = _twi.m_end = 0;
    }
    _twi_master->nack();
}

static void got_data(void *) {
    unsigned char n;
    {
        IntDisable idi;
        n = _twi.m_ptr;
        _twi.m_addr = 0; _twi.m_ptr = _twi.m_end = 0;
    }
    _twi_master->data_from_slave(n, _twi.m_buf);
}

static void done_sending(void *) {
    {
        IntDisable idi;
        //  if still not done stopping the bus, try again soon
        if (TWCR & (1 << TWSTO)) {
            after(0, done_sending, 0);
            return;
        }
        _twi.m_addr = 0;
        _twi.m_ptr = _twi.m_end = 0;
    }
    _twi_master->xmit_complete();
}

ISR(TWI_vect) {
    //  I've gotten some state change from the TWI circuitry
    uint8_t twcr = TWCR;
    switch (TWSR & 0xf8) {
    /* master transmitter */
    case 0x08:  //  START
    case 0x10:  //  REPEATED START
        twcr = _twi.on_start(twcr);
        break;
    case 0x18:  //  addr + ACK
        twcr = _twi.on_mt_addr(twcr);
        break;
    case 0x20:  //  addr + NAK
        twcr = _twi.on_nak(twcr);
        break;
    case 0x28:  //  data byte
        twcr = _twi.on_mt_data(twcr);
        break;
    case 0x30:  //  data + NAK
        twcr = _twi.on_nak(twcr);
        break;
    /* master receiver */
    case 0x38:  //  NAK or lost arbitration
        twcr = _twi.on_nak(twcr);
        break;
    case 0x40:  //  addr + ACK
        twcr = _twi.on_mr_addr(twcr);
        break;
    case 0x48:  //  addr + NAK
        twcr = _twi.on_nak(twcr);
        break;
    case 0x50:
    case 0x58:
        twcr = _twi.on_mr_data(twcr);
        break;
    /* slave receiver */
    case 0x60:
        twcr = _twi.on_sr_addr(twcr);
        break;
    case 0x80:
        twcr = _twi.on_sr_data(true, twcr);
        break;
    case 0x88:
        twcr = _twi.on_sr_data(false, twcr);
        break;
    case 0xa0:
        twcr = _twi.on_sr_end(twcr);
        break;
    case 0x68:  //  lost arbitration, addressed
    case 0x70:  //  general call
    case 0x78:  //  lost arbitration, general call
    case 0x90:  //  general call data
    case 0x98:  //  general call data + nak
    case 0xf8:
    case 0x0:
    default:
        twcr = _twi.on_nak(twcr);
        break;
    }
    //  Finally, clear the interrupt -- always do this last
    TWCR = twcr;
}

TWIMaster *start_twi_master(ITWIMaster *m)
{
    _twi_master = m;
    power_twi_enable();
    /* Run at 400 kHz. See data sheet: */
    /* 400 kHz == f_cpu / (16 + 2 * twbr * twsr) */
    /* 2 * twbr * twsr == f_cpu / 400 kHz - 16 */
    /* twbr * twsr == f_cpu / 800 kHz - 8 */
    /* twsr == 0 means twsr scaler value 1 */
    DDRC &= ~0x30;
    PORTC |= 0x30;  //  internal pull-ups
    TWSR = 0;
    if (actual_f_cpu < 6400000) {
        TWBR = 0;
    }
    else {
        TWBR = (actual_f_cpu / 800000) - 8;
    }
    TWCR |= (1 << TWEN) | (1 << TWINT) | (1 << TWIE);
    return &_twi;
}

/*  Call start_twi_slave() to become a slave. If you were a master, that's shut down. */
void start_twi_slave(ITWISlave *s, unsigned char addr)
{
    _twi_slave = s;
    power_twi_enable();
    DDRC &= ~0x30;
    PORTC |= 0x30;  //  internal pull-ups
    TWAR = addr << 1;
    TWAMR = 0;
    TWCR |= (1 << TWEN) | (1 << TWINT) | (1 << TWIE) | (1 << TWEA);
}

/*  Shut down all TWI without becoming slave/master. */
void stop_twi()
{
    TWCR = 0;
    DDRC &= ~0x30;
    power_twi_disable();
    _twi_master = 0;
    _twi_slave = 0;
}


#endif

