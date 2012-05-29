
#include <libavr.h>
#include <nRF24L01.h>
#include <stdio.h>

#include "cmds.h"


/*  less than 6.5V in the battery pack, and I can't run. */
#define THRESHOLD_VOLTAGE 0x68
#define VOLTAGE_SCALER 102  //  was 109

/* For some reason, running the servo on PWM is not very clean. */
/* Perhaps an approach that uses timer1 interrupts for high resolution */
/* would be better. But for now, I just schedule 50 Hz updates. */
#define USE_SERVO_TIMER 0

#define LED_GO_B (1 << PB0)
#define LED_PAUSE_D (1 << PD7)

nRF24L01<false, 0|7, 16|4, 0|6> rf;

class RfInt : public IPinChangeNotify {
    void pin_change(unsigned char) {
        rf.onIRQ();
    }
};
RfInt rf_int;

uint8_t nRadioContact = 1;


struct TuneStruct {
    char d_steer;
    char m_power;
    unsigned char cksum;
};
TuneStruct g_tuning;

unsigned char calc_cksum(unsigned char sz, void const *src)
{
    unsigned char cksum = 0x55;
    unsigned char const *d = (unsigned char const *)src;
    for (unsigned char ix = 0; ix != sz; ++ix) {
        cksum = ((cksum << 1) + ix) ^ d[ix];
    }
    return cksum;
}

void write_tuning()
{
    g_tuning.cksum = calc_cksum(sizeof(g_tuning)-1, &g_tuning);
    eeprom_write_block(&g_tuning, (void *)EE_TUNING, sizeof(g_tuning));
}

void read_tuning()
{
    eeprom_read_block(&g_tuning, (void const *)EE_TUNING, sizeof(g_tuning));
    if (g_tuning.cksum != calc_cksum(sizeof(g_tuning)-1, &g_tuning)) {
        memset(&g_tuning, 0, sizeof(g_tuning));
        g_tuning.d_steer = -20;
        g_tuning.m_power = 128;
        write_tuning();
    }
}




int g_led_timer;
bool g_led_paused;
bool g_led_go;
bool g_led_blink;
bool g_after;

void blink_leds(bool on)
{
    DDRB |= LED_GO_B;
    DDRD |= LED_PAUSE_D;
    if (on) {
        PORTB |= LED_GO_B;
        PORTD |= LED_PAUSE_D;
    }
    else {
        PORTB &= ~LED_GO_B;
        PORTD &= ~LED_PAUSE_D;
    }
}

void setup_leds()
{
    blink_leds(false);
    fatal_set_blink(&blink_leds);
}

void update_leds(void *)
{
    g_after = false;
    if (g_led_timer) {
        g_led_blink = !g_led_blink;
    }
    else {
        g_led_blink = true;
    }
    if (g_led_go && g_led_blink) {
        PORTB = PORTB | LED_GO_B;
    }
    else {
        PORTB = PORTB & ~LED_GO_B;
    }
    if (g_led_paused && g_led_blink) {
        PORTD = PORTD | LED_PAUSE_D;
    }
    else {
        PORTD = PORTD & ~LED_PAUSE_D;
    }
    if (g_led_timer && !g_after) {
        g_after = true;
        after(g_led_timer, &update_leds, 0);
    }
}

void set_led_state(bool paused, bool go, int blink)
{
    g_led_timer = blink;
    g_led_paused = paused;
    g_led_go = go;
    if (!g_after) {
        g_after = true;
        after(0, &update_leds, 0);
    }
}


#define MOTOR_A_PCH_D (1 << PD5)
#define MOTOR_A_NCH_D (1 << PD6)
#define MOTOR_B_PCH_B (1 << PB2)
#define MOTOR_B_NCH_B (1 << PB1)

int g_motor_actual_power;
int g_motor_desired_power;
unsigned char g_motor_allowed = false;
unsigned char g_local_stop = false;

//  These were taken at 5.0V regulation.
//  Now, there's a Schottky with 0.32V drop-out in the way.
//  0xcc == 7.93V -> 8.47V
//  0xb4 == 7.01V -> 7.49V
//  0x9f == 6.20V -> 6.62V
//  0x98 == 5.45V -> 5.82V

unsigned char g_voltage;

void setup_motors()
{
    power_timer0_enable();
    TIMSK0 = 0;
    TIFR0 = 0x7;

    PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_A_NCH_D));
    PORTB = (PORTB & ~(MOTOR_B_PCH_B | MOTOR_B_NCH_B));
    DDRD |= (MOTOR_A_PCH_D | MOTOR_A_NCH_D);
    DDRB |= (MOTOR_B_PCH_B | MOTOR_B_NCH_B);
    TCCR0A = 0x03;  //  Fast PWM, not yet turned on
    TCCR0B = 0x3;   //  0.5 kHz (0x2 is 4 kHz)
    OCR0A = 128;
    OCR0B = 128;
}

