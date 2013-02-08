
#include "Settings.h"
#include "ServoSet.h"
#include "USBLink.h"
#include "util.h"
#include <stdexcept>
#include <boost/lexical_cast.hpp>


#define SEQ_TIMEOUT 1
#define SEQ_RETRY 0.1
#define MIN_SEND_PERIOD 0.075


#define SET_REG1 0x13
#define SET_REG2 0x14
#define GET_STATUS 0x20
#define GET_REGS 0x23
#define DELAY 0x31
#define NOP 0xf0
#define LERP 0xf3

#define READ_COMPLETE 0x41
#define STATUS_COMPLETE  0x51

#define DEFAULT_TORQUE_LIMIT 768    //  3/4 of max power

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
    neutral_(neutral),
    id_(id),
    ss_(ss),
    lastSlowRd_(0),
    updateTorque_(10) {
    memset(registers_, 0, sizeof(registers_));
    set_goal_position(neutral);
}

Servo::~Servo() {
}


void Servo::set_goal_position(unsigned short gp) {
    if (gp > 4095) {
        gp = 4095;
    }
    set_reg2(REG_GOAL_POSITION, gp);
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




ServoSet::ServoSet() {
    boost::shared_ptr<Settings> st(Settings::load("settings.ini"));
    usbModule_ = USBLink::open(st);
    usb_ = usbModule_->cast_as<USBLink>();
    pollIx_ = 0;
    torqueLimit_ = DEFAULT_TORQUE_LIMIT; //  some fraction of max power
    lastServoId_ = 0;
    lastSeq_ = nextSeq_ = 0;
    lastStep_ = 0;
    lastSend_ = 0;
    battery_ = 0;
    //  Compensate for a bug: first packet doesn't register unless 
    //  the receiver board is freshly reset.
    unsigned char nop[] = { 0, NOP };
    usb_->raw_send(&nop, 2);
    //  broadcast turn off torque
    unsigned char disable_torque_pack[] = {
        0, SET_REG1, 0xfe, REG_TORQUE_ENABLE, 0,
    };
    usb_->raw_send(disable_torque_pack, sizeof(disable_torque_pack));
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
    servos_[id] = boost::shared_ptr<Servo>(new Servo(id, neutral, *this));
    unsigned char set_regs_pack[] = {
        nextSeq_,
        SET_REG1, id, REG_STATUS_RETURN_LEVEL, 1,           //  set reg
        DELAY, 0,
        SET_REG1, id, REG_RETURN_DELAY_TIME, 2,
        SET_REG1, id, REG_ALARM_LED, 0x7C,      //  everything except voltage and angle limit
        SET_REG1, id, REG_ALARM_SHUTDOWN, 0x4,  //  temperature
        SET_REG1, id, REG_HIGHEST_LIMIT_TEMPERATURE, 75,
        SET_REG1, id, REG_HIGHEST_LIMIT_VOLTAGE, 170,
        SET_REG1, id, REG_LOWEST_LIMIT_VOLTAGE, 120,
        SET_REG2, id, REG_TORQUE_LIMIT, 103, 0,     //  10% of full torque to start out
        SET_REG2, id, REG_GOAL_POSITION, (unsigned char)(neutral & 0xff), (unsigned char)((neutral >> 8) & 0xff),
        SET_REG2, id, REG_MOVING_SPEED, 0, 0,       //  set speed at max
        SET_REG1, id, REG_LOCK, 1,
        SET_REG1, id, REG_TORQUE_ENABLE, 1,
    };
    ++nextSeq_;
    usb_->raw_send(set_regs_pack, sizeof(set_regs_pack));
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
                std::cerr << "forcing a seq update from " << (int)lastSeq_ << " to "
                    << (lastSeq_+1) << " with nextSeq_ " << (int)nextSeq_ << std::endl;
                lastSend_ = now - SEQ_TIMEOUT + SEQ_RETRY;
                lastSeq_++;
            }
        }
    }

    //  select next servo
    if (timeready && servos_.size() && ((unsigned char)(nextSeq_ - lastSeq_) < 3)) {
        buf[bufptr++] = nextSeq_;
        ++nextSeq_;
        while (true) {
            ++lastServoId_;
            if (lastServoId_ >= servos_.size()) {
                lastServoId_ = -1;  //  so that next iteration becomes 0
                //  read the status for all servos
                buf[bufptr++] = GET_STATUS;
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
                    if (!s.updateTorque_) {
                        s.set_reg2(REG_TORQUE_LIMIT, torqueLimit_);
                        std::cerr << "torque for servo " << (int)lastServoId_ << " is " << torqueLimit_ << std::endl;
                    }
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
            usb_->raw_send(buf, bufptr);
            cmds_.erase(cmds_.begin(), ptr);
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
            unsigned char cnt = 1;
            switch (*d) {
            case READ_COMPLETE:
                cnt = do_read_complete(d, sz);
                break;
            case STATUS_COMPLETE:
                cnt = do_status_complete(d, sz);
                break;
            default:
                std::cerr << hexnum(*d) << " ";
                break;
            }
            d += cnt;
            sz -= cnt;
        }
        usb_->end_receive(szs + 1);
    }
}

