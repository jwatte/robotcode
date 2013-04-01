
#include "Config.h"
#include <LUFA/Drivers/USB/USB.h>
#include "OnyxWalker.h"
#include <avr/io.h>
#include <util/atomic.h>

#define EMPTY_IN_TIMEOUT 8

//  how many ms per "shot" fired
#define GUNS_TIMER 200

//  duty cycle of gun PWM
#define INIT_GUN_DUTY_CYCLE_3 0x58
#define INIT_GUN_DUTY_CYCLE_4 0x70

unsigned char GUN_DUTY_CYCLE_3 = INIT_GUN_DUTY_CYCLE_3;
unsigned char GUN_DUTY_CYCLE_4 = INIT_GUN_DUTY_CYCLE_4;

#define PWM_FIRE 1


//  DIP switch 4: do not display/animate battery level

#define CMD_RAW_MODE 0x01
//  param is baud rate
#define CMD_RAW_DATA 0x00
//  all data is data
#define CMD_SET_LEDS 0x11
//  data is byte of LED state
#define CMD_FIRE_GUNS 0x12
//  data is left, right fire time
#define CMD_SET_REG1 0x13
//  param is id, reg, data
#define CMD_SET_REG2 0x14
//  param is id, reg, datal, datah
#define CMD_GET_STATUS 0x20
//  no param -- just return status!
#define CMD_GET_REGS 0x23
//  param is id, reg, count
#define CMD_NOP 0xf0
//  no param
#define CMD_DELAY 0x31
//  param is milliseconds
#define CMD_LERP_POS 0xf3
//  params are actually more than header indicates
//  timel/h, nids, (id,posl/h)*nids
//  This sets the goal velocity field as well as the goal pos field.

#define READ_COMPLETE 0x41
#define STATUS_COMPLETE 0x51
#define DIPS_COMPLETE 0x61

#define DXL_PING 1
#define DXL_REG_READ 2
#define DXL_REG_WRITE 3
#define DXL_PEND_WRITE 4
#define DXL_PEND_COMMIT 5
#define DXL_RESET 6
#define DXL_MULTIWRITE 0x83

#define DXL_REG_GOAL_POSITION 0x1E
#define DXL_REG_MOVING_SPEED 0x20

#define BLINK_CNT 5
//  one tick is 5 microseconds; used in an uchar with timer, so 
//  if it's big, there's a very small window to actually detect 
//  the timeout
#define RECV_TIMEOUT_TICKS 50

#define DISPLAY_MS 30

void Reconfig(void);
void clear_poses(void);

int main(void) {
    SetupHardware();
    clear_poses();
    MCUSR |= (1 << WDRF);
    wdt_enable(WDTO_8S);

    sei();

    while (1) {
        wdt_reset();
        USB_USBTask();
        OnyxWalker_Task();
    }
}


static unsigned char display_state;
static unsigned char battery_level;
static unsigned char battery_voltage;
static unsigned char dropped_id;
static unsigned short display_time;
static bool display_blink;
static unsigned char guns_left = 0;
static unsigned char guns_right = 0;
static unsigned short guns_timer = 0;
static unsigned short guns_lasttime = 0;

static unsigned char servo_stati[32];
static unsigned short target_pose[32];
static unsigned short target_prev_pose[32];

unsigned short last_in = 0;
unsigned char last_seq = 0;


void clear_poses(void) {
    for (unsigned char id = 0; id != sizeof(target_pose)/sizeof(target_pose[0]); ++id) {
        target_pose[id] = 2048;
        target_prev_pose[id] = 2048;
    }
}

//  Broadcast to turn off torque for all servos on the bus.
static const unsigned char notorque_packet[] = {
    0xff, 0xff, 0xfe, 0x4, 0x3, 0x18, 0, (unsigned char)~(0xfe + 0x4 + 0x3 + 0x18 + 0),
};

static unsigned char xbuf[DATA_RX_EPSIZE];
unsigned char xbufptr = 0;

void clear_xbuf(void) {
    xbufptr = 0;
}

unsigned char last_cmd = 0;

void add_xbuf(unsigned char const *ptr, unsigned char len) {
    if (sizeof(xbuf) - xbufptr < len) {
        //  too high data rate
        show_error(7, xbufptr);
        return;
    }
    memcpy(&xbuf[xbufptr], ptr, len);
    xbufptr += len;
}


