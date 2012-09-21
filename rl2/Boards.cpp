#include "Boards.h"
#include "protocol.h"

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



