#include "Boards.h"
#include "protocol.h"
#include "Settings.h"

boost::shared_ptr<Module> MotorBoard::open(boost::shared_ptr<Settings> const &set) {
    return boost::shared_ptr<Module>(new MotorBoard());
}

static std::string str_motorboard = "Motor board";
static std::string str_cmd_power = "w_cmd_power";
static std::string str_cmd_steer = "w_cmd_steer";
static std::string str_e_allow = "w_e_allow";
static std::string str_trim_power = "w_trim_power";
static std::string str_trim_steer = "w_trim_steer";
static std::string str_actual_power = "r_actual_power";
static std::string str_self_stop = "r_self_stop";
static std::string str_e_conn = "r_e_conn";
static std::string str_voltage = "r_voltage";
static std::string str_debug_bits = "r_debug_bits";
static std::string str_last_fatal = "r_last_fatal";


MotorBoard::MotorBoard() :
    Board(str_motorboard, sizeof(info_MotorPower), MOTOR_BOARD) {

    add_schar_prop(str_cmd_power, 0, 1.0/127.0);
    add_schar_prop(str_cmd_steer, 1, 1.0/127.0);
    add_uchar_prop(str_e_allow, 2, 0);
    add_uchar_prop(str_trim_power, 3, 1.0/255.0);
    add_schar_prop(str_trim_steer, 4, 1.0/127.0);

    add_schar_prop(str_actual_power, 5, 1.0/127.0);
    add_uchar_prop(str_self_stop, 6, 0);
    add_uchar_prop(str_e_conn, 7, 0);
    add_uchar_prop(str_voltage, 8, 16.0 / 256);
    add_uchar_prop(str_debug_bits, 9, 0);
    add_uchar_prop(str_last_fatal, 10, 0);
}



struct ir_fun {
    long max_value;
    double distance_at_max;
    long min_value;
    double distance_at_min;
    double s_curve;
};

static double smoothstep(double t, double a, double b) {
    double x = t * t * (3 - 2 * t);
    return (x * (b - a)) + a;
}

class IRTranslator : public Translator<double> {
public:
    IRTranslator(ir_fun const &fn) :
        fn_(fn) {
    }
    ir_fun const fn_;
    double translate(void const *src) {
        unsigned char uc = *(unsigned char const *)src;
        if (uc > fn_.max_value) {
            return fn_.distance_at_max;
        }
        if (uc < fn_.min_value) {
            return fn_.distance_at_min;
        }
        double d = (uc - fn_.min_value) / (double)(fn_.max_value - fn_.min_value);
        return smoothstep(pow(d, fn_.s_curve), fn_.distance_at_max, fn_.distance_at_min);
    }
};

boost::shared_ptr<Module> InputBoard::open(boost::shared_ptr<Settings> const &set) {
    ir_fun ifn = {
        255,    //  max_value
        0.4,    //  distance_at_max
        60,     //  min_value
        1.5,    //  distance_at_min
        1       //  s_curve
    };
    maybe_get(set, "max_value", ifn.max_value);
    maybe_get(set, "distance_at_max", ifn.distance_at_max);
    maybe_get(set, "min_value", ifn.min_value);
    maybe_get(set, "distance_at_min", ifn.distance_at_min);
    maybe_get(set, "s_curve", ifn.s_curve);
    return boost::shared_ptr<Module>(new InputBoard(ifn));
}

static std::string str_InputBoard = "Input board";
static std::string str_w_laser0 = "w_laser0";
static std::string str_w_laser1 = "w_laser1";
static std::string str_w_laser2 = "w_laser2";
static std::string str_r_ir0 = "r_ir0";
static std::string str_r_ir1 = "r_ir1";
static std::string str_r_ir2 = "r_ir2";
static std::string str_r_us0 = "r_us0";
static std::string str_r_us1 = "r_us1";
static std::string str_r_us2 = "r_us2";
static std::string str_r_iter = "r_iter";

InputBoard::InputBoard(ir_fun const &fn) :
    Board(str_InputBoard, sizeof(info_SensorInput), SENSOR_BOARD) {

    add_uchar_prop(str_w_laser0, 0, 0);
    add_uchar_prop(str_w_laser1, 1, 0);
    add_uchar_prop(str_w_laser2, 2, 0);
    boost::shared_ptr<Translator<double>> xlat(new IRTranslator(fn));
    add_prop(str_r_ir0, 3, xlat);
    add_prop(str_r_ir1, 4, xlat);
    add_prop(str_r_ir2, 5, xlat);
    add_uchar_prop(str_r_us0, 6, 0.01);
    add_uchar_prop(str_r_us1, 7, 0.01);
    add_uchar_prop(str_r_us2, 8, 0.01);
    add_uchar_prop(str_r_iter, 9, 0);
}



boost::shared_ptr<Module> USBBoard::open(boost::shared_ptr<Settings> const &set) {
    double top_voltage = 16.0;
    maybe_get(set, "top_voltage", top_voltage);
    return boost::shared_ptr<Module>(new USBBoard(top_voltage));
}

static std::string str_USBBoard = "USB board";
static std::string str_r_voltage = "r_voltage";

USBBoard::USBBoard(double top_voltage) :
    Board(str_USBBoard, sizeof(info_USBInterface), USB_BOARD) {

    add_uchar_prop(str_r_voltage, 0, top_voltage/256);
}




boost::shared_ptr<Module> IMUBoard::open(boost::shared_ptr<Settings> const &set) {
    return boost::shared_ptr<Module>(new IMUBoard());
}

static std::string str_IMUBoard = "IMU board";
static std::string str_r_mag_x = "r_mag_x";
static std::string str_r_mag_y = "r_mag_y";
static std::string str_r_mag_z = "r_mag_z";
static std::string str_r_accel_x = "r_accel_x";
static std::string str_r_accel_y = "r_accel_y";
static std::string str_r_accel_z = "r_accel_z";
static std::string str_r_gyro_x = "r_gyro_x";
static std::string str_r_gyro_y = "r_gyro_y";
static std::string str_r_gyro_z = "r_gyro_z";

IMUBoard::IMUBoard() :
    Board(str_IMUBoard, sizeof(info_IMU), IMU_BOARD) {

    add_sshort_prop(str_r_mag_x, 0, 2.0 / 32767);
    add_sshort_prop(str_r_mag_y, 2, 2.0 / 32767);
    add_sshort_prop(str_r_mag_z, 4, 2.0 / 32767);
    add_sshort_prop(str_r_accel_x, 6, 2.0 / 32767);
    add_sshort_prop(str_r_accel_y, 8, 2.0 / 32767);
    add_sshort_prop(str_r_accel_z, 10, 2.0 / 32767);
    add_sshort_prop(str_r_gyro_x, 12, 2.0 / 32767);
    add_sshort_prop(str_r_gyro_y, 14, 2.0 / 32767);
    add_sshort_prop(str_r_gyro_z, 16, 2.0 / 32767);
}


