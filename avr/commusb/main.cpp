
#define F_CPU 16000000UL

#include "libavr.h"
#include "pins_avr.h"
#include "cmds.h"
#include "L3G4200D.h"
#include <stdio.h>

#define LED_PIN (0|5)
#define RESET_SENSOR (1 << 2)
#define RESET_MOTOR (1 << 3)
#define SERIAL_INDICATOR (1 << 4)
#define CMD_INDICATOR (1 << 5)

#define BAUD_RATE 115200

/* UART protocol:

    0xed <len> <cmd> <data>
    <len> excludes the ed byte and the len byte itself.
    The maximum value of <len> is 30

    Board -> Host
    cmd arg1 arg2 data
    O                           board is online
    D   node      data          data from node
    F   node code               node fatalled

    Host -> Board
    m   node      data          send message to node (typically offset-size-data config write)
    c   node      data          send command to node (free-form data)
    r                           reset node (sensor and motor only)

 */

#define SYNC_BYTE ((unsigned char)0xed)

info_USBInterface g_info;
TWIMaster *twi;

#define MAXCMD 30
static unsigned char cmdbuf[2 + MAXCMD];
static unsigned char inptr;
static unsigned char inbuf[2 + MAXCMD];

static void cmd_start(char ch) {
    PORTD |= SERIAL_INDICATOR;
    cmdbuf[0] = 0xed;
    cmdbuf[1] = 1;
    cmdbuf[2] = ch;
}
static void cmd_add(uint8_t len, void const *data) {
    if (cmdbuf[0] != 0xed) {
        fatal(FATAL_BAD_SERIAL);
    }
    if (cmdbuf[1] + len > MAXCMD) {
        fatal(FATAL_TOO_BIG_SERIAL);
    }
    memcpy(&cmdbuf[2+cmdbuf[1]], data, len);
    cmdbuf[1] += len;
}
static void cmd_finish() {
    uart_send_all(cmdbuf[1] + 2, cmdbuf);
    cmdbuf[0] = 0;
    PORTD &= ~SERIAL_INDICATOR;
}

void send_data_from(unsigned char from, unsigned char n, void const *data) {
    if (n > TWI_MAX_SIZE) {
        fatal(FATAL_TOO_BIG_SERIAL);
    }
    cmd_start('D');
    cmd_add(1, &from);
    cmd_add(n, data);
    cmd_finish();
}


class ITWIUser : public ITWIMaster {
public:
    static ITWIUser *first_;
    ITWIUser *next_;

    void (ITWIUser ::*cb_)();
    unsigned char from_;

    virtual void tick() = 0;

    void getfrom(unsigned char node, unsigned char cnt,
                void (ITWIUser::*cb)() = 0);
    void sendto(unsigned char cnt, void const *d, unsigned char node,
                void (ITWIUser::*cb)() = 0);
    virtual void data_from_slave(unsigned char n, void const *data);
    virtual void xmit_complete();
    virtual void nack();
    virtual void complete();
    virtual void enqueue();
    static void next(void *);
};
ITWIUser *ITWIUser::first_;

//  The XSendUser doesn't wait for its scheduled turn, but instead
//  immediately grabs the bus as soon as it becomes available and 
//  sends when it has data. It's a helper used by the master for 
//  ad-hoc requests.
class XSendUser : public ITWIUser {
public:
    void tick() {}
    void enqueue() {}
};
XSendUser xUser;

class MyMaster : public ITWIMaster {
public:
        bool try_send_to(unsigned char node, unsigned char sz, unsigned char const *data) {
            if (user_ || twi->is_busy()) {
                return false;
            }
            user_ = &xUser;
            twi->send_to(sz, data, node);
            return true;
        }
        virtual void data_from_slave(unsigned char n, void const *data) {
            if (!user_) {
                fatal(FATAL_UNEXPECTED);
            }
            user_->data_from_slave(n, data);
        }
        virtual void xmit_complete() {
            if (!user_) {
                fatal(FATAL_UNEXPECTED);
            }
            user_->xmit_complete();
        }
        virtual void nack() {
            if (!user_) {
                fatal(FATAL_UNEXPECTED);
            }
            user_->nack();
        }
        ITWIUser *user_;
};
MyMaster master;

void ITWIUser::getfrom(unsigned char node, unsigned char cnt,
            void (ITWIUser::*cb)()) {
    from_ = node;
    cb_ = cb;
    twi->request_from(node, cnt);
}

void ITWIUser::sendto(unsigned char cnt, void const *d, unsigned char node,
            void (ITWIUser::*cb)()) {
    from_ = node;
    cb_ = cb;
    twi->send_to(cnt, d, node);
}

void ITWIUser::data_from_slave(unsigned char n, void const *data) {
    if (cb_ && master.user_) {
        (master.user_->*cb_)();
    }
    else if (from_) {
        send_data_from(from_, n, data);
    }
    complete();
}