void setup_guns(void) {
    power_timer3_enable();
    PORTD &= ~(0x20 | 0x10);
    PORTC &= ~(0x80 | 0x40);
    DDRC |= (0x80 | 0x40);
    DDRD |= (0x20 | 0x10);
    //  timer 3, for PC6, which is gun motor left
    TCCR3A = (1 << WGM30);  //  8 bit PWM mode
    TCCR3B = (1 << WGM32) | (1 << CS32);    //  fast PWM, clock
    TCCR3C = 0;
    TCNT3H = 0;
    TCNT3L = 0;
    OCR3AH = 0;
    OCR3AL = GUN_DUTY_CYCLE_3;
    //  timer 4, for PC7, which is gun motor right
    TCCR4A = (1 << PWM4A);
    TCCR4B = (1 << CS42) | (1 << CS41) | (1 << CS40);
    TCCR4C = 0;
    TCCR4D = 0;
    TCCR4E = 0;
    TCNT4 = 0;
    OCR4A = GUN_DUTY_CYCLE_4;
    DT4 = 0;
}

void setup_dip(void) {
    DDRF &= ~((1 << PF5) | (1 << PF4) | (1 << PF1) | (1 << PF0));
    //  pull-up
    PORTF |= (1 << PF5) | (1 << PF4) | (1 << PF1) | (1 << PF0);
}

void setup_adc(void) {
    power_adc_enable();
    ADMUX = (1 << REFS0)    //  AVcc
        | 6                 //  channel 6 (PF6, where the battery is)
        ;
    ADCSRA = (1 << ADEN)    //  enable
        | (1 << ADIF)       //  clear complete flag
        | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0)    //  clock divide by 128
        ;
    ADCSRB = 0;
    DIDR0 = (1 << ADC6D)    //  disable digital buffer pin F6
        ;
    DIDR2 = 0;
    ADCSRA |= (1 << ADSC);  //  read once, to bootstrap the system
}


void SetupHardware(void) {
    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    //  settle down TxD/RxD
    PORTD |= ((1 << PD2) | (1 << PD3));
    DDRD &= ~((1 << PD2) | (1 << PD3));

    //  weapons fire off
    setup_guns();

    //  config
    setup_dip();
    
    //  Status LEDs on
    setup_status();
    set_status(0xff, 0xff);

    clock_prescale_set(clock_div_1);
    set_status(0x7f, 0xff);

    setup_delay();
    delayms(50);
    set_status(0x3f, 0xff);

    setup_uart(0);
    //  delayms built into setup_uart
    send_sync((unsigned char const *)"\0\0\0\0\0", 6, 0);
    //send_sync(notorque_packet, sizeof(notorque_packet), 1);    //  turn off torque on all servos
    clear_xbuf();
    set_status(0x1f, 0xff);

    delayms(50);
    set_status(0xf, 0xff);

    setup_adc();
    delayms(50);
    set_status(0x7, 0xff);

    delayms(50);
    set_status(0x3, 0xff);

    delayms(50);
    set_status(0x1, 0xff);
    delayms(50);
    set_status(0x0, 0xff);

    USB_Init();
}

void EVENT_USB_Device_Connect(void) {
    set_status(CONNECTED_LED, CONNECTED_LED);
}

void EVENT_USB_Device_Disconnect(void) {
    set_status(0x0, CONNECTED_LED);
}

void EVENT_USB_Device_ConfigurationChanged(void) {
    Reconfig();
}

void EVENT_USB_Device_Reset(void) {
    Reconfig();
}

void Reconfig() {
    bool ConfigSuccess = 1;

    ConfigSuccess &= Endpoint_ConfigureEndpoint(
        DATA_RX_EPNUM,
        EP_TYPE_BULK, 
        DATA_RX_EPSIZE,
        2);
    ConfigSuccess &= Endpoint_ConfigureEndpoint(
        DATA_TX_EPNUM,
        EP_TYPE_BULK, 
        DATA_TX_EPSIZE,
        1);

    if (!ConfigSuccess) {
        while (1) {
            set_status(0xff, 0xff);
            delayms(100);
            set_status(0, 0xff);
            delayms(100);
        }
    }
    clear_poses();
}

unsigned char pbuf[16];

