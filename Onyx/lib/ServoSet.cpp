
#include "Settings.h"
#include "ServoSet.h"
#include "USBLink.h"
#include "util.h"
#include "istatus.h"
#include <stdexcept>
#include <boost/lexical_cast.hpp>


#define SEQ_TIMEOUT 1
#define SEQ_RETRY 0.1
#define MIN_SEND_PERIOD 0.075

#if ALL_PID
    #define D_GAIN 8
    #define I_GAIN 12
    #define P_GAIN 24
#else
    #define D_GAIN 0
    #define I_GAIN 0
    #define P_GAIN 32
#endif

#define DEFAULT_TORQUE_LIMIT 1023
#define DEFAULT_TORQUE_STEPS 1

#define MAX_OUTSTANDING_PACKETS 3

static const unsigned char read_regs[] = {
    REG_MODEL_NUMBER,
    REG_MODEL_NUMBER_HI,
    REG_VERSION,
    REG_ID,
    REG_BAUD_RATE,
    REG_RETURN_DELAY_TIME,
    REG_CW_ANGLE_LIMIT,
    REG_CW_ANGLE_LIMIT_HI,
    REG_CCW_ANGLE_LIMIT,
    REG_CCW_ANGLE_LIMIT_HI,
    REG_HIGHEST_LIMIT_TEMPERATURE,
    REG_LOWEST_LIMIT_VOLTAGE,
    REG_HIGHEST_LIMIT_VOLTAGE,
    REG_MAX_TORQUE,
    REG_MAX_TORQUE_HI,
    REG_STATUS_RETURN_LEVEL,
    REG_ALARM_LED,
    REG_ALARM_SHUTDOWN,

    //  Volatile
    REG_TORQUE_ENABLE,
    REG_LED,
    REG_D_GAIN,
    REG_I_GAIN,
    REG_P_GAIN,
    REG_GOAL_POSITION,
    REG_GOAL_POSITION_HI,
    REG_MOVING_SPEED,
    REG_MOVING_SPEED_HI,
    REG_TORQUE_LIMIT,
    REG_TORQUE_LIMIT_HI,
    REG_PRESENT_POSITION,
    REG_PRESENT_POSITION_HI,
    REG_PRESENT_SPEED,
    REG_PRESENT_SPEED_HI,
    REG_PRESENT_LOAD,
    REG_PRESENT_LOAD_HI,
    REG_PRESENT_VOLTAGE,
    REG_PRESENT_TEMPERATURE,
    REG_REGISTERED,
    REG_MOVING,
    REG_LOCK,
    REG_PUNCH,
    REG_PUNCH_HI,
    REG_CURRENT,
    REG_CURRENT_HI,
    REG_TORQUE_CONTROL_MODE_ENABLE,
    REG_GOAL_TORQUE,
    REG_GOAL_TORQUE_HI,
    REG_GOAL_ACCELERATION,
};

static const unsigned char frequent_read_regs[] = {
    REG_PRESENT_POSITION, 8,
    REG_CURRENT, 2
};

Servo::Servo(unsigned char id, unsigned short neutral, ServoSet &ss) :
    ss_(ss),
    id_(id),
    lastSlowRd_(0),
    updateTorque_(10),
    torqueSteps_(10),
    neutral_(neutral),
    prevTorque_(0),
    nextTorque_(206)
{
    memset(registers_, 0, sizeof(registers_));
    set_goal_position(neutral);
}

void Servo::set_torque(unsigned short thousandths, unsigned char steps)
{
    if (thousandths >= 1024) {
        throw std::runtime_error("Bad torque argument to Servo::set_torque().");
    }
    if (steps == 0 || steps > 30) {
        throw std::runtime_error("Bad steps argument to Servo::set_torque().");
    }
    prevTorque_ = (prevTorque_ * updateTorque_ + nextTorque_ * (torqueSteps_ - updateTorque_)) / torqueSteps_;
    nextTorque_ = thousandths;
    torqueSteps_ = steps;
    updateTorque_ = steps;
}


