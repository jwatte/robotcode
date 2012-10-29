
#include <libavr.h>
#include <pins_avr.h>
//#include <nRF24L01.h>
#include <stdio.h>

#include "cmds.h"


#define NUM_LOW_VOLTAGE_TO_CARE 50

/*  less than 6.5V in the battery pack, and I can't run. */
#define VOLTAGE_PIN 1
//  When to turn off motor because of undervoltage
#define THRESHOLD_VOLTAGE 0x68
//  When to signal power-off to power control to protect batteries
#define OFF_VOLTAGE 0x67    //  3.2V*2
//  8.2V is 164 in the reading
//  formula is (rdbyte << 6) / scaler
//  8.2 is 0x83, 150 is 0xA4, so relation is 0x83 == (0xA4 * 64) / scaler
//  scaler = 0xA4 * 64 / 0x83 == 80
#define VOLTAGE_SCALER 80

#define LED_GO_B (1 << PB0)
#define LED_PAUSE_D (1 << PD7)

//  Forward: MOTOR_A_PCH_D + MOTOR_B_NCH_B
//  Backward: MOTOR_B_PCH_D + MOTOR_A_NCH_B

#define MOTOR_A_PCH_D (1 << PD5)
#define MOTOR_A_NCH_B (1 << PB2)
#define MOTOR_B_PCH_D (1 << PD6)
#define MOTOR_B_NCH_B (1 << PB1)

//  PC0
#define POWEROFF_PIN (8|0)

#define BTN_A (1 << 2)

//nRF24L01<false, 0|7, 16|4, 0|6> rf;

/*
class RfInt : public IPinChangeNotify {
    void pin_change(unsigned char) {
        rf.onIRQ();
    }
};
RfInt rf_int;
*/

info_MotorPower g_write_state;
info_MotorPower g_actual_state;

bool powerfail;
bool btn_power;


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
    g_tuning.d_steer = g_actual_state.w_trim_steer;
    g_tuning.m_power = g_actual_state.w_trim_power;
    g_tuning.cksum = calc_cksum(sizeof(g_tuning)-1, &g_tuning);
    eeprom_write_block(&g_tuning, (void *)EE_TUNING, sizeof(g_tuning));
}

bool g_tuningWriteScheduled = false;

void on_write_tuning(void *)
{
    g_tuningWriteScheduled = false;
    write_tuning();
}

void schedule_write_tuning()
{
    if (!g_tuningWriteScheduled)
    {
        g_tuningWriteScheduled = true;
        after(5000, on_write_tuning, 0);
    }
}

void read_tuning()
{
    eeprom_read_block(&g_tuning, (void const *)EE_TUNING, sizeof(g_tuning));
    if (g_tuning.cksum != calc_cksum(sizeof(g_tuning)-1, &g_tuning)) {
        memset(&g_tuning, 0, sizeof(g_tuning));
        g_tuning.d_steer = -32;
        g_tuning.m_power = 128;
        g_actual_state.w_trim_steer = g_write_state.w_trim_steer = g_tuning.d_steer;
        g_actual_state.w_trim_power = g_write_state.w_trim_power = g_tuning.m_power;
        write_tuning();
    }
    g_actual_state.w_trim_steer = g_write_state.w_trim_steer = g_tuning.d_steer;
    g_actual_state.w_trim_power = g_write_state.w_trim_power = g_tuning.m_power;
}




enum LED_state {
    LED_powerfail = 0x1,
    LED_connected = 0x2,
    LED_forward = 0x4,
    LED_backward = 0x8,
};
bool g_led_blink;
bool g_after;
unsigned char g_led_bits = 0;

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

    //  kill motor power to be safe
    PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_B_PCH_D));
    PORTB = (PORTB | MOTOR_A_NCH_B | MOTOR_B_NCH_B);
    DDRD |= (MOTOR_A_PCH_D | MOTOR_B_PCH_D);
    DDRB |= (MOTOR_A_NCH_B | MOTOR_B_NCH_B);
}

void setup_leds()
{
    blink_leds(false);
    fatal_set_blink(&blink_leds);
}