unsigned char cksum(unsigned char const *ptr, unsigned char n) {
    unsigned char ck = 0;
    unsigned char const *end = ptr + n;
    while (end != ptr) {
        ck += *ptr;
        ++ptr;
    }
    return ~ck;
}

void reg_write(unsigned char id, unsigned char reg, unsigned char const *buf, unsigned char cnt, unsigned char lookaside) {
    if (cnt > 9) {
        show_error(5, cnt);
        cnt = 9;
    }
    if (lookaside) {
        if ((reg <= DXL_REG_GOAL_POSITION) && (reg+cnt >= DXL_REG_GOAL_POSITION + 2) &&
            (id < sizeof(target_pose)/sizeof(target_pose[0]))) {
            //  half updates of goal register are not look-aside snooped...
            unsigned char offs = DXL_REG_GOAL_POSITION - reg;
            target_pose[id] = ((buf[offs] + ((unsigned short)buf[offs + 1] << 8)) & 4095);
        }
    }
    pbuf[0] = 0xff;
    pbuf[1] = 0xff;
    pbuf[2] = id;
    pbuf[3] = cnt + 3;
    pbuf[4] = DXL_REG_WRITE;
    pbuf[5] = reg;
    memcpy(&pbuf[6], buf, cnt);
    pbuf[6 + cnt] = cksum(&pbuf[2], cnt + 4);
    send_sync(pbuf, cnt + 7, 1);
    //  assume servos do not ack writes
}

#if INTERRUPT_RECV

//  turns out, interrupts are too slow for 2 Mbit!
/*
unsigned char recv_packet(unsigned char *dst, unsigned char sz) {
    set_status(WAITING_LED, WAITING_LED);
    unsigned char cnt = 0;
    unsigned char timo = TCNT0;
    while (cnt < sz) {
        unsigned char ra = recv_avail();
        if (ra > cnt) {
            timo = TCNT0;
            cnt = ra;
        }
        else {
            unsigned char nn = TCNT0;
            if (nn < timo) {
                timo += 5;
            }
            if (nn - timo > RECV_TIMEOUT_TICKS) {
                if (cnt) {
                    recv_eat(cnt);
                }
                set_status(TIMEOUT_LED, WAITING_LED | TIMEOUT_LED);
                return 0;
            }
        }
    }
    memcpy(dst, recv_buf(), sz);
    recv_eat(sz);
    set_status(0, WAITING_LED | TIMEOUT_LED);
    return sz;
}
*/

#else 

unsigned char recv_packet(unsigned char *dst, unsigned char maxsz) {
    unsigned char cnt = 0;
    //  turn off receive interrupts
    UCSR1B = (1 << RXEN1);
    while (cnt < maxsz) {
        //  new timeout for each byte received
        unsigned char tc = TCNT0;
        while (!(UCSR1A & (1 << RXC1))) {
            //  don't spend more than X microseconds waiting for something that won't come
            unsigned char ntc = TCNT0;
            if ((unsigned char)(ntc - tc) > RECV_TIMEOUT_TICKS) {
                UCSR1B = (1 << RXEN1) | (1 << RXCIE1);
                set_status(TIMEOUT_LED, TIMEOUT_LED);
                return 0;
            }
        }
        dst[cnt] = UDR1;
        ++cnt;
    }
    UCSR1B = (1 << RXEN1) | (1 << RXCIE1);
    return cnt;
}

#endif


void reg_read(unsigned char id, unsigned char reg, unsigned char cnt) {
    if (cnt > 9) {
        show_error(6, cnt);
        cnt = 9;
    }
    pbuf[0] = 0xff;
    pbuf[1] = 0xff;
    pbuf[2] = id;
    pbuf[3] = 4;
    pbuf[4] = DXL_REG_READ;
    pbuf[5] = reg;
    pbuf[6] = cnt;
    pbuf[7] = cksum(&pbuf[2], 5);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        send_sync(pbuf, 8, 0);
        cnt = recv_packet(pbuf, 6 + cnt);
        if (cnt > 6) {
            if (id < sizeof(servo_stati)) {
                servo_stati[id] |= pbuf[4];
            }
            pbuf[1] = READ_COMPLETE;
            pbuf[2] = id;
            pbuf[3] = reg;
            pbuf[4] = cnt - 6;
            if (cnt > 6) {
                add_xbuf(&pbuf[1], cnt - 2);
            }
        }
        else {
            //  talk about incomplete read
            dropped_id = id;
            if (id < sizeof(servo_stati)) {
                servo_stati[id] |= 0x80;
            }
        }
    }
}


