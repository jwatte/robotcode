
#include <libavr.h>
#include <pins_avr.h>
#include <cmds.h>


void read_ping(void *);
void read_ir(void *);

void blink(bool on) {
    DDRB |= 0x6;
    if (on) {
        PORTB |= 0x6;
    }
    else {
        PORTB |= 0x4;
        PORTB &= ~0x2;
    }
}

class Ping4 : public IPinChangeNotify {
    public:
        Ping4(unsigned char pinTrigger, unsigned char pinPulse) :
            pinTrigger_(pinTrigger),
            pinPulse_(pinPulse)
    {
        pinMode(pinTrigger, OUTPUT);
        digitalWrite(pinTrigger_, LOW); //  don't trigger yet
        pinMode(pinPulse, INPUT);
        digitalWrite(pinPulse_, LOW); //  no pull-up
        //  hook up pin change interrupt
        on_pinchange(pinPulse_, this);
    }
        unsigned char pinTrigger_;
        unsigned char pinPulse_;
        unsigned short value_;
        unsigned short readTimer_;
        void startRead() {
            //  trigger read signal
            IntDisable idi;
            digitalWrite(pinTrigger_, HIGH);
            udelay(11);
            digitalWrite(pinTrigger_, LOW);
        }
        void pin_change(unsigned char level) {
            if (level) {
                readTimer_ = uread_timer();
                PORTB |= 0x4;
            }
            else {
                value_ = uread_timer() - readTimer_;
                PORTB &= ~0x4;
            }
        }
};

Ping4 pings[3] = {
    Ping4(16|5, 16|2),  //  front
    Ping4(16|6, 16|3),  //  left
    Ping4(16|7, 16|4)   //  right
};
unsigned char nPing = 0;

void read_ping(void *)
{
    nPing = (nPing + 1);
    if (nPing == 3) {
        nPing = 0;
    }
    pings[nPing].startRead();
    after(50, read_ping, 0);
}

unsigned char nIr;
unsigned char irVal[3];

void on_ir(unsigned char val)
{
    irVal[nIr] = val;
    nIr = (nIr + 1);
    if (nIr == 3) {
        nIr = 0;
    }
}

void read_ir(void *)
{
    after(1, read_ir, 0);
    if (adc_busy()) {
        return;
    }
    adc_read(nIr, on_ir);
}

unsigned char state = 0;

void blink_laser(void *p)
{
    static unsigned char pins[8] = {
        0, 1, 3, 7, 7, 6, 4, 0
    };
    unsigned char val = pins[state];
    state = (state + 1) & 7;
    digitalWrite(0, (val & 1) ? HIGH : LOW);
    digitalWrite(6, (val & 2) ? HIGH : LOW);
    digitalWrite(7, (val & 4) ? HIGH : LOW);
    after(200, blink_laser, 0);
}

class Slave : public ITWISlave {
    public:
        virtual void data_from_master(unsigned char n, void const *data) {
            //  do nothing
        }
        virtual void request_from_master(void *o_buf, unsigned char &o_size) {
            SensorOutput so;
            for (unsigned char i = 0; i != 3; ++i) {
                so.usDistance[i] = pings[i].value_ / 59;  //  microseconds to centimeters
                so.irDistance[i] = irVal[i];  //  todo: correct for distance curve
            }
            memcpy(o_buf, &so, sizeof(so));
            o_size = sizeof(so);
        }
};
Slave slave;

void setup() {
    DDRB |= 0x6;
    fatal_set_blink(blink);
    pinMode(0, OUTPUT);
    pinMode(6, OUTPUT);
    pinMode(7, OUTPUT);
    adc_setup(true);
    blink_laser(0);
    read_ping(0);
    read_ir(0);
    start_twi_slave(&slave, NodeSensorInput);
}