Servo::~Servo() {
}


void Servo::set_goal_position(unsigned short gp) {
    if (gp > 4095) {
        gp = 4095;
    }
    //  don't update the position if it's already updated
    if (get_reg2(REG_GOAL_POSITION) != gp) {
        set_reg2(REG_GOAL_POSITION, gp);
    }
}

unsigned short Servo::get_goal_position() const {
    return get_reg2(REG_GOAL_POSITION);
}

unsigned short Servo::get_present_position() const {
    return get_reg2(REG_PRESENT_POSITION);
}

unsigned short Servo::get_present_speed() const {
    return get_reg2(REG_PRESENT_SPEED);
}

unsigned short Servo::get_present_load() const {
    return get_reg2(REG_PRESENT_LOAD);
}

unsigned char Servo::get_present_voltage() const {
    return get_reg1(REG_PRESENT_VOLTAGE);
}

unsigned char Servo::get_present_temperature() const {
    return get_reg1(REG_PRESENT_TEMPERATURE);
}

void Servo::set_lock(unsigned char l) {
    if (l != 0) {
        l = 1;
    }
    set_reg1(REG_LOCK, l);
}

unsigned char Servo::get_lock() const {
    return get_reg1(REG_LOCK);
}

void Servo::set_punch(unsigned short p) {
    if (p > 4095) {
        p = 4095;
    }
    set_reg2(REG_PUNCH, p);
}

unsigned short Servo::get_punch() const {
    return get_reg2(REG_PUNCH);
}

void Servo::set_reg1(unsigned char reg, unsigned char val) {
    assert(reg < NUM_SERVO_REGS);
    servo_cmd sc;
    sc.id = id_;
    sc.reg = reg;
    sc.value = val;
    ss_.add_cmd(sc);
    registers_[reg] = val;
}

void Servo::set_reg2(unsigned char reg, unsigned short val) {
    assert(reg < NUM_SERVO_REGS - 1);
    assert(val < 4096);
    servo_cmd sc;
    sc.id = id_;
    sc.reg = reg | 0x80;
    sc.value = val;
    ss_.add_cmd(sc);
    registers_[reg] = val & 0xff;
    registers_[reg + 1] = (val >> 8) & 0xff;
}

unsigned char Servo::get_reg1(unsigned char reg) const {
    assert(reg < NUM_SERVO_REGS);
    return registers_[reg];
}

unsigned short Servo::get_reg2(unsigned char reg) const {
    assert(reg < NUM_SERVO_REGS - 1);
    return registers_[reg] + (registers_[reg + 1] << 8);
}

unsigned int Servo::queue_depth() const {
    unsigned int cnt = 0;
    for (std::vector<servo_cmd>::iterator ptr(ss_.cmds_.begin()), end(ss_.cmds_.end());
        ptr != end;
        ++ptr) {
        if ((*ptr).id == id_) {
            ++cnt;
        }
    }
    return cnt;
}


namespace {
class DummyStatus : public IStatus {
    void message(std::string const &msg) {
        std::cerr << msg << std::endl;
    }
    void error(std::string const &msg) {
        std::cerr << msg << std::endl;
    }
    size_t n_messages() {
        return 0;
    }
    bool get_message(Message &o_message) {
        return false;
    }
};
DummyStatus dummy_status;
}