void ITWIUser::xmit_complete() {
    if (master.user_ && cb_) {
        (master.user_->*cb_)();
    }
    else {
        complete();
    }
}

void ITWIUser::nack() {
    //  just do nothing
    complete();
}

void ITWIUser::complete() {
    master.user_ = 0;
    enqueue();
}

void ITWIUser::enqueue() {
    ITWIUser **pp = &first_;
    while (*pp) {
        if (*pp == this) {
            fatal(FATAL_DOUBLE_ENQUEUE);
        }
        pp = &(*pp)->next_;
    }
    next_ = 0;
    *pp = this;
}

static unsigned short last_tick;
static unsigned char no_i2c;


void ITWIUser::next(void *) {
    unsigned char at = 5;
    if (master.user_ || twi->is_busy()) {
        at = 1;
    }
    else if (first_ != 0) {
        last_tick = read_timer_fast();
        no_i2c = 0;
        ITWIUser *it = first_;
        first_ = it->next_;
        master.user_ = it;
        it->tick();
    }
    after(at, &ITWIUser::next, 0);
}



class MotorBoardUser : public ITWIUser {
public:
    void tick() {
        getfrom(NodeMotorPower, sizeof(info_MotorPower));
    }
};
MotorBoardUser motorBoard;

class SensorBoardUser : public ITWIUser {
public:
    void tick() {
        getfrom(NodeSensorInput, sizeof(info_SensorInput));
    }
};
SensorBoardUser sensorBoard;


//  I2C addresses
#define ACCEL           0x18
#define MAG             0x1E
#define GYRO            0x69
//  MAG registers
#define CRA_REG_M       0x00
#define MR_REG_M        0x02
#define OUT_X_H_M       0x03
//  ACCEL registers
#define CTRL_REG1_A     0x20
#define CTRL_REG4_A     0x23
#define OUT_X_L_A       0x28

#define REG_IS_MAG(x) ((x) < 0x20)

static void bigend(unsigned short *dst, void const *src) {
    unsigned char const *s = (unsigned char const *)src;
    dst[0] = (s[0] << 8) | s[1];
    dst[1] = (s[2] << 8) | s[3];
    dst[2] = (s[4] << 8) | s[5];
}

static void litend(unsigned short *dst, void const *src) {
    unsigned char const *s = (unsigned char const *)src;
    dst[0] = (s[1] << 8) | s[0];
    dst[1] = (s[3] << 8) | s[2];
    dst[2] = (s[5] << 8) | s[4];
}


class IMUUser : public ITWIUser {
public:
    enum State {
        Uninited0 = 0x00,
        Uninited1,
        Uninited2,
        Uninited3,
        Uninited4,
        Inited = 0x20,  //  a k a AddrMag
        RequestMag,
        ReplyMag,
        AddrAccel = ReplyMag,
        RequestAccel,
        ReplyAccel,
        AddrGyro = ReplyAccel,
        RequestGyro,
        ReplyGyro
    };

    IMUUser() :
        state_(Uninited0)
    {
        memset(&imu_, 0, sizeof(imu_));
    }
    unsigned char state_;
    unsigned char nextState_;
    info_IMU imu_;

    virtual void tick() {
        next();
    }
    virtual void data_from_slave(unsigned char n, void const *data) {
        switch (state_) {
            case ReplyMag:
                bigend(imu_.r_mag, data);
                break;
            case ReplyAccel:
                litend(imu_.r_accel, data);
                break;
            case ReplyGyro:
                litend(imu_.r_gyro, data);
                break;
            default:
                //  how did I end up here?
                {
                    IntDisable idi;
                    uart_force_out(0xed);
                    uart_force_out('s');
                    uart_force_out(state_);
                    uart_force_out(n);
                    fatal(FATAL_UNEXPECTED);
                }
        }
        next();
    }
    void next() {
        if (state_ == 5) {
            state_ = Inited;
        }
        nextState_ = state_ + 1;
        switch (state_) {
        case Uninited0:
            regwr(CTRL_REG1_A, 0x27, ACCEL);   //  full power on
            break;
        case Uninited1:
            regwr(CTRL_REG4_A, 0x00, ACCEL);   //  no limitation of range
            break;
        case Uninited2:
            regwr(CRA_REG_M, 0x14, MAG);     //  full output rate
            break;
        case Uninited3:
            regwr(MR_REG_M, 0x00, MAG);      //  keep running!
            break;
        case Uninited4:
            regwr(L3G4200D_CTRL_REG1, 0x0F, GYRO);    //  full power
            break;
        //case 5:
        //    nextState_ = Inited;
        //    //  aaand the transition is dropped on the floor...
        //    break;
        case Inited:
            regrd(OUT_X_H_M, MAG);
            break;
        case RequestMag:
            getfrom(MAG, 6);
            break;
        case AddrAccel:
            regrd(OUT_X_L_A | 0x80, ACCEL);
            break;
        case RequestAccel:
            getfrom(ACCEL, 6);
            break;
        case AddrGyro:
            regrd(L3G4200D_OUT_X_L | 0x80, GYRO);
            break;
        case RequestGyro:
            getfrom(GYRO, 6);
            break;
        default:
            //  I'm complete with one cycle!
            send_data();
            complete();
            nextState_ = Inited;
            break;
        }
        state_ = nextState_;
    }