bool rawmode = 0;
unsigned char sbuf[DATA_TX_EPSIZE];
unsigned char sbuflen;

void set_rawmode(bool rm) {
    rawmode = rm;
    set_status(rm ? RAWMODE_LED : 0, RAWMODE_LED);
}

void wait_for_idle(void) {
    while (1) {
        unsigned char av = recv_avail();
        unsigned char ts = TCNT0;
        //  Wait for 40 us without any data -- that's 80 bits; plenty of time 
        //  for the bus to clear! Don't do this in delayus, as I still want 
        //  to keep interrupts enabled.
        while (1) {
            unsigned char ts2 = TCNT0;
            if (ts2 < ts) {
                ts += 5;
            }
            if ((unsigned char)(ts2 - ts) >= 10) {
                break;
            }
        }
        if (av == recv_avail()) {
            break;
        }
    }
}

unsigned char lerp_pos(unsigned char const *data, unsigned char sz) {
    if (sz < 3) {
        //  bad command
        return sz;
    }
    unsigned short dt = data[0] + ((unsigned short)data[1] << 8);
    if (dt < 5) {
        show_error(3, 3);
        //  don't allow lerping faster than at 200 Hz
        dt = 5;
    }
    unsigned char nid = data[2];
    if (nid > 32 || sz < nid * 3 + 3) {
        //  bad command
        return sz;
    }
    unsigned char ptr = 3;
    while (nid > 0) {
        unsigned char id = data[ptr];
        if (id >= sizeof(target_pose)/sizeof(target_pose[0])) {
            //  bad parameter
            show_error(3, 5);
        }
        else {
            unsigned short pos = data[ptr+1] + ((unsigned short)data[ptr+2] << 8);
            target_prev_pose[id] = target_pose[id];
            target_pose[id] = pos;
            //  calculate velocity
            short distance = target_prev_pose[id] - target_pose[id];
            if (distance < 0) {
                distance = -distance;
            }
            distance = distance << 4;   //  max range for short
            unsigned short ticks_per_ms_16 = distance / dt;
            //  The value is scaled by 16.
            //  A value of 1023 means 117 rpm, which we'll round to 120, which 
            //  in turn is 8192 ticks per second.
            //  That's about 8 ticks per millisecond.
            //  Multiply by 16, and you get 128 as the max value after the division.
            //  This means I need to scale by another 8.
            unsigned short spdval = ticks_per_ms_16 << 3;
            //  kick some off to avoid oscillation (introduces mush instead)
            spdval -= (spdval >> 3);    //  gets us 7/8th of the calculated value
            spdval = spdval > 1023 ? 1023 : spdval;
            unsigned char bf[4] = { pos & 0xff, (pos >> 8) & 0xff, spdval & 0xff, (spdval >> 8) & 0xff };
            //  write both goal position and speed, which is right after
            reg_write(id, DXL_REG_GOAL_POSITION, bf, 4, 0);
        }
        ptr += 3;
        --nid;
    }
    return ptr;
}

void do_get_status(void) {
    unsigned char cmd[6] = {
        STATUS_COMPLETE,
        0,
        get_nmissed(),
        battery_voltage,
        dropped_id,
        PINF & 0x33
        };
    cmd[1] = sizeof(cmd) - 2 + sizeof(servo_stati);
    dropped_id = 0;
    add_xbuf(cmd, sizeof(cmd));
    add_xbuf(servo_stati, sizeof(servo_stati));
    memset(servo_stati, 0, sizeof(servo_stati));
    set_status(0, TIMEOUT_LED);
}

void fire_guns(unsigned char left, unsigned char right) {
    OCR3AH = 0;
    OCR3AL = GUN_DUTY_CYCLE_3 - (battery_voltage >> 2);
    OCR4A = GUN_DUTY_CYCLE_4 - (battery_voltage >> 2);
    guns_left = left > guns_left ? left : guns_left;
    guns_right = right > guns_right ? right : guns_right;
    guns_timer = GUNS_TIMER;
    guns_lasttime = getms();
    set_status((left ? 0x20 : 0) | (right ? 0x10 : 0), 0x30);
}