ServoSet::ServoSet(bool usb, boost::shared_ptr<Logger> const &l, IStatus *status) {
    if (!status) {
        status = &dummy_status;
    }
    istatus_ = status;
    boost::shared_ptr<Settings> st(Settings::load("settings.ini"));
    if (usb) {
        usbModule_ = USBLink::open(st, l);
        usb_ = usbModule_->cast_as<USBLink>();
    }
    else {
        usb_ = nullptr;
    }
    pollIx_ = 0;
    torqueLimit_ = DEFAULT_TORQUE_LIMIT; //  some fraction of max power
    torqueSteps_ = DEFAULT_TORQUE_STEPS;
    lastServoId_ = 0;
    lastSeq_ = nextSeq_ = 0;
    lastOutSeq_ = -1;
    lastStep_ = 0;
    lastSend_ = 0;
    battery_ = 0;
    power_ = 0;
    powerFail_ = 0;
    if (usb) {
        //  Compensate for a bug: first packet doesn't register unless 
        //  the receiver board is freshly reset (?!)
        unsigned char nop[] = { 0, OpGetStatus | 1, TargetPower };
        usb_->raw_send(&nop, sizeof(nop));
        //  broadcast turn off torque
        unsigned char disable_torque_pack[] = {
            0, OpWriteServo | 3, 0xfe, REG_TORQUE_ENABLE, 0,
        };
        usb_->raw_send(disable_torque_pack, sizeof(disable_torque_pack));
    }
}

ServoSet::~ServoSet() {
}



Servo &ServoSet::add_servo(unsigned char id, unsigned short neutral) {
    assert(id > 0);
    assert(id < 254);
    assert(neutral < 4096);
    if (servos_.size() <= id) {
        servos_.resize(id + 1);
    }
    if (!!servos_[id]) {
        throw new std::runtime_error("Servo with duplicate ID added.");
    }

#define SET_REG1 (OpWriteServo | 3)
#define SET_REG2 (OpWriteServo | 4)
#define GET_REGS (OpReadServo | 3)

    servos_[id] = boost::shared_ptr<Servo>(new Servo(id, neutral, *this));
    unsigned short torque = std::min((unsigned short)103, torqueLimit_);
    if (!!usb_) {
        unsigned char set_regs_pack[] = {
            nextSeq_,
            SET_REG1, id, REG_STATUS_RETURN_LEVEL, 1,           //  set reg
            SET_REG1, id, REG_RETURN_DELAY_TIME, 2,
            SET_REG1, id, REG_ALARM_LED, 0x7C,       //  everything except voltage and angle limit
            SET_REG1, id, REG_ALARM_SHUTDOWN, 0x24,  //  temperature, overload
            SET_REG1, id, REG_HIGHEST_LIMIT_VOLTAGE, 168,
            SET_REG1, id, REG_LOWEST_LIMIT_VOLTAGE, 96,
            SET_REG2, id, REG_TORQUE_LIMIT, (unsigned char)(torque & 0xff), (unsigned char)((torque >> 8) & 0xff),     //  10% of full torque to start out
            SET_REG2, id, REG_GOAL_POSITION, (unsigned char)(neutral & 0xff), (unsigned char)((neutral >> 8) & 0xff),
            SET_REG2, id, REG_MOVING_SPEED, 0, 0,       //  set speed at max
        };
        usb_->raw_send(set_regs_pack, sizeof(set_regs_pack));
        ++nextSeq_;
        unsigned char set_regs_pack2[] = {
            nextSeq_,
            SET_REG1, id, REG_D_GAIN, D_GAIN,
            SET_REG1, id, REG_I_GAIN, I_GAIN,
            SET_REG1, id, REG_P_GAIN, P_GAIN,
            SET_REG1, id, REG_LOCK, 1,
            SET_REG1, id, REG_TORQUE_ENABLE, 1,
        };
        usb_->raw_send(set_regs_pack2, sizeof(set_regs_pack2));
    }
    ++nextSeq_;
    return *servos_[id];
}

Servo &ServoSet::id(unsigned char id) {
    if (id >= servos_.size()) {
        throw std::runtime_error("Bad servo ID index requested.");
    }
    if (!servos_[id]) {
        throw std::runtime_error("Bad servo ID index requested.");
    }
    return *servos_[id];
}