void update_leds(void *)
{
    g_after = false;

    //  default
    int delay = 400;
    bool ledb = g_led_blink;
    bool ledd = false;

    if (g_led_bits & LED_powerfail) {
        delay = 100;
        ledb = g_led_blink;
        ledd = g_led_blink;
    }
    else if (g_led_bits & LED_forward) {
        delay = 0;
        ledb = false;
        ledd = true;
    }
    else if (g_led_bits & LED_backward) {
        delay = 400;
        ledb = false;
        ledd = g_led_blink;
    }
    else if (g_led_bits & LED_connected) {
        delay = 0;
        ledb = true;
        ledd = false;
    }

    if (delay) {
        g_led_blink = !g_led_blink;
    }
    else {
        g_led_blink = true;
    }

    if (ledb) {
        PORTB = PORTB | LED_GO_B;
    }
    else {
        PORTB = PORTB & ~LED_GO_B;
    }

    if (ledd) {
        PORTD = PORTD | LED_PAUSE_D;
    }
    else {
        PORTD = PORTD & ~LED_PAUSE_D;
    }

    if (delay && !g_after) {
        g_after = true;
        after(delay, &update_leds, 0);
    }
}

void set_led_bits(unsigned char state)
{
    g_led_bits = state;
    if (!g_after) {
        g_after = true;
        after(0, &update_leds, 0);
    }
}


//  These were taken at 5.0V regulation.
//  Now, there's a Schottky with 0.32V drop-out in the way.
//  0xcc == 7.93V -> 8.47V
//  0xb4 == 7.01V -> 7.49V
//  0x9f == 6.20V -> 6.62V
//  0x98 == 5.45V -> 5.82V

void setup_motors()
{
    power_timer0_enable();
    TIMSK0 = 0;
    TIFR0 = 0x7;

    PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_B_PCH_D));
    PORTB = (PORTB | MOTOR_A_NCH_B | MOTOR_B_NCH_B);
    DDRD |= (MOTOR_A_PCH_D | MOTOR_B_PCH_D);
    DDRB |= (MOTOR_A_NCH_B | MOTOR_B_NCH_B);
    TCCR0A = 0x03;  //  Fast PWM, not yet turned on
    TCCR0B = 0x03;   //  0x3 is 0.5 kHz, 0x2 is 4 kHz, 0x1 is 32 kHz
    OCR0A = 128;
    OCR0B = 128;
}

void update_motor_power()
{
    char power = (char)g_actual_state.w_cmd_power;
    if (!g_actual_state.r_e_conn || !g_actual_state.w_e_allow ||
        g_actual_state.r_self_stop || powerfail) {
        power = 0;
    }
    if (btn_power) {
        power = 64;
    }
    if (!((power == 0 && g_actual_state.r_actual_power == 0) ||
                (power > 0 && g_actual_state.r_actual_power > 0) ||
                (power < 0 && g_actual_state.r_actual_power < 0))) {
        //  turn everything off in preparation for change
        TCCR0A = 0x03;  //  Fast PWM, not yet turned on
        PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_B_PCH_D));
        PORTB = (PORTB & ~(MOTOR_A_NCH_B | MOTOR_B_NCH_B));
        udelay(50); // prevent shooth-through
    }
    if (power > g_actual_state.r_actual_power) {
        //  ramp up over time to avoid spikes
        int v = g_actual_state.r_actual_power + 2;
        if (v < power) {
            power = v;
        }
    }
    g_actual_state.r_actual_power = power;
    unsigned char led = 0;
    if (powerfail) {
        led |= LED_powerfail;
    }
    if (g_actual_state.r_e_conn) {
        led |= LED_connected;
    }
    if (power == 0) {
        //  ground everything out
        PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_B_PCH_D));
        PORTB = (PORTB | MOTOR_A_NCH_B | MOTOR_B_NCH_B);
        TCCR0A = 0x03;  //  Fast PWM, not yet turned on
    }
    else if (power < 0) {
        if (power < -127) {
            power = -127;
        }
        led |= LED_backward;
        PORTB = (PORTB & ~(MOTOR_A_NCH_B)) | (MOTOR_B_NCH_B);
        PORTD = (PORTD & ~(MOTOR_B_PCH_D)) | (MOTOR_A_PCH_D);
        //  Note: tuning 255 means "full power," 0 means "almost no power"
        OCR0B = (-(int)power * (g_actual_state.w_trim_power + 1)) >> 7;
        TCCR0A = (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);  //  Fast PWM, channel B
    }
    else {
        if (power > 127) {
            power = 127;
        }
        led |= LED_forward;
        PORTB = (PORTB & ~(MOTOR_B_NCH_B)) | (MOTOR_A_NCH_B);
        PORTD = (PORTD & ~(MOTOR_A_PCH_D)) | (MOTOR_B_PCH_D);
        OCR0A = ((int)power * (g_actual_state.w_trim_power + 1)) >> 7;
        TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);  //  Fast PWM, channel A
    }
    set_led_bits(led);
}