void update_motor_power()
{
    int power = g_motor_desired_power;
    if (!nRadioContact || !g_motor_allowed || g_local_stop) {
        power = 0;
    }
    if (!((power == 0 && g_motor_actual_power == 0) ||
                (power > 0 && g_motor_actual_power > 0) ||
                (power < 0 && g_motor_actual_power < 0))) {
        TCCR0A = 0x03;  //  Fast PWM, not yet turned on
        PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_A_NCH_D));
        PORTB = (PORTB & ~(MOTOR_B_PCH_B | MOTOR_B_NCH_B));
        udelay(10); // prevent shooth-through
    }
    g_motor_actual_power = power;
    if (power == 0) {
        //  ground everything out
        PORTD = (PORTD & ~(MOTOR_A_PCH_D)) | MOTOR_A_NCH_D;
        PORTB = (PORTB & ~(MOTOR_B_PCH_B)) | MOTOR_B_NCH_B;
        TCCR0A = 0x03;  //  Fast PWM, not yet turned on
        if (!nRadioContact) {
            set_led_state(true, false, 1200);
        }
        else if (!g_motor_allowed || g_local_stop) {
            set_led_state(true, false, 400);
        }
        else {
            set_led_state(true, false, 0);
        }
    }
    else if (power < 0) {
        //  negative A, positive B
        set_led_state(false, true, 200);
        PORTB = (PORTB & ~(MOTOR_B_NCH_B)) | MOTOR_B_PCH_B;
        //PORTD = (PORTD & ~(MOTOR_A_PCH_D)) | MOTOR_A_NCH_D;
        //  Note: tuning 255 means "full power," 0 means "almost no power"
        OCR0A = (((power < -255) ? 255 : -power) * (g_tuning.m_power + 1)) >> 8;
        TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);  //  Fast PWM, channel A
    }
    else {
        //  positive A, negative B
        set_led_state(false, true, 0);
        PORTB = (PORTB & ~(MOTOR_B_PCH_B)) | MOTOR_B_NCH_B;
        //PORTD = (PORTD & ~(MOTOR_A_NCH_D)) | MOTOR_A_PCH_D;
        OCR0B = (((power > 255) ? 255 : power) * g_tuning.m_power) >> 8;
        TCCR0A = (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);  //  Fast PWM, channel B
    }
}

void set_motor_power(int power)
{
    g_motor_desired_power = power;
    update_motor_power();
}

unsigned char g_steering_angle = 90;

unsigned char tuned_angle()
{
    int i = (int)g_steering_angle + g_tuning.d_steer;
    if (i < 0) i = 0;
    if (i > 180) i = 180;
    return (unsigned char)i;
}

#if USE_SERVO_TIMER
void set_servo_timer_angle(unsigned char a)
{
    //  this is very approximately the angle
    OCR2B = tuned_angle() / 3 + 16;
}
#endif

void setup_servo()
{
    DDRD |= (1 << PD3);
    PORTD &= ~(1 << PD3);
#if USE_SERVO_TIMER
    power_timer2_enable();
    ASSR = 0;
    //  Fast PWM on COM2B1 pin
    TCCR2A = (1 << COM2B1) | (0 << COM2B0) | (1 << WGM21) | (1 << WGM20);
    //  120 Hz
    TCCR2B = (1 << CS22) | (1 << CS21) | (0 << CS20);
    set_servo_timer_angle(g_steering_angle);
#endif
}

void update_servo(void *v)
{
#if USE_SERVO_TIMER
    //  this is very approximate
    set_servo_timer_angle(g_steering_angle);
#else
    {
        IntDisable idi;
        PORTD |= (1 << PD3);
        udelay(tuned_angle() * 11U + 580U);
        PORTD &= ~(1 << PD3);
    }
#endif
    //  2 ms spin delay every 40 ms is 5% of available CPU...
    after(40, &update_servo, 0);
}


void setup_buttons()
{
    DDRD &= ~(1 << PD2);
    PORTD |= (1 << PD2);
}

void poll_button(void *)
{
    if (!(PIND & (1 << PD2))) {
        g_local_stop++;
        update_motor_power();
    }
    else if (g_local_stop > 20) {
        //  start again by holding for 2 seconds
        g_local_stop = 0;
        update_motor_power();
    }
    else if (g_local_stop > 0) {
        g_local_stop = 1;
        update_motor_power();
    }
    after(100, &poll_button, 0);
}


void tune_steering(cmd_parameter_value const &cpv)
{
    g_tuning.d_steer = cpv.value[0];
    write_tuning();
}

void tune_power(cmd_parameter_value const &cpv)
{
    g_tuning.m_power = cpv.value[0];
    write_tuning();
}

void dispatch_cmd(unsigned char n, char const *data)
{
    cmd_hdr const &hdr = *(cmd_hdr const *)data;
    if (hdr.toNode == NodeMotorPower) {
        if (hdr.cmd == CMD_STOP_GO) {
            g_motor_allowed = ((cmd_stop_go const &)hdr).go;
        }
        else if (hdr.cmd == CMD_PARAMETER_VALUE) {
            cmd_parameter_value const &cpv = (cmd_parameter_value const &)hdr;
            switch (cpv.parameter) {
                case ParamTuneSteering:
                    tune_steering(cpv);
                    break;
                case ParamTunePower:
                    tune_power(cpv);
                    break;
            }
        }
        update_motor_power();
    }
}

