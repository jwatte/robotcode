
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

#define SYNC_BYTE ((char)0xed)
#define CMD_SENDING 0xff

info_USBInterface g_info;


void blink(bool on) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void debug_text(char const *txt)
{
    int l = strlen(txt);
    if (l > 32) {
        l = 32;
    }
    char buf[3] = { SYNC_BYTE, 'X', (char)(l & 0xff) };
    uart_send_all(3, buf);
    uart_send_all(l, txt);
}

unsigned char board_size(unsigned char type) {
    switch (type) {
        case NodeMotorPower: return sizeof(info_MotorPower);
        case NodeSensorInput: return sizeof(info_SensorInput);
        default: return 12;
    }
}

void request_from_boards(void *);

class IRequest {
public:
    virtual unsigned char id() = 0;
    virtual void on_tick() = 0;
    virtual void on_data(unsigned char n, void const *data) = 0;
    virtual void on_xmit() = 0;
};

TWIMaster *twi;
class MyMaster : public ITWIMaster {
    public:
        unsigned char requestFrom;
        unsigned char requestState;

        MyMaster() :
            requestFrom(0),
            requestState(0) {
        }

        virtual void data_from_slave(unsigned char n, void const *data) {
            boards[requestState]->on_data(n, data);
        }
        virtual void xmit_complete() {
            if (requestFrom != CMD_SENDING) {
                boards[requestState]->on_xmit();
            }
            else {
                requestFrom = 0;
            }
        }
        virtual void nack() {
            unsigned char ch[3] = { (unsigned char)SYNC_BYTE, 'N', requestFrom };
            uart_send_all(3, ch);
            requestFrom = 0;
            after(50, request_from_boards, 0);
        }
        void got_board_data(unsigned char n, void const *data, unsigned char boardId) {
            unsigned char ch[4] = { (unsigned char)SYNC_BYTE, 'D', boardId, n };
            uart_send_all(4, ch);
            if (n > 0) {
                uart_send_all(n, data);
            }
            requestFrom = 0;
            after(50, request_from_boards, 0);
        }
        void tick() {
            if (requestFrom || twi->is_busy()) {
                after(1, request_from_boards, 0);
                return;
            }
            request_next();
        }
        void request_next();
        static IRequest *boards[];
};
MyMaster twiMaster;

class DefaultRequest : public IRequest {
public:
    DefaultRequest(unsigned char i) :
        id_(i) {
    }
    unsigned char id_;
    virtual unsigned char id() {
        return id_;
    }
    virtual void on_tick() {
        twi->request_from(id_, board_size(id_));
    }
    virtual void on_data(unsigned char n, void const *data) {
        twiMaster.got_board_data(n, data, id_);
    }
    virtual void on_xmit() {}
};
DefaultRequest motorRequest(NodeMotorPower);
DefaultRequest sensorRequest(NodeSensorInput);

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

class IMURequest : public IRequest {
public:
    enum State {
        Uninited = 0x00,
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
    IMURequest() :
        state_(Uninited) {
        memset(&imu_, 0, sizeof(imu_));
    }

    unsigned char state_;
    unsigned char nextState_;
    info_IMU imu_;

