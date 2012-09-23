
#include <libavr.h>
#include <pins_avr.h>
#include <cmds.h>


info_SensorInput g_actualData;
info_SensorInput g_writeData;

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
    unsigned char inches() const {
        //  12 inches (1 foot) in a millisecond
        unsigned char inchVal = value_ * 12 / 1000;
        if (inchVal > 200) {
            //  more than 5 meters, I don't yet detect it
            inchVal = 255;
        }
        return inchVal;
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
    ++g_actualData.r_iter;
    g_actualData.r_us[nPing] = pings[nPing].inches();
    nPing = (nPing + 1);
    if (nPing == 3) {
        nPing = 0;
    }
    pings[nPing].startRead();
    after(50, read_ping, 0);
}

unsigned char nIr;

/*
#define MIN_IR_VAL 20       //  was 60
#define MAX_IR_VAL 240      //  was 180
#define IR_MIN_DISTANCE 10
#define IR_MAX_DISTANCE 200

unsigned char ir_inches(unsigned char val)
{
    if (val < MIN_IR_VAL) {
        return IR_MAX_DISTANCE;
    }
    if (val > MAX_IR_VAL) {
        return IR_MIN_DISTANCE;
    }
    //  this is not a good formula, really -- but, whatever
    unsigned short inchVal = IR_MIN_DISTANCE + (MAX_IR_VAL - val);
    if (inchVal > IR_MAX_DISTANCE) {
        return IR_MAX_DISTANCE;
    }
    return inchVal & 0xff;
}
*/

void on_ir(unsigned char val)
{
    ++g_actualData.r_iter;
    g_actualData.r_ir[nIr] = val; //ir_inches(val);
    nIr = (nIr + 1);
    if (nIr == 3) {
        nIr = 0;
    }
}

void read_ir(void *)
{
    after(5, read_ir, 0);
    if (adc_busy()) {
        return;
    }
    adc_read(nIr, on_ir);
}

unsigned char state = 0;

void blink_laser(void *p)
{
    static unsigned char pins[4] = {
        7, 6, 5, 3, 
    };
    unsigned char val = pins[state];
    state = (state + 1) & 3;
    digitalWrite(0, (val & 1) ? HIGH : LOW);
    digitalWrite(6, (val & 2) ? HIGH : LOW);
    digitalWrite(7, (val & 4) ? HIGH : LOW);
    after(100, blink_laser, 0);
}

class Slave : public ITWISlave {
    public:
        virtual void data_from_master(unsigned char n, void const *data) {
            //  do nothing
        }
        virtual void request_from_master(void *o_buf, unsigned char &o_size) {
            memcpy(o_buf, &g_actualData, sizeof(g_actualData));
            o_size = sizeof(g_actualData);
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