void ServoSet::step() {

    if (!usb_) {
        return;
    }

    //  pack up as many commands as can fit in a single USB packet
    unsigned char buf[56];
    unsigned char bufptr = 0;

    //  keep track of size of reply
    bool delayread = false;
    bool timeready = false;
    double now = read_clock();
    if (now - lastStep_ >= 0.001) {
        lastStep_ = floor(now * 1000) * 0.001;
        timeready = true;
        if (nextSeq_ - lastSeq_ >= 3) {
            //  force out at least one packet per 100 ms
            if (now - lastSend_ > SEQ_TIMEOUT) {
                std::stringstream strstr;
                strstr << "forcing a seq update from " << (int)lastSeq_ << " to "
                    << (lastSeq_+1) << " with nextSeq_ " << (int)nextSeq_;
                istatus_->error(strstr.str());
                lastSend_ = now - SEQ_TIMEOUT + SEQ_RETRY;
                lastSeq_++;
            }
        }
    }

    //  select next servo
    if (timeready && servos_.size()) {
        if ((unsigned char)(nextSeq_ - lastSeq_) < MAX_OUTSTANDING_PACKETS) {
            buf[bufptr++] = nextSeq_;
            ++nextSeq_;
            while (true) {
                ++lastServoId_;
                if (lastServoId_ >= servos_.size()) {
                    lastServoId_ = -1;  //  so that next iteration becomes 0
                    //  read the status for all servos
                    buf[bufptr++] = OpGetStatus | 1;
                    buf[bufptr++] = TargetPower;
                    buf[bufptr++] = OpGetStatus | 1;
                    buf[bufptr++] = TargetServos;
                    delayread = true;
                    break;
                }
                if (!!servos_[lastServoId_]) {
                    break;
                }
            }
            if (!delayread) {
                //  read stuff from the selected servo
                //  all "fast read" values
                unsigned char const *rdcmd = frequent_read_regs;
                unsigned char const *rdend = &frequent_read_regs[sizeof(frequent_read_regs)];
                while (rdcmd < rdend) {
                    buf[bufptr++] = GET_REGS;
                    buf[bufptr++] = lastServoId_;
                    buf[bufptr++] = rdcmd[0];
                    buf[bufptr++] = rdcmd[1];
                    rdcmd += 2;
                }
                //  some "slow read" values, if there's space
                Servo &s = *servos_[lastServoId_];
                if (s.lastSlowRd_ >= sizeof(read_regs)) {
                    s.lastSlowRd_ = 0;
                    if (s.updateTorque_) {
                        s.updateTorque_--;
                        unsigned short torque =
                            (s.nextTorque_ * (s.torqueSteps_ - s.updateTorque_) + s.prevTorque_ * s.updateTorque_) / s.torqueSteps_;
                        s.set_reg2(REG_TORQUE_LIMIT, torque);
                        std::stringstream strstr;
                        strstr << "torque for servo " << (int)lastServoId_ << " is " << torque << " "
                            << (int)s.updateTorque_ << "/" << (int)s.torqueSteps_;
                        istatus_->message(strstr.str());
                    }
                }
                buf[bufptr++] = GET_REGS;
                buf[bufptr++] = lastServoId_;
                buf[bufptr++] = read_regs[s.lastSlowRd_];
                buf[bufptr++] = 1;
                s.lastSlowRd_++;
                //  coalesce contiguous registers, up to a buffer size of 8
                while (s.lastSlowRd_ < sizeof(read_regs) && buf[bufptr-1] < 8) {
                    if (read_regs[s.lastSlowRd_] != buf[bufptr-2] + buf[bufptr-1]) {
                        break;
                    }
                    buf[bufptr - 1]++;
                    s.lastSlowRd_++;
                }
            }

            //  drain the command queue
            std::vector<servo_cmd>::iterator ptr(cmds_.begin()), end(cmds_.end());
            while (ptr != end) {
                if (bufptr >= sizeof(buf)-4) {
                    break;
                }
                if ((*ptr).reg & 0x80) {
                    buf[bufptr++] = SET_REG2;
                    buf[bufptr++] = (*ptr).id;
                    buf[bufptr++] = (*ptr).reg & ~0x80;
                    buf[bufptr++] = (*ptr).value & 0xff;
                    buf[bufptr++] = ((*ptr).value >> 8) & 0xff;
                }
                else {
                    buf[bufptr++] = SET_REG1;
                    buf[bufptr++] = (*ptr).id;
                    buf[bufptr++] = (*ptr).reg;
                    buf[bufptr++] = (*ptr).value & 0xff;
                }
                ++ptr;
            }
            if ((bufptr > 1) || (now - lastSend_ > MIN_SEND_PERIOD)) {
                lastSend_ = now;
                if (!!usb_) {
                    usb_->raw_send(buf, bufptr);
                }
                cmds_.erase(cmds_.begin(), ptr);
            }
            lastOutSeq_ = -1;
        }
        else {
            if (nextSeq_ != lastOutSeq_) {
            /*
                fprintf(stderr, "outstanding packets: %d-%d=%d\n",
                    nextSeq_, lastSeq_, nextSeq_-lastSeq_);
             */
                lastOutSeq_ = nextSeq_;
            }
        }
    }

    usb_->step();

    //  drain receive queue
    while (true) {
        size_t sz = 0, szs = 0;
        unsigned char const *d = usb_->begin_receive(sz);
        if (!d) {
            usb_->end_receive(0);
            break;
        }
        szs = sz;
        lastSeq_ = *d;
        d++;
        sz--;
        while (sz > 0) {
            unsigned char cnt = *d & 0xf;
            unsigned char cmd = *d & 0xf0;
            if (cnt == 15) {
                if (sz == 1) {
                    std::cerr
                        << "Short USB command packet"
                        << std::endl;
                    break;
                }
                ++d;
                --sz;
                cnt = *d;
            }
            ++d;
            --sz;
            switch (cmd) {
                case OpGetStatus:
                /*
                    std::cerr << "status complete "
                        << (int)d[0] << " " << (int)cnt
                        << std::endl;
                 */
                    do_status_complete(d, cnt);
                    break;
                case OpReadServo:
                /*
                    std::cerr << "read servo "
                        << (int)d[0] << " " << (int)cnt
                        << std::endl;
                 */
                    do_read_complete(d, cnt);
                    break;
                default:
                    std::cerr << "Unknown USB response: "
                        << (int)cmd
                        << std::endl;
            }
            d += cnt;
            assert(cnt <= sz);
            sz -= cnt;
        }
        usb_->end_receive(szs);
    }
}