void ServoSet::set_torque(unsigned short thousandths) {
    if (thousandths >= 1024) {
        throw std::runtime_error("Bad argument to ServoSet::set_torque().");
    }
    torqueLimit_ = thousandths;
    for (auto ptr(servos_.begin()), end(servos_.end()); ptr != end; ++ptr) {
        if (!!*ptr && !(*ptr)->updateTorque_) {
            //  catch this guy up the next time I see him
            (*ptr)->updateTorque_ = 1;
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
    cmds_.push_back(cmd);
}

static int nincomplete = 0;

unsigned char ServoSet::do_read_complete(unsigned char const *pack, unsigned char sz) {
    
    if (sz < 5 || sz < pack[3] + 4) {
        if (nincomplete < 10) {
            std::cerr << "short read complete packet; got " << (int)sz << " wanted " << 
                ((sz < 5) ? "min 5" : boost::lexical_cast<std::string>(pack[3] + 4)) << " bytes." << std::endl;
            nincomplete++;
        }
        return sz;
    }
    if (pack[1] >= servos_.size() || !servos_[pack[1]]) {
        std::cerr << "read complete for non-existent servo " << (int)pack[1] << std::endl;
        return 4 + pack[3];
    }
    if (pack[2] >= NUM_SERVO_REGS || pack[3] > NUM_SERVO_REGS - pack[2]) {
        std::cerr << "read complete with bad offset " << (int)pack[2] << " size " << (int)pack[3] << std::endl;
        return 4 + pack[3];
    }
    Servo &s = id(pack[1]);
    memcpy(&s.registers_[pack[2]], &pack[4], pack[3]);

    if (nincomplete > 0) {
        --nincomplete;
    }
    return 4 + pack[3];
}

unsigned char ServoSet::do_status_complete(unsigned char const *pack, unsigned char sz) {
    if (sz < 2 || sz < 2 + pack[1]) {
        if (nincomplete < 10) {
            std::cerr << "short read status packet; got " << (int)sz << "wanted " <<
                ((sz < 2) ? "min 2" : boost::lexical_cast<std::string>(pack[1] + 2)) << " bytes." << std::endl;
            nincomplete++;
        }
        return sz;
    }
    status_.resize(pack[1]);
    battery_ = pack[2];
    if (pack[1]) {
        memcpy(&status_[0], &pack[3], pack[1]);
    }
    if (nincomplete > 0) {
        --nincomplete;
    }
    return 2 + pack[1];
}

unsigned char ServoSet::get_status(unsigned char *buf, unsigned char n) {
    if (status_.size()) {
        if (n > status_.size()) {
            n = status_.size();
        }
        memcpy(buf, &status_[0], n);
        return status_.size();
    }
    return 0;
}

unsigned char ServoSet::battery() {
    return battery_;
}

void ServoSet::lerp_pose(unsigned short ms, cmd_pose const *pose, unsigned char npose) {
    if (npose > 19) {
        throw std::runtime_error("attempt to lerp_pose() with too many servos");
    }
    unsigned char cmd[64];
    cmd[0] = nextSeq_;
    ++nextSeq_;
    cmd[1] = LERP;
    cmd[2] = ms & 0xff;
    cmd[3] = (ms >> 8) & 0xff;
    cmd[4] = npose;
    unsigned char ptr = 5;
    for (unsigned char i = 0; i != npose; ++i) {
        if (pose[i].id >= servos_.size() || !servos_[pose[i].id]) {
            throw std::runtime_error("invalid servo ID in lerp_pose()");
        }
        cmd[ptr++] = pose[i].id;
        if (pose[i].pose > 4095) {
            throw std::runtime_error("invalid servo pose in lerp_pose()");
        }
        cmd[ptr++] = pose[i].pose & 0xff;
        cmd[ptr++] = (pose[i].pose >> 8) & 0xff;
    }
    //  immediately send the command
    usb_->raw_send(cmd, ptr);
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