unsigned char tuned_angle()
{
    int i = (int)(char)g_actual_state.w_cmd_steer + (char)g_actual_state.w_trim_steer + 90;
    if (i < 0) i = -0;
    if (i > 180) i = 180;
    return (unsigned char)i;
}

void setup_servo()
{
    DDRD |= (1 << PD3);
    PORTD &= ~(1 << PD3);
}

void actually_write_servo()
{
    IntDisable idi;
    PORTD |= (1 << PD3);
    udelay(tuned_angle() * 11U + 580U);
    PORTD &= ~(1 << PD3);
}

void update_servo(void *v)
{
    //  if killed from button, also kill servo
    if (!g_actual_state.r_self_stop)
    {
        actually_write_servo();
    }
    //  2 ms spin delay every 30 ms is 7.5% of available CPU.
    after(30, &update_servo, 0);
}


void setup_buttons()
{
    // pull up pause button
    DDRD &= ~(1 << PD2);
    PORTD |= (1 << PD2);
    DDRC &= BTN_A;
    PORTC |= BTN_A; //  pull-up
}

void poll_button(void *)
{
    //  If the button is held in, increment the stop counter
    if (!(PIND & (1 << PD2))) {
        g_actual_state.r_self_stop++;
    }
    else if (g_actual_state.r_self_stop > 20) {
        //  start again by holding for 2 seconds, and letting go
        g_actual_state.r_self_stop = 0;
    }
    else if (g_actual_state.r_self_stop > 0) {
        g_actual_state.r_self_stop = 1;
    }
    if (!(PINC & BTN_A)) {
        btn_power = true;
    }
    else {
        btn_power = false;
    }
    update_motor_power();
    after(100, &poll_button, 0);
}

void apply_state()
{
    bool power = false;
    bool steering = false;
    bool tuning = false;
    if (g_actual_state.w_cmd_power != g_write_state.w_cmd_power)
    {
        g_actual_state.w_cmd_power = g_write_state.w_cmd_power;
        //  When to signal power-off to power control to protect batteries
        power = true;
    }
    if (g_actual_state.w_cmd_steer != g_write_state.w_cmd_steer)
    {
        g_actual_state.w_cmd_steer = g_write_state.w_cmd_steer;
        steering = true;
    }
    if (g_actual_state.w_e_allow != g_write_state.w_e_allow)
    {
        g_actual_state.w_e_allow = g_write_state.w_e_allow;
        power = true;
    }
    if (g_actual_state.w_trim_power != g_write_state.w_trim_power)
    {
        g_actual_state.w_trim_power = g_write_state.w_trim_power;
        power = true;
        tuning = true;
    }
    if (g_actual_state.w_trim_steer != g_write_state.w_trim_steer)
    {
        g_actual_state.w_trim_steer = g_write_state.w_trim_steer;
        steering = true;
        tuning = true;
    }
    if (power)
    {
        update_motor_power();
    }
    if (steering)
    {
        actually_write_servo();
    }
    if (tuning)
    {
        schedule_write_tuning();
    }
}

void dispatch_cmd(unsigned char sz, unsigned char const *d)
{
    Cmd const &cmd = *(Cmd const *)d;
    for (unsigned char off = 0; off < cmd.reg_count; ++off) {
        unsigned char p = off + cmd.reg_start;
        if (p > sizeof(info_MotorPower)) {
            fatal(FATAL_UNEXPECTED);
        }
        ((unsigned char *)&g_write_state)[p] = d[2 + off];
    }
    apply_state();
}

/*
void reset_radio(void *)
{
    if (!g_actual_state.r_e_conn) {
        blink_leds(true);
        rf.teardown();
        delay(100);
        rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);
        blink_leds(false);
        after(8000, &reset_radio, 0);
    }
}

void poll_radio(void *)
{
    unsigned char n = rf.hasData();
    if (n > 0) {
        g_actual_state.r_e_conn = 20;
        char buf[32];
        rf.readData(n, buf);
        dispatch_cmd(n, (unsigned char const *)buf);
    }
    else if (g_actual_state.r_e_conn > 0) {
        --g_actual_state.r_e_conn;
        if (g_actual_state.r_e_conn == 0) {
            after(1, &reset_radio, 0);
        }
    }
    update_motor_power();
    after(50, &poll_radio, 0);
}


void slow_bits_update(void *v)
{
    g_actual_state.r_debug_bits = rf.readClearDebugBits();
    if (rf.canWriteData()) {
        rf.writeData(sizeof(g_actual_state), &g_actual_state);
    }
    after(175, &slow_bits_update, 0);
}
*/