void ServoSet::set_torque(unsigned short thousandths, unsigned char steps) {
    if (thousandths >= 1024) {
        throw std::runtime_error("Bad argument to ServoSet::set_torque().");
    }
    if (steps < 1 || steps > 30) {
        throw std::runtime_error("Bad step count in ServoSet::set_torque().");
    }
    torqueLimit_ = thousandths;
    torqueSteps_ = steps;
    for (auto ptr(servos_.begin()), end(servos_.end()); ptr != end; ++ptr) {
        if (!!*ptr) {
            (*ptr)->set_torque(thousandths, steps);
        }
    }
}

unsigned int ServoSet::queue_depth() {
    return cmds_.size();
}

void ServoSet::add_cmd(servo_cmd const &cmd) {
    //  already pending write in queue?
    for (std::vector<servo_cmd>::iterator ptr(cmds_.begin()), end(cmds_.end());
        ptr != end;
        ++ptr) {
        servo_cmd &sc(*ptr);
        if (sc.id == cmd.id && sc.reg == cmd.reg) {
            //  Drop older data but keep queue location for fairness
            sc = cmd;
            return;
        }
    }
    if (!!usb_) {
        cmds_.push_back(cmd);
    }
}

void ServoSet::do_read_complete(unsigned char const *pack, unsigned char sz) {
    if (sz < 3) {
        std::cerr << "short read servo data: " << sz << std::endl;;
        return;
    }
    unsigned char id = pack[0];
    unsigned int reg = pack[1];
    pack += 2;
    sz -= 2;
    if (!servos_[id]) {
        std::cerr << "servo ID not configured in read: " << id << std::endl;
        return;
    }
    Servo &s(*servos_[id]);
    if (reg >= NUM_SERVO_REGS || reg + sz > NUM_SERVO_REGS) {
        std::cerr << "servo read beyond end of registers: "
            << (int)id << " " << (int)reg << " " << (int)sz << std::endl;
        return;
    }
    memcpy(&s.registers_[reg], pack, sz);
}