    virtual unsigned char id() {
        return NodeIMU;
    }
    virtual void on_tick() {
        next();
    }
    virtual void on_data(unsigned char n, void const *data) {
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
    virtual void on_xmit() {
        next();
    }
    void next() {
        if (state_ == 5) {
            state_ = Inited;
        }
        nextState_ = state_ + 1;
        switch (state_) {
        case 0:
            regwr(CTRL_REG1_A, 0x27, ACCEL);   //  full power on
            break;
        case 1:
            regwr(CTRL_REG4_A, 0x00, ACCEL);   //  no limitation of range
            break;
        case 2:
            regwr(CRA_REG_M, 0x14, MAG);     //  full output rate
            break;
        case 3:
            regwr(MR_REG_M, 0x00, MAG);      //  keep running!
            break;
        case 4:
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
            twi->request_from(MAG, 6);
            break;
        case AddrAccel:
            regrd(OUT_X_L_A | 0x80, ACCEL);
            break;
        case RequestAccel:
            twi->request_from(ACCEL, 6);
            break;
        case AddrGyro:
            regrd(L3G4200D_OUT_X_L | 0x80, GYRO);
            break;
        case RequestGyro:
            twi->request_from(GYRO, 6);
            break;
        default:
            twiMaster.got_board_data(sizeof(imu_), &imu_, NodeIMU);
            //  I'm complete with one cycle!
            nextState_ = Inited;
            break;
        }
        state_ = nextState_;
    }

    void regwr(unsigned char reg, unsigned char val, unsigned char addr) {
        unsigned char data[2] = { reg, val };
        twi->send_to(2, data, addr);
    }
    void regrd(unsigned char reg, unsigned char addr) {
        twi->send_to(1, &reg, addr);
    }
};
IMURequest imuRequest;

IRequest *MyMaster::boards[] = {
    &motorRequest,
    &sensorRequest,
    &imuRequest
};

void MyMaster::request_next() {
    ++requestState;
    if (requestState >= sizeof(boards)/sizeof(boards[0])) {
        requestState = 0;
    }
    requestFrom = boards[requestState]->id();
    boards[requestState]->on_tick();
}


void request_from_boards(void *)
{
    twiMaster.tick();
}

void request_from_usbboard(void *)
{
    char data[4] = { SYNC_BYTE, 'D', NodeUSBInterface, (char)sizeof(g_info) };
    uart_send_all(4, data);
    uart_send_all(sizeof(g_info), &g_info);
    after(1000, request_from_usbboard, 0);
}


unsigned char map_voltage(unsigned char val)
{
    if (val == 255) {
        return 255;
    }
    unsigned short us = (unsigned short)val * 37 / 40;
    return (unsigned char)us;
}

void voltage_cb(unsigned char val)
{
    g_info.r_voltage = map_voltage(val);
}

void poll_voltage(void *)
{
    if (!adc_busy()) {
        adc_read(0, &voltage_cb);
    }
    after(500, &poll_voltage, 0);
}

bool twiSending = false;
char ogtwi[TWI_MAX_SIZE + 2];
void send_ogtwi(void *)
{
    if (twiMaster.requestFrom) {
        after(1, send_ogtwi, 0);
        return;
    }
    if (twi->is_busy()) {
        after(0, send_ogtwi, 0);
        return;
    }
    uart_send_all(2, "\xedo");
    twiSending = false;
    twiMaster.requestFrom = CMD_SENDING;
    twi->send_to(ogtwi[0], &ogtwi[2], ogtwi[1]);
}

unsigned char parse_in_cmd(unsigned char n, char const *buf) {
    for (unsigned char ch = 0; ch < n; ++ch) {
        if (buf[ch] == 'W') {
            if (n - ch >= 3) {
                unsigned char dlen = buf[ch + 2];
                if (n - ch >= 3 + dlen) {
                    //  got a full cmd
                    if (dlen > TWI_MAX_SIZE) {
                        uart_send_all(2, "\xede");
                    }
                    else if (twiSending) {
                        uart_send_all(2, "\xedx");
                    }
                    else {
                        twiSending = true;
                        const char *dptr = &buf[ch + 3];
                        unsigned char dnode = buf[1];
                        memcpy(&ogtwi[2], dptr, dlen);
                        ogtwi[0] = dnode;
                        ogtwi[1] = dlen;
                        after(0, send_ogtwi, 0);
                    }
                    return ch + 3 + buf[ch + 2];
                }
            }
            return ch;
        }
    }
    return n;
}

char ser_buf[35];

void poll_serial(void *ptr) {
    return;
    unsigned char left = sizeof(ser_buf) - ((char *)ptr - ser_buf);
    unsigned char n = uart_available();
    if (n > left) {
        n = left;
    }
    uart_read(n, &ser_buf[sizeof(ser_buf)-left]);
    left -= n;
    while ((n = parse_in_cmd(left, ser_buf)) > 0) {
        memmove(ser_buf, &ser_buf[n], sizeof(ser_buf)-left-n);
        left += n;
    }
}

void setup(void) {
    fatal_set_blink(&blink);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    setup_timers(F_CPU);
    uart_setup(115200, F_CPU);
    delay(200);
    uart_send_all(2, "\xedO");
    digitalWrite(LED_PIN, LOW);
    twi = start_twi_master(&twiMaster);
    adc_setup(false);
    after(0, request_from_boards, 0);
    after(2000, request_from_usbboard, 0);
    after(0, poll_voltage, 0);
    after(0, poll_serial, ser_buf);
}

