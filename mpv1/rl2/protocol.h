#if !defined(rl2_protocol_h)
#define rl2_protocol_h

#define BEGIN_PACKET        0xed
#define CMD_BOARD_UPDATE    0x44
#define MOTOR_BOARD         0x01
#define SENSOR_BOARD        0x03
#define USB_BOARD           0x04
#define IMU_BOARD           0x05
#define DISPLAY_BOARD       0x06

#define MAX_BOARD_INDEX     0x07

struct info_MotorPower {
    unsigned char w_cmd_power;  //  -127 back, 127 forward
    unsigned char w_cmd_steer;  //  -90 left, 90 right
    unsigned char w_e_allow;    //  0 stop, !0 go
    unsigned char w_trim_power; //  PWM modulator
    unsigned char w_trim_steer; //  -90 .. 90 add to steer

    unsigned char r_actual_power;
    unsigned char r_self_stop;
    unsigned char r_e_conn;
    unsigned char r_voltage;
    unsigned char r_debug_bits;

    unsigned char r_last_fatal;
};

/*
Nobody really asks for this
struct info_Estop {
    unsigned char r_m_conn;
};
*/

struct info_SensorInput {
    unsigned char w_laser[3];
    unsigned char r_ir[3];
    unsigned char r_us[3];
    unsigned char r_iter;
};

struct info_USBInterface {
    unsigned char r_voltage;
    unsigned char r_badsync;
    unsigned char r_badcmd;
    unsigned char r_naks;
    unsigned char r_overrun;
};

struct info_IMU {
    unsigned short r_mag[3];
    unsigned short r_accel[3];
    unsigned short r_gyro[3];
};

struct info_Display {
    unsigned char counter;
    unsigned char buttons;
    unsigned char spinner;
};

enum {
    cmdSetColors = 0x21,
    cmdDrawText = 0x22,
    cmdFillRect = 0x23
};
enum {
    fillFlagFront = 1,
    fillFlagHeight = 0x40,
    fillFlagYCoord = 0x80
};

struct cmd_Display_setColors {
    unsigned char cmd;
    unsigned short front;
    unsigned short back;
};

struct cmd_Display_drawText {
    unsigned char cmd;
    unsigned char x;
    unsigned char y;
    //  the high bit of 'len' is the lowest bit of y
    unsigned char len;
    char text[26];
};

struct cmd_Display_fillRect {
    unsigned char cmd;
    unsigned char x;
    unsigned char y;
    unsigned char w;
    unsigned char h;
    unsigned char flags;   //  high bit is low bit of y, low bit is front/back
};

#endif  //  rl2_protocol_h