    void send_data() {
        send_data_from(NodeIMU, sizeof(imu_), &imu_);
    }

    void regwr(unsigned char reg, unsigned char val, unsigned char addr) {
        unsigned char data[2] = { reg, val };
        sendto(2, data, addr, &ITWIUser::tick);
    }
    void regrd(unsigned char reg, unsigned char addr) {
        sendto(1, &reg, addr, &ITWIUser::tick);
    }
};
IMUUser imu;


void blink(bool on) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, on ? HIGH : LOW);
}

bool dispatch_uart_cmd(unsigned char cmd, unsigned char sz, unsigned char const *data) {
    switch (cmd) {
    case 'c':
    case 'm':
        if (sz > 1) {
            return master.try_send_to(data[0], sz-1, &data[1]);
        }
        else {
            //  no data after node address?
        }
        break;
    case 'r':
        DDRD |= (RESET_MOTOR | RESET_SENSOR);
        PORTD &= ~(RESET_MOTOR | RESET_SENSOR);
        udelay(50);
        PORTD |= (RESET_MOTOR | RESET_SENSOR);
        DDRD &= ~(RESET_MOTOR | RESET_SENSOR);
        break;
    default:
        //  bad command -- fatal?
        break;
    }
    return true;
}

void uart_poll(void *) {
    after(0, &uart_poll, 0);
    if ((read_timer_fast() - last_tick) > 1000) {
        ++no_i2c;
        if (no_i2c > 4) {
            fatal(FATAL_TWI_TIMEOUT);
        }
        last_tick = read_timer_fast();
    }
    if (inptr == 0) {
        unsigned char there = uart_available();
        while (there > 0) {
            unsigned char rd = uart_getch();
            there -= 1;
            if (rd == 0xed) {
                inbuf[0] = rd;
                inptr = 1;
                goto got_packet;
            }
        }
        return;
    }
got_packet:
    if (inptr < sizeof(inbuf)) {
        inptr += uart_read(sizeof(inbuf)-inptr, &inbuf[inptr]);
    }
    if (inbuf[0] != 0xed) {
        fatal(FATAL_BAD_SERIAL);
    }
    //  format is always 0xed-len-cmd-data
    if (inptr > 1 && inptr >= 2 + inbuf[1]) {
        // got full cmd
        PORTD = PORTD | CMD_INDICATOR;
        if (inptr > 2) {
            //  not a null cmd
            if (!dispatch_uart_cmd(inbuf[2], inbuf[1] - 1, &inbuf[3])) {
                //  re-try dispatching later -- note, cmd indicator will stay on
                return;
            }
        }
        PORTD &= ~CMD_INDICATOR;
        if (inptr > 2 + inbuf[1]) {
            unsigned char off = 2 + inbuf[1];
            while (off < inptr) {
                if (inbuf[off] == 0xed) {
                    memmove(inbuf, &inbuf[off], inptr - off);
                    inptr -= off;
                    goto got_packet;
                }
                ++off;
            }
            //  eat junk bytes
        }
        inptr = 0;
    }
    else if (inptr >= 2 && inbuf[1] > sizeof(inbuf)-2) {
        //  a bad command length -- flush
        PORTD = PORTD | CMD_INDICATOR;
        PORTD &= ~CMD_INDICATOR;
        inptr = 0;
    }
}


void read_voltage(void *);

void on_voltage(unsigned char v) {
    g_info.r_voltage = v;
    send_data_from(NodeUSBInterface, sizeof(g_info), &g_info);
    after(1000, &read_voltage, 0);
}

void read_voltage(void *) {
    adc_read(0, &on_voltage);
}

void setup() {
    //  reset other boards by pulling reset low
    PORTD &= ~(SERIAL_INDICATOR | RESET_SENSOR | RESET_MOTOR | CMD_INDICATOR);
    DDRD |= SERIAL_INDICATOR | RESET_SENSOR | RESET_MOTOR | CMD_INDICATOR;
    fatal_set_blink(&blink);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    setup_timers(F_CPU);
    uart_setup(BAUD_RATE, F_CPU);
    delay(1);
    adc_setup();
    //  turn reset into an input, to be voltage independent
    PORTD |= RESET_SENSOR | RESET_MOTOR;
    DDRD &= ~(RESET_SENSOR | RESET_MOTOR);
    twi = start_twi_master(&master);
    cmd_start('O');
    cmd_finish();
    motorBoard.enqueue();
    sensorBoard.enqueue();
    imu.enqueue();
    delay(20);
    after(1, &ITWIUser::next, 0);
    after(1000, &read_voltage, 0);
    digitalWrite(LED_PIN, LOW);
    uart_poll(0);
}

