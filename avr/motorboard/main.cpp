
#include <libavr.h>
#include <nRF24L01.h>
#include <stdio.h>

#include "cmds.h"


/*  less than 6.5V in the battery pack, and I can't run. */
#define VOLTAGE_PIN 2
#define THRESHOLD_VOLTAGE 0x68
#define OFF_VOLTAGE 0x67    //  3.2V*2
#define VOLTAGE_SCALER 105  //  was 109

/* For some reason, running the servo on PWM is not very clean. */
/* Perhaps an approach that uses timer1 interrupts for high resolution */
/* would be better. But for now, I just schedule 50 Hz updates. */
#define USE_SERVO_TIMER 0

#define LED_GO_B (1 << PB0)
#define LED_PAUSE_D (1 << PD7)

#define POWEROFF_PIN (8|0)

nRF24L01<false, 0|7, 16|4, 0|6> rf;

class RfInt : public IPinChangeNotify {
    void pin_change(unsigned char) {
        rf.onIRQ();
    }
};
RfInt rf_int;

info_MotorPower g_write_state;
info_MotorPower g_actual_state;



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
        g_tuning.d_steer = 0;
        g_tuning.m_power = 128;
        write_tuning();
    }
    g_actual_state.w_trim_steer = g_write_state.w_trim_steer = g_tuning.d_steer;
    g_actual_state.w_trim_power = g_write_state.w_trim_power = g_tuning.m_power;
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
    char power = (char)g_actual_state.w_cmd_power;
    if (!g_actual_state.r_e_conn || !g_actual_state.w_e_allow || g_actual_state.r_self_stop) {
        power = 0;
    }
    if (!((power == 0 && g_actual_state.r_actual_power == 0) ||
                (power > 0 && g_actual_state.r_actual_power > 0) ||
                (power < 0 && g_actual_state.r_actual_power < 0))) {
        TCCR0A = 0x03;  //  Fast PWM, not yet turned on
        PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_A_NCH_D));
        PORTB = (PORTB & ~(MOTOR_B_PCH_B | MOTOR_B_NCH_B));
        udelay(10); // prevent shooth-through
    }
    g_actual_state.r_actual_power = power;
    if (power == 0) {
        //  ground everything out
        PORTD = (PORTD & ~(MOTOR_A_PCH_D)) | MOTOR_A_NCH_D;
        PORTB = (PORTB & ~(MOTOR_B_PCH_B)) | MOTOR_B_NCH_B;
        TCCR0A = 0x03;  //  Fast PWM, not yet turned on
        if (!g_actual_state.r_e_conn) {
            set_led_state(true, false, 1200);
        }
        else if (!g_actual_state.w_e_allow || g_actual_state.r_self_stop) {
            set_led_state(true, false, 200);
        }
        else {
            set_led_state(true, false, 0);
        }
    }
    else if (power < 0) {
        if (power < -127) {
            power = -127;
        }
        //  negative A, positive B
        set_led_state(false, true, 200);
        PORTB = (PORTB & ~(MOTOR_B_NCH_B)) | MOTOR_B_PCH_B;
        //PORTD = (PORTD & ~(MOTOR_A_PCH_D)) | MOTOR_A_NCH_D;
        //  Note: tuning 255 means "full power," 0 means "almost no power"
        OCR0A = (-(int)power * (g_actual_state.w_trim_power + 1)) >> 7;
        TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);  //  Fast PWM, channel A
    }
    else {
        if (power > 127) {
            power = 127;
        }
        //  positive A, negative B
        set_led_state(false, true, 0);
        PORTB = (PORTB & ~(MOTOR_B_PCH_B)) | MOTOR_B_NCH_B;
        //PORTD = (PORTD & ~(MOTOR_A_NCH_D)) | MOTOR_A_PCH_D;
        OCR0B = ((int)power * (g_actual_state.w_trim_power + 1)) >> 7;
        TCCR0A = (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);  //  Fast PWM, channel B
    }
}

unsigned char tuned_angle()
{
    int i = (int)(char)g_actual_state.w_cmd_steer + (char)g_actual_state.w_trim_steer + 90;
    if (i < 0) i = -0;
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
    set_servo_timer_angle(tuned_angle());
#endif
}

void actually_write_servo()
{
#if USE_SERVO_TIMER
    set_servo_timer_angle(tuned_angle());
#else
    IntDisable idi;
    PORTD |= (1 << PD3);
    udelay(tuned_angle() * 11U + 580U);
    PORTD &= ~(1 << PD3);
#endif
}

void update_servo(void *v)
{
#if USE_SERVO_TIMER
    //  this is very approximate
    actually_write_servo();
#else
    //  if killed from button, also kill servo
    if (!g_actual_state.r_self_stop)
    {
        actually_write_servo();
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
    //  If the button is held in, increment the stop counter
    if (!(PIND & (1 << PD2))) {
        g_actual_state.r_self_stop++;
        update_motor_power();
    }
    else if (g_actual_state.r_self_stop > 20) {
        //  start again by holding for 2 seconds, and letting go
        g_actual_state.r_self_stop = 0;
        update_motor_power();
    }
    else if (g_actual_state.r_self_stop > 0) {
        g_actual_state.r_self_stop = 1;
        update_motor_power();
    }
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

void reset_radio(void *)
{
    if (!g_actual_state.r_e_conn) {
        set_led_state(false, false, 0);
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
    else {
        set_led_state(false, true, 50);
    }
    after(175, &slow_bits_update, 0);
}

void setup_power()
{
    adc_setup();
}

void poll_power(void *);
unsigned char n_badVoltage = 0;

void poll_power_result(unsigned char value)
{
    //  8 bits -- just read high half
    unsigned short adcValue = value;
    g_actual_state.r_voltage = (adcValue << 6) / VOLTAGE_SCALER;
    if (g_actual_state.r_voltage < THRESHOLD_VOLTAGE) {
        //  stopping locally -- out of juice!
        g_actual_state.r_self_stop = true;
    }
    else {
        n_badVoltage = 0;
    }
    if (g_actual_state.r_voltage < OFF_VOLTAGE) { //   don't over-discharge
        ++n_badVoltage;
        if (n_badVoltage > 4) {
            digitalWrite(POWEROFF_PIN, HIGH);
        }
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
        virtual void data_from_master(unsigned char n, void const *data) {
            //  packet format: start, count, <data>
            if (n != ((unsigned char const *)data)[1] + 2) {
                fatal(FATAL_TWI_UNEXPECTED);
            }
            dispatch_cmd(n, (unsigned char const *)data);
        }
        virtual void request_from_master(void *o_buf, unsigned char &o_size) {
            if (o_size > sizeof(g_actual_state)) {
                o_size = sizeof(g_actual_state);
            }
            memcpy(o_buf, &g_actual_state, o_size);
        }
};
MySlave twiSlave;

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
    delay(100); //  wait for radio to boot
    on_pinchange(rf.getPinIRQ(), &rf_int);
    rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);
    start_twi_slave(&twiSlave, NodeMotorPower);
    //  kick off the chain of tasks
    update_servo(0);
    poll_radio(0);
    slow_bits_update(0);
    poll_button(0);
    poll_power(0);
    reset_radio(0);
}