void reset_radio(void *)
{
    if (!nRadioContact) {
        set_led_state(false, false, 0);
        blink_leds(true);
        rf.teardown();
        blink_leds(false);
        delay(100);
        blink_leds(true);
        rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);
        blink_leds(false);
        after(8000, &reset_radio, 0);
    }
}

void poll_radio(void *)
{
    unsigned char n = rf.hasData();
    if (n > 0) {
        nRadioContact = 20;
        char buf[32];
        rf.readData(n, buf);
        dispatch_cmd(n, buf);
    }
    else if (nRadioContact > 0) {
        --nRadioContact;
        if (nRadioContact == 0) {
            after(100, &reset_radio, 0);
        }
    }
    update_motor_power();
    after(50, &poll_radio, 0);
}


unsigned char curSendParam = 0;

bool hasTWIData = false;
unsigned char twiData[12];

void send_param(unsigned char p)
{
    cmd_parameter_value cpv;
    cpv.cmd = CMD_PARAMETER_VALUE;
    cpv.fromNode = NodeMotorPower;
    cpv.toNode = NodeAny;
    cpv.parameter = p;

    switch (p) {
        case ParamGoAllowed:
            set_value(cpv, (unsigned char)(g_motor_allowed && !g_local_stop));
            break;
        case ParamMotorPower:
            //  The range for motor power is "PWM duty cycle"
            set_value(cpv, (int)g_motor_desired_power);
            break;
        case ParamSteerAngle:
            //  the range for steering is "128 in center"
            set_value(cpv, (unsigned char)(g_steering_angle * 128 / 90));
            break;
        case ParamEEDump:
            //  For debugging, dump latest TWI packet
            if (hasTWIData) {
                memcpy(&cpv.value[1], twiData, 12);
            }
            else {
                eeprom_read_block(&cpv.value[1], (void const *)0, 12);
            }
            cpv.value[0] = 12;
            cpv.type = TypeRaw;
            break;
        case ParamTuneSteering:
            //  the range for steering tuning is simply "degrees"
            set_value(cpv, (unsigned char)g_tuning.d_steer);
            break;
        case ParamTunePower:
            //  The range for power is "PWM duty cycle multiplier"
            set_value(cpv, (unsigned char)g_tuning.m_power);
            break;
        case ParamVoltage:
            //  The range for voltage is "4-bit fractional Volts"
            set_value(cpv, g_voltage);
            break;
    }

    rf.writeData(param_size(cpv), &cpv);
}

void slow_bits_update(void *v)
{
    if (rf.canWriteData()) {
        send_param(curSendParam);
        curSendParam++;
        if (curSendParam == ParamMax) {
            curSendParam = 0;
        }
    }
    after(175, &slow_bits_update, 0);
}

void setup_power()
{
    power_adc_enable();
    ADCSRA |= (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);  //  enable, prescaler @ 125 kHz
    ADMUX |= (1 << ADLAR) | (1 << MUX0);  //  ADC1
    DIDR0 |= (1 << ADC1D);  //  disable ADC1 as digital
}

void poll_power(void *);

void poll_power_result(void *)
{
    //  8 bits -- just read high half
    unsigned short adcValue = (unsigned short)(unsigned char)ADCH;
    g_voltage = (adcValue << 6) / VOLTAGE_SCALER;
    if (g_voltage < THRESHOLD_VOLTAGE) {
        //  stopping locally -- out of juice!
        g_local_stop = true;
    }
    update_motor_power();
    after(500, &poll_power, 0);
}

void poll_power(void *)
{
    ADCSRA |= (1 << ADSC) | (1 << ADIF);  //  start conversion, clear interrupt flag
    after(1, &poll_power_result, 0);
}

class MySlave : public ITWISlave {
    public:
        virtual void data_from_master(unsigned char n, void const *data) {
            if (n > sizeof(twiData)) {
                n = sizeof(twiData);
            }
            memcpy(twiData, data, n);
            hasTWIData = true;
        }
        virtual void request_from_master(void *o_buf, unsigned char &o_size) {
            ((char *)o_buf)[0] = g_voltage;
            ((char *)o_buf)[1] = (unsigned char)(g_motor_desired_power >> 1);
            ((char *)o_buf)[2] = g_motor_actual_power;
            o_size = 3;
        }
};
MySlave twiSlave;

void setup()
{
    read_tuning();
    setup_leds();
    setup_motors();
    setup_servo();
    setup_buttons();
    setup_power();
    uart_setup(115200, F_CPU);
    delay(100); //  wait for radio to boot
    rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);
    on_pinchange(rf.getPinIRQ(), &rf_int);
    start_twi_slave(&twiSlave, NodeMotorPower);
    //  kick off the chain of tasks
    update_servo(0);
    poll_radio(0);
    slow_bits_update(0);
    poll_button(0);
    poll_power(0);
}


