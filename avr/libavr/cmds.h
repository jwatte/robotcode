
#if !defined(libavr_cmds_h)
#define libavr_cmds_h

/* there really should be a librobo in addition to libavr ... */

#define ESTOP_RF_CHANNEL 48
#define ESTOP_RF_ADDRESS 1228

enum Node {
    NodeAny,
    NodeMotorPower,     //  1
    NodeEstop,          //  2
    NodeSensorInput,    //  3
    NodeUSBInterface,   //  4
    NodeIMU,            //  5
    NodeDisplay,        //  6

    NodeCount
};

enum RegType {
    RegTypeUnknown,
    RegTypeUchar,
    RegTypeUchar16,
    RegTypeSchar,
    RegTypeUshort,
    RegTypeSshort
};

struct Cmd {
    unsigned char reg_start;
    unsigned char reg_count;
};

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
    unsigned char screen;
    unsigned char field;
    unsigned char size;
    unsigned char value[26];
};

char hexchar(unsigned char nybble);
void nybbles(char *oData, unsigned char bsz, unsigned char const *value, unsigned char vsz);
void format_value(void const *src, RegType type, unsigned char bufsz, char *oData);

#endif // libavr_cmds_h

