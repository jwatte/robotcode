#if !defined(ServoSet_h)
#define ServoSet_h

#include <boost/shared_ptr.hpp>

class USBLink;
class Module;
class Logger;

struct servo_cmd {
    unsigned char id;
    unsigned char reg;  //  high bit set if two-byte
    unsigned short value;
};

struct cmd_pose {
    unsigned char id;
    unsigned short pose;
};

enum ServoReg {
    //  EEPROM
    REG_MODEL_NUMBER = 0,
    REG_MODEL_NUMBER_HI = 1,
    REG_VERSION = 2,
    REG_ID = 3,
    REG_BAUD_RATE = 4,
    REG_RETURN_DELAY_TIME = 5,
    REG_CW_ANGLE_LIMIT = 6,
    REG_CW_ANGLE_LIMIT_HI = 7,
    REG_CCW_ANGLE_LIMIT = 8,
    REG_CCW_ANGLE_LIMIT_HI = 9,
    REG_HIGHEST_LIMIT_TEMPERATURE = 11,
    REG_LOWEST_LIMIT_VOLTAGE = 12,
    REG_HIGHEST_LIMIT_VOLTAGE = 13,
    REG_MAX_TORQUE = 14,
    REG_MAX_TORQUE_HI = 15,
    REG_STATUS_RETURN_LEVEL = 16,
    REG_ALARM_LED = 17,
    REG_ALARM_SHUTDOWN = 18,

    //  Volatile
    REG_TORQUE_ENABLE = 24,
    REG_LED = 25,
    REG_D_GAIN = 26,
    REG_I_GAIN = 27,
    REG_P_GAIN = 28,
    REG_GOAL_POSITION = 30,
    REG_GOAL_POSITION_HI = 31,
    REG_MOVING_SPEED = 32,
    REG_MOVING_SPEED_HI = 33,
    REG_TORQUE_LIMIT = 34,
    REG_TORQUE_LIMIT_HI = 35,
    REG_PRESENT_POSITION = 36,
    REG_PRESENT_POSITION_HI = 37,
    REG_PRESENT_SPEED = 38,
    REG_PRESENT_SPEED_HI = 39,
    REG_PRESENT_LOAD = 40,
    REG_PRESENT_LOAD_HI = 41,
    REG_PRESENT_VOLTAGE = 42,
    REG_PRESENT_TEMPERATURE = 43,
    REG_REGISTERED = 44,
    REG_MOVING = 46,
    REG_LOCK = 47,
    REG_PUNCH = 48,
    REG_PUNCH_HI = 49,
    REG_CURRENT = 68,
    REG_CURRENT_HI = 69,
    REG_TORQUE_CONTROL_MODE_ENABLE = 70,
    REG_GOAL_TORQUE = 71,
    REG_GOAL_TORQUE_HI = 72,
    REG_GOAL_ACCELERATION = 73,

    NUM_SERVO_REGS = 74
};

class ServoSet;

class Servo {
public:
    Servo(unsigned char id, unsigned short neutral, ServoSet &ss);
    ~Servo();

    void set_goal_position(unsigned short gp);
    unsigned short get_goal_position() const;

    unsigned short get_present_position() const;
    unsigned short get_present_speed() const;
    unsigned short get_present_load() const;
    unsigned char get_present_voltage() const;
    unsigned char get_present_temperature() const;

    void set_lock(unsigned char l);
    unsigned char get_lock() const;

    void set_punch(unsigned short p);
    unsigned short get_punch() const;

    //  low-level access
    void set_reg1(unsigned char reg, unsigned char val);
    void set_reg2(unsigned char reg, unsigned short val);
    unsigned char get_reg1(unsigned char reg) const;
    unsigned short get_reg2(unsigned char reg) const;
    unsigned int queue_depth() const;

    void set_torque(unsigned short thousandths, unsigned char steps);

private:

    friend class ServoSet;
    ServoSet &ss_;
    unsigned char registers_[NUM_SERVO_REGS];
    unsigned char id_;
    unsigned char lastSlowRd_;
    unsigned char updateTorque_;
    unsigned char torqueSteps_;
    unsigned short neutral_;
    unsigned short prevTorque_;
    unsigned short nextTorque_;
};

class ServoSet {
public:
    ServoSet(bool usb, boost::shared_ptr<Logger> const &l);
    ~ServoSet();

    Servo &add_servo(unsigned char id, unsigned short neutral = 2048);
    Servo &id(unsigned char id);
    bool torque_pending();
    void step();
    void set_torque(unsigned short thousandths, unsigned char steps = 1);
    unsigned int queue_depth();
    //  byte 0 is the number of detected droped byte events.
    //  byte 1 .. n is status byte for servo 0 .. n-1.
    //  return value is OR of servo status bytes.
    unsigned char get_status(unsigned char *bytes, unsigned char cnt);
    //  get a particular register across all servos
    void slice_reg1(unsigned char reg, unsigned char *bytes, unsigned char cnt);
    void slice_reg2(unsigned char reg, unsigned short *bytes, unsigned char cnt);
    //  set many poses over some future amount of time
    void lerp_pose(unsigned short ms, cmd_pose const *pose, unsigned char npose);
    unsigned char battery();
    unsigned char dips();
    //  this is needed because ServoSet actually does protocol 
    //  framing, which ought to live in USBLink
    void raw_cmd(void const *data, unsigned char sz);

private:
    friend class Servo;
    std::vector<boost::shared_ptr<Servo>> servos_;
    std::vector<servo_cmd> cmds_;
    std::vector<unsigned char> status_;
    boost::shared_ptr<Module> usbModule_;
    double lastStep_;
    double lastSend_;
    USBLink *usb_;
    size_t pollIx_;
    unsigned short torqueLimit_;
    unsigned short torqueSteps_;
    unsigned char lastServoId_;
    unsigned char lastSeq_;
    unsigned char nextSeq_;
    unsigned char battery_;
    unsigned char dips_;

    void add_cmd(servo_cmd const &cmd);
    unsigned char do_read_complete(unsigned char const *pack, unsigned char sz);
    unsigned char do_status_complete(unsigned char const *pack, unsigned char sz);
};

#endif  //  ServoSet_h