void set_guns(void) {
    unsigned char dreg = 0;
    unsigned char creg = 0;
    if (guns_left) {
        dreg |= (1 << 4);
        creg |= (1 << 6);
#if PWM_FIRE
        TCCR3A = (1 << WGM30) | (1 << COM3A1);
    }
    else {
        TCCR3A = (1 << WGM30);
#endif
    }
    if (guns_right) {
        dreg |= (1 << 5);
        creg |= (1 << 7);
#if PWM_FIRE
        if (!(TCCR4A & (1 << COM4A1))) {
            TCCR4A |= (1 << COM4A1);
        }
    }
    else {
        TCCR4A &= ~(1 << COM4A1);
#endif
    }
    PORTD = (PORTD & ~((1 << 4) | (1 << 5))) | dreg;
    PORTC = (PORTC & ~((1 << 6) | (1 << 7))) | creg;
}

void run_guns(void) {
    if (guns_timer) {
        unsigned char d = getms() - guns_lasttime;
        if (d >= guns_timer) {
            guns_timer = 0;
            if (guns_left) {
                --guns_left;
                guns_timer = GUNS_TIMER;
            }
            if (guns_right) {
                --guns_right;
                guns_timer = GUNS_TIMER;
            }
        }
        else {
            guns_timer -= d;
        }
        set_guns();
    }
}

void dispatch(unsigned char const *sbuf, unsigned char offset, unsigned char end) {
    while (offset < end) {
        bool clear_rawmode = 1;
        if (rawmode && (sbuf[offset] == CMD_RAW_DATA)) {
            wait_for_idle();
            send_sync(&sbuf[offset+1], end - offset - 1, 1);
            break;
        }
        unsigned char cmdSize = (sbuf[offset] & 0x7) + 1;
        if ((end - offset) < cmdSize) {
            //  missing data!
            show_error(1, cmdSize);
            break;
        }
        switch (sbuf[offset]) {
            case CMD_RAW_MODE:
                set_rawmode(1);
                clear_rawmode = 0;
                setup_uart(sbuf[offset+1]);
                break;
            case CMD_SET_LEDS:
                set_status(sbuf[offset+1], 0xff);
                break;
            case CMD_FIRE_GUNS:
                fire_guns(sbuf[offset+1], sbuf[offset+2]);
                break;
            case CMD_SET_REG1:
                reg_write(sbuf[offset+1], sbuf[offset+2], &sbuf[offset+3], 1, 1);
                break;
            case CMD_SET_REG2:
                reg_write(sbuf[offset+1], sbuf[offset+2], &sbuf[offset+3], 2, 1);
                break;
            case CMD_GET_REGS:
                reg_read(sbuf[offset+1], sbuf[offset+2], sbuf[offset+3]);
                break;
            case CMD_GET_STATUS:
                do_get_status();
                break;
            case CMD_DELAY:
                delayms(sbuf[offset+1]);
                wait_for_idle();
                break;
            case CMD_NOP:
                break;
            case CMD_LERP_POS:
                cmdSize = lerp_pos(sbuf+offset+1, end-offset-1) + 1;
                break;
            default:
                //  unknown command
                show_error(2, sbuf[offset]);
                return;
        }
        if (cmdSize == 0) {
            show_error(4, sbuf[offset]);
        }
        offset += cmdSize;
        if (clear_rawmode && rawmode) {
            set_rawmode(0);
        }
    }
}

unsigned char epic;
unsigned char epiir;
unsigned char epirwa;

unsigned short clear_received;