void ServoSet::do_status_complete(unsigned char const *pack, unsigned char sz) {
    if (sz < 1) {
        std::cerr << "short status packet" << std::endl;
        return;
    }
    switch (*pack) {
        case TargetPower:
            do_status_power(pack+1, sz-1);
            break;
        case TargetServos:
            do_status_servos(pack+1, sz-1);
            break;
        default:
            std::cerr << "Unknown status received: " << *pack << std::endl;
            break;
    }
}

void ServoSet::do_status_power(unsigned char const *buf, unsigned char n) {
    if (n < 4) {
        std::cerr << "Bad power status size: " << n << std::endl;
    }
    battery_ = buf[0] + ((unsigned short)buf[1] << 8);
    power_ = buf[2];
    powerFail_ = buf[3];
}

void ServoSet::do_status_servos(unsigned char const *buf, unsigned char n) {
    if (status_.size() <= n) {
        status_.resize(n + 1);
    }
    memcpy(&status_[1], buf, n);
}

unsigned char ServoSet::get_status(unsigned char *buf, unsigned char n) {
    if (n > status_.size()) {
        n = status_.size();
    }
    memcpy(buf, &status_[0], n);
    return n;
}

unsigned char ServoSet::slice_reg1(unsigned char reg, unsigned char *buf, unsigned char n) {
    unsigned char top = 0;
    memset(buf, 0, n);
    for (size_t i = 0, l = std::min(servos_.size(), (size_t)n); i != l; ++i) {
        if (!!servos_[i]) {
            buf[i] = servos_[i]->get_reg1(reg);
            top = i + 1;
        }
    }
    return top;
}

unsigned char ServoSet::slice_reg2(unsigned char reg, unsigned short *buf, unsigned char n) {
    unsigned char top = 0;
    memset(buf, 0, n * 2);
    for (size_t i = 0, l = std::min(servos_.size(), (size_t)n); i != l; ++i) {
        if (!!servos_[i]) {
            buf[i] = servos_[i]->get_reg2(reg);
            top = i + 1;
        }
    }
    return top;
}

unsigned short ServoSet::battery() {
    return battery_;
}

unsigned char ServoSet::power() {
    return power_;
}

unsigned char ServoSet::power_fail() {
    return powerFail_;
}

void ServoSet::set_power(unsigned char p) {
    unsigned char pwr[3] = { OpSetStatus | 2, TargetPower, p };
    raw_cmd(pwr, 3);
}

void ServoSet::raw_cmd(void const *data, unsigned char sz) {
    char buf[32];
    if (sz > 30) {
        throw std::runtime_error("Too long raw_cmd in ServoSet");
    }
    buf[0] = nextSeq_;
    memcpy(&buf[1], data, sz);
    nextSeq_++;
    if (!!usb_) {
        usb_->raw_send(buf, sz + 1);
    }
}

bool ServoSet::torque_pending() {
    //  torque pending on at least one leg?
    for (auto ptr(servos_.begin()), end(servos_.end()); ptr != end; ++ptr) {
        if (!!*ptr && (*ptr)->updateTorque_) {
            return true;
        }
    }
    return false;
}
