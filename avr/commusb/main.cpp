
#define F_CPU 16000000UL

#include "libavr.h"
#include "pins_avr.h"
#include "cmds.h"
#include "L3G4200D.h"
#include <stdio.h>

#define LED_PIN (0|5)

/* UART protocol:

   Each cmd prefixed by sync byte value 0xed
   Usbboard->Host
   O                      On, running
   F <code>               Fatal death, will reboot in 8 seconds
   D <node> <len> <data>  Data polled from node
   N <node>               Nak from node when polling
   R <sensor> <distance>  Distance reading from ranging sensor
   X <len> <text>         Debug text

   x                      Command denied
   o                      Command accepted
   e                      Command error

   Host->Usbboard
   W <node> <len> <data>  Write data to given node
 */

#define SYNC_BYTE ((unsigned char)0xed)
#define CMD_SENDING 0xff

info_USBInterface g_info;
TWIMaster *twi;


void data_out(unsigned char n, void const *s) {
    PORTD |= (1 << 4);
    uart_send_all(n, s);
    PORTD &= ~(1 << 4);
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
    void complete();
    void enqueue();
    static void next(void *);
};
ITWIUser *ITWIUser::first_;


class MyMaster : public ITWIMaster {
public:
        virtual void data_from_slave(unsigned char n, void const *data)
        {
            if (!user_) {
                fatal(FATAL_UNEXPECTED);
            }
            user_->data_from_slave(n, data);
        }
        virtual void xmit_complete()
        {
            if (!user_) {
                fatal(FATAL_UNEXPECTED);
            }
            user_->xmit_complete();
        }
        virtual void nack()
        {
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
        unsigned char d[TWI_MAX_SIZE + 4];
        d[0] = SYNC_BYTE;
        d[1] = 'D';
        d[2] = from_;
        d[3] = n;
        memcpy(&d[4], data, n);
        data_out(4 + n, d);
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

void ITWIUser::next(void *) {
    unsigned char at = 10;
    if (master.user_ || twi->is_busy()) {
        at = 1;
    }
    else if (first_ != 0) {
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
        getfrom(NodeSensorInput, sizeof(info_MotorPower));
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
                uart_force_out(0xed);
                uart_force_out('s');
                uart_force_out(state_);
                uart_force_out(n);
                fatal(FATAL_UNEXPECTED);
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
        unsigned char d[sizeof(imu_) + 4];
        d[0] = SYNC_BYTE;
        d[1] = 'D';
        d[2] = NodeIMU;
        d[3] = sizeof(imu_);
        memcpy(&d[4], &imu_, sizeof(imu_));
        data_out(4 + sizeof(imu_), d);
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


void setup() {
    DDRD |= (1 << 4);
    PORTD &= ~(1 << 4);
    fatal_set_blink(&blink);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    setup_timers(F_CPU);
    uart_setup(115200, F_CPU);
    adc_setup(false);
    twi = start_twi_master(&master);
    data_out(2, "\xedO");
    motorBoard.enqueue();
    sensorBoard.enqueue();
    imu.enqueue();
    delay(20);
    after(1, &ITWIUser::next, 0);
    digitalWrite(LED_PIN, LOW);
}