void setup_power()
{
    adc_setup(AREF_INTERNAL);
}

void poll_power(void *);
unsigned char n_badVoltage = 0;
unsigned char n_lowVoltage = 0;

void poll_power_result(unsigned char value)
{
    //  8 bits -- just read high half
    unsigned short adcValue = value;
    g_actual_state.r_voltage = (adcValue << 6) / VOLTAGE_SCALER;
    if (g_actual_state.r_voltage < THRESHOLD_VOLTAGE) {
        //  stopping locally -- out of juice!
        ++n_lowVoltage;
        if (n_lowVoltage > NUM_LOW_VOLTAGE_TO_CARE) {
            g_actual_state.r_self_stop = true;
            uart_force_out('V');
        }
    }
    else {
        powerfail = false;
        n_lowVoltage = 0;
    }
    if (g_actual_state.r_voltage < OFF_VOLTAGE) { //   don't over-discharge
        powerfail = true;
        ++n_badVoltage;
        if (n_badVoltage > NUM_LOW_VOLTAGE_TO_CARE) {
            digitalWrite(POWEROFF_PIN, HIGH);
        }
    }
    else {
        n_badVoltage = 0;
    }
    update_motor_power();
    after(500, &poll_power, 0);
}

void poll_power(void *)
{
    if (adc_busy()) {
        after(1, &poll_power, 0);
        return;
    }
    adc_read(VOLTAGE_PIN, &poll_power_result);
}

class MySlave : public ITWISlave {
    public:
        bool volatile got_request;
        MySlave() : got_request(0) {}
        virtual void data_from_master(unsigned char n, void const *data) {
            got_request = true;
            //  packet format: start, count, <data>
            if (n != ((unsigned char const *)data)[1] + 2) {
                fatal(FATAL_TWI_UNEXPECTED);
            }
            dispatch_cmd(n, (unsigned char const *)data);
        }
        virtual void request_from_master(void *o_buf, unsigned char &o_size) {
            got_request = true;
            if (o_size > sizeof(g_actual_state)) {
                o_size = sizeof(g_actual_state);
            }
            memcpy(o_buf, &g_actual_state, o_size);
        }
};
MySlave twiSlave;

unsigned char no_request_count;


ISR(TIMER2_OVF_vect) {
    if (!twiSlave.got_request) {
        ++no_request_count;
        //  appx 8000 Hz, divided by 256 == 31, so this is 3 seconds without I2C
        if (no_request_count == 100) {
            fatal(FATAL_TWI_TIMEOUT);
        }
    }
    else {
        no_request_count = 0;
    }
    twiSlave.got_request = false;
    TIFR2 = TIFR2;
}

void start_i2c_timer_check(void *) {
    power_timer2_enable();
    ASSR = 0;
    TCCR2A = 0;
    TCCR2B = 7;  //  clock / 1024
    TIMSK2 = 1;  //  overflow interrupt
}

void setup()
{
    setup_timers();
    uart_setup(115200, F_CPU);
    read_tuning();
    setup_leds();
    setup_motors();
    setup_servo();
    setup_buttons();
    setup_power();
    digitalWrite(POWEROFF_PIN, LOW);
    pinMode(POWEROFF_PIN, OUTPUT);
    eeprom_read_block(&g_actual_state.r_last_fatal, (void *)EE_FATALCODE, 1);
    //delay(100); //  wait for radio to boot
    //on_pinchange(rf.getPinIRQ(), &rf_int);
    //rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);
    start_twi_slave(&twiSlave, NodeMotorPower);
    //  kick off the chain of tasks
    update_servo(0);
    g_actual_state.r_e_conn = g_write_state.r_e_conn = true;
    //poll_radio(0);
    //slow_bits_update(0);
    poll_button(0);
    poll_power(0);
    //reset_radio(0);
    after(1000, start_i2c_timer_check, 0);
}