void OnyxWalker_Task(void) {

    if (USB_DeviceState != DEVICE_STATE_Configured) {
        return;
    }

    /* see if there's data to send to host */
    unsigned short now = getms();
    Endpoint_SelectEndpoint(DATA_RX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_IN);
    epic = Endpoint_IsConfigured();
    epiir = epic && Endpoint_IsINReady();
    epirwa = epiir && Endpoint_IsReadWriteAllowed();
    if (epirwa) {
        unsigned char gg = xbufptr;
        unsigned char m = recv_avail();
        if (gg || m || (now - last_in > EMPTY_IN_TIMEOUT)) {
            set_status(POLLING_LED, POLLING_LED);
            clear_received = now + BLINK_CNT;
            last_in = now;
            Endpoint_Write_8(last_seq);
            if (gg) {
                for (unsigned char q = 0; q < gg; ++q) {
                    Endpoint_Write_8(xbuf[q]);
                }
                clear_xbuf();
            }
            ++gg;   //  for the seq
            if (m) {
                unsigned char nm = m;
                if (rawmode) {
                    if ((unsigned short)m + gg > DATA_RX_EPSIZE) {
                        m = DATA_RX_EPSIZE - gg;
                    }
                    nm = m;
                    unsigned char const *ptr = recv_buf();
                    while (m > 0) {
                        Endpoint_Write_8(*ptr);
                        ++ptr;
                        --m;
                    }
                }
                recv_eat(nm);
            }
            Endpoint_ClearIN();
        }
    }

    /* see if there's data from the host */
    Endpoint_SelectEndpoint(DATA_TX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_OUT);
    if (Endpoint_IsConfigured() && Endpoint_IsOUTReceived() && Endpoint_IsReadWriteAllowed()) {
        set_status(RECEIVED_LED, RECEIVED_LED);
        clear_received = now + BLINK_CNT;
        uint8_t n = Endpoint_BytesInEndpoint();
        if (n > sizeof(sbuf)) {
            n = sizeof(sbuf);
        }
        if (n) {
            last_seq = Endpoint_Read_8();
            --n;
            for (unsigned char c = 0; c < n; ++c) {
                sbuf[c] = Endpoint_Read_8();
            }
        }
        Endpoint_ClearOUT();
        if (n) {
            sbuflen = n;
            dispatch(sbuf, 0, n);
        }
    }

    /* remove received status */
    if ((short)(now - clear_received) > 0) {
        set_status(0, RECEIVED_LED | SENDING_LED | POLLING_LED);
        clear_received = now;
    }

    run_guns();

    /* display LED status (voltage animation etc) */
    if ((short)(now - display_time) >= DISPLAY_MS) {
        ++display_state;
        display_time = now;
        display_blink = !display_blink;

        if (ADCSRA & (1 << ADIF)) {
            ADCSRA |= 1 << ADIF;
            unsigned short aval = (unsigned short)ADCL | ((unsigned short)ADCH << 8u);
            //  102 is adjusted for high AREF (measured 5.1V)
            battery_voltage = (unsigned char)((long)aval * 102 / 508);
            //  LiPo batteries are very nonlinear in voltage -- there is a large 
            //  capacity range where they hover around the 14.5-15.0 volt range.
            static struct {
                unsigned char bits;
                unsigned char voltage;
            } voltages[] = {  //  853 is 16.8 volts; these numbers assume some load
                { 0xff, 160 },
                { 0xfe, 155 },
                { 0xfc, 151 },
                { 0xf8, 148 },
                { 0xf0, 145 },
                { 0xe0, 142 },
                { 0xc0, 138 },
                { 0x80, 128 },
                { 0, 0u },       //  terminator
            };
            int i = 0;
            do {
                battery_level = voltages[i].bits;
            } while (voltages[i++].voltage > battery_voltage);
            if (battery_voltage < 132) {  //  about to go bust -- turn off!
                send_sync(notorque_packet, sizeof(notorque_packet), 1);    //  turn off torque on all servos
                display_state = 128;    //  force battery display
                battery_level = 0x80;
                //  todo: robot controller should kill power at this point
            }
            ADCSRA |= (1 << ADSC);  //  start another one
        }

        /* switch 4 can turn off battery display, but not the voltage checking part */
        if (PINF & (1 << PF5)) {
            if (display_state < 112) {
                set_status_override(0, 0);
            }
            else if (display_state < 120) {
                set_status_override(1 << (display_state - 112), 0xff);
            }
            else if (display_state < 128) {
                set_status_override((0x80 >> (display_state - 120)) |
                    (battery_level & ~(0xff >> (display_state - 120))), 0xff);
            }
            else if (display_state < 180) {
                unsigned char dl = battery_level;
                if (battery_level == 0x80 && display_blink) {
                    dl = 0;
                }
                set_status_override(dl, 0xff);
            }
            else if (display_state < 188) {
                set_status_override((battery_level & (0xff >> (display_state - 180)))
                    | (0x80 >> (display_state - 180)), 0xff);
            }
            else {
                set_status_override(0, 0);
            }
        }
        else {
            set_status_override(0, 0);
        }
    }
}

