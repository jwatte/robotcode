
#include "Tie.h"
#include "PropertyImpl.h"
#include "Settings.h"
#include "ModuleList.h"
#include "Boards.h"
#include "Image.h"
#include <sstream>
#include "str.h"
#include "async.h"
#include "analysis.h"



#define LOW_GAS 0.8
#define MEDIUM_GAS 0.9
#define HIGH_GAS 1.0

#define USE_LEFT_CAM 0
#define ALWAYS_FOLLOW 1

//  ed 47 15  // f2 7a 72 // ef 57 6e // dd 47 46
static Color orange_color(0xf2, 0x7a, 0x72);
static Color orange_color2((0xed + 0xef + 0xdd) / 3, (0x47 + 0x57 + 0x47) / 3,
    (0x15 + 0x6e + 0x46) / 3);

//  f7 cd 22  // ea f7 93 // cb d2 8f // 93 9f 57
static Color yellow_color(0xea, 0xf7, 0x93);
static Color yellow_color2((0xf7 + 0xcb + 0x93) / 3, (0xcd + 0xd2 + 0x9f) / 3,\
    (0x22 + 0x8f + 0x57) / 3);

enum {
    brainPropAllowed = 0,
    brainPropSpeed = 1,
    brainPropTurn = 2,
    brainPropStatusText = 3
};

template<typename T>
class BrainProp : public PropertyImpl<T> {
public:
    BrainProp(std::string const &name) :
        PropertyImpl<T>(name) {
    }
    static boost::shared_ptr<PropertyImpl<T>> create(std::string const &name) {
        return boost::shared_ptr<PropertyImpl<T>>(new BrainProp(name));
    }
};
template<typename T> struct BrainPropPtr : boost::shared_ptr<PropertyImpl<T>> {
    BrainPropPtr() {}
    BrainPropPtr(boost::shared_ptr<PropertyImpl<T>> const &sp) : boost::shared_ptr<PropertyImpl<T>>(sp) {}
};

class Brain : public TieBase, public ListenableImpl<Listenable> {
public:
    Brain(std::string const &name, boost::shared_ptr<Settings> const &set); virtual void start(boost::shared_ptr<ModuleList> const &modules);
    virtual void step();
    virtual void on_change();
    virtual std::string const & name() {
        return name_;
    }
    virtual size_t num_properties() {
        return 4;
    }
    virtual boost::shared_ptr<Property> get_property_at(size_t ix) {
        switch (ix) {
        case brainPropAllowed:      return allowedProp_;
        case brainPropSpeed:        return speedProp_;
        case brainPropTurn:         return turnProp_;
        case brainPropStatusText:   return statusProp_;
        default:
            throw std::runtime_error("get_property_at() out of range in Brain()");
        }
    }
    virtual void set_return(boost::shared_ptr<IReturn> const &r) {
        return_ = r;
    }

    std::string name_;
    BrainPropPtr<long> allowedProp_;
    BrainPropPtr<double> speedProp_;
    BrainPropPtr<double> turnProp_;
    BrainPropPtr<std::string> statusProp_;
    boost::shared_ptr<IReturn> return_;

    double force_;
    double turning_;
    double irLimit_;
    double usLimit_;

    template<typename T>
    struct PropRef {
        std::string                     name_;
        std::string                     path_;
        boost::shared_ptr<Property>     prop_;

        void setup(boost::shared_ptr<Settings> const &set, std::string const &name, std::string const &dflt,
            boost::shared_ptr<ModuleList> const &modules) {
            name_ = name;
            maybe_get(set, name, path_, dflt);
            std::vector<std::string> path;
            split(path_, '.', path);
            if (path.size() != 2) {
                throw std::runtime_error("Bad path format '" + path_ + "' in Brain " + name);
            }
            auto mod = modules->get_module_named(path[0]);
            if (!!mod) {
                prop_ = mod->get_property_named(path[1]);
            }
            if (!prop_) {
                throw std::runtime_error("Cannot find property '" + path_ + "' in Brain " + name);
            }
        }
        
        T get() {
            return prop_->get<T>();
        }
    };

    boost::shared_ptr<Settings>         set_;
#if USE_LEFT_CAM
    PropRef<boost::shared_ptr<Image>>   leftCam_;
#endif
    PropRef<boost::shared_ptr<Image>>   rightCam_;
    PropRef<double>                     leftUs_;
    PropRef<double>                     rightUs_;
    PropRef<double>                     leftIr_;
    PropRef<double>                     rightIr_;
    PropRef<double>                     backUs_;
    PropRef<double>                     downIr_;
    PropRef<long>                       selfStop_;
    PropRef<long>                       eAllow_;
    PropRef<double>                     cmdPower_;
    PropRef<double>                     cmdSteer_;
    bool                                dirty_;
};

TieReg<Brain> regBrain("Brain");

static std::string str_r_allowed("r_allowed");
static std::string str_r_speed("r_speed");
static std::string str_r_turn("r_turn");
static std::string str_r_status("r_status");

Brain::Brain(std::string const &name, boost::shared_ptr<Settings> const &set) :
    name_(name),
    allowedProp_(BrainProp<long>::create(str_r_allowed)),
    speedProp_(BrainProp<double>::create(str_r_speed)),
    turnProp_(BrainProp<double>::create(str_r_turn)),
    statusProp_(BrainProp<std::string>::create(str_r_status)),
    force_(1.0),
    turning_(0.25),
    irLimit_(0.4),
    usLimit_(0.4),
    set_(set)
{
    maybe_get(set, "title", name_);
}

void Brain::start(boost::shared_ptr<ModuleList> const &modules) {
#if USE_LEFT_CAM
    leftCam_.setup(set_, "l_cam", "/dev/_Lcam.image", modules);
#endif
    rightCam_.setup(set_, "r_cam", "/dev/_Rcam.image", modules);
    leftUs_.setup(set_, "l_us", "Input board.r_us1", modules);
    rightUs_.setup(set_, "r_us", "Input board.r_us2", modules);
    leftIr_.setup(set_, "l_ir", "Input board.r_ir1", modules);
    rightIr_.setup(set_, "r_ir", "Input board.r_ir2", modules);
    backUs_.setup(set_, "b_us", "Input board.r_us0", modules);
    downIr_.setup(set_, "d_ir", "Input board.r_ir0", modules);
    selfStop_.setup(set_, "s_stop", "Motor board.r_self_stop", modules);
    eAllow_.setup(set_, "e_allow", "Motor board.w_e_allow", modules);
    cmdPower_.setup(set_, "cmd_power", "Motor board.w_cmd_power", modules);
    cmdSteer_.setup(set_, "e_allow", "Motor board.w_cmd_steer", modules);
    maybe_get(set_, "force", force_);
    maybe_get(set_, "turning", turning_);
    maybe_get(set_, "ir_limit", irLimit_);
    maybe_get(set_, "us_limit", usLimit_);
#if USE_LEFT_CAM
    leftCam_.prop_->add_listener(boost::shared_ptr<Listener>(new ListenerTramp(this)));
#endif
    rightCam_.prop_->add_listener(boost::shared_ptr<Listener>(new ListenerTramp(this)));
}

static int npic = 0;
static int serial = 0;
static time_t lastpic = 0;
static int baddetect = 0;
static double gas = 0;
static double turn = 0;

enum States {
    StateStopped = 0,
    StateLooking = 1,
    StateFollowing = 2,
    StateTurningAround = 3
};
static int curstate = StateLooking;
static int prevstate = StateStopped;
static double enterstatetime;
static double lost_time;
static double stop_save_time;

char const *state_name(int state) {
    switch (state) {
    case StateStopped: return "Stopped";
    case StateLooking: return "Looking";
    case StateFollowing: return "Following";
    case StateTurningAround: return "Turning Around";
    default: return "Unknown";
    }
}


static double now_time() {
    struct timespec tspec;
    clock_gettime(CLOCK_MONOTONIC, &tspec);
    return (double)tspec.tv_sec + (double)tspec.tv_nsec * 1.0e-9;
}

static void stop() {
    gas = 0;
    turn = 0;
    if (curstate != StateStopped) {
        std::cerr << "stop()" << std::endl;
        prevstate = curstate;
        stop_save_time = enterstatetime;
        enterstatetime = now_time();
    }
    curstate = StateStopped;
    lost_time = 0;
}

//  any higher than 0.0025 and skin tone is picked up for orange
static float tolerance = 0.005;
static float normalization = 0.5;

static Area lastlock_yellow(0, 0, 0, 0);
static Area lastlock_orange(0, 0, 0, 0);

Area focus_yellow(Area size) {
    switch (curstate) {
        default:
        case StateLooking:
            return Area(size.width * 0.25, size.height * 0.5, size.width * 0.5, size.height * 0.5);
        case StateFollowing: {
                /* look only in the neighborhood I last found something */
                int left = lastlock_yellow.left;
                int top = lastlock_yellow.top;
                int right = lastlock_yellow.right();
                int bottom = lastlock_yellow.bottom();
                int w01 = size.width * 0.1;
                int h01 = size.height * 0.1;
                left = std::max(left - w01, 0);
                top = std::max(top - h01, 0);
                right = std::min(left + lastlock_yellow.width + 2 * w01, size.right());
                bottom = std::min(top + lastlock_yellow.height + 2 * h01, size.bottom());
                return Area(left, top, right-left, bottom-top);
            }
            break;
        case StateStopped:
        case StateTurningAround:
            /* don't look at all, really */
            return Area(0, 0, 1, 1);
    }
}

Area focus_orange(Area size) {
    switch (curstate) {
        default:
        case StateLooking:
            return Area(size.width * 0.25, size.height * 0.5, size.width * 0.5, size.height * 0.5);
        case StateFollowing: {
                /* look only in the neighborhood I last found something */
                int left = lastlock_orange.left;
                int top = lastlock_orange.top;
                int right = lastlock_orange.right();
                int bottom = lastlock_orange.bottom();
                int w01 = size.width * 0.1;
                int h01 = size.height * 0.1;
                left = std::max(left - w01, 0);
                top = std::max(top - h01, 0);
                right = std::min(left + lastlock_orange.width + 2 * w01, size.right());
                bottom = std::min(top + lastlock_orange.height + 2 * h01, size.bottom());
                return Area(left, top, right-left, bottom-top);
            }
            break;
        case StateStopped:
        case StateTurningAround:
            /* don't look at all, really */
            return Area(0, 0, 0, 0);
    }
}

static bool should_look(int state) {
    switch (state) {
        case StateLooking:
        case StateFollowing:
            return true;
        default:
            return false;
    }
}

static void looking() {
    lost_time = 0;
    if (prevstate != curstate) {
        std::cerr << "looking()" << std::endl;
        prevstate = curstate;
    }
    curstate = StateLooking;
    if (prevstate == StateStopped && stop_save_time) {
        std::cerr << "accumulate " << (enterstatetime - stop_save_time + 1) << " sec" << std::endl;
        enterstatetime = now_time() - (enterstatetime - stop_save_time + 1);
        stop_save_time = 0;
    }
    else {
        enterstatetime = now_time();
    }
    gas = 0;
    turn = 0;
}

static void follow() {
    gas = MEDIUM_GAS;
    turn = 0;
    lost_time = 0;
    if (prevstate != curstate) {
        std::cerr << "follow()" << std::endl;
        prevstate = curstate;
        if (prevstate == StateStopped && stop_save_time) {
            std::cerr << "accumulate " << (enterstatetime - stop_save_time + 1) << " sec" << std::endl;
            enterstatetime = now_time() - (enterstatetime - stop_save_time + 1);
            stop_save_time = 0;
        }
        else {
            enterstatetime = now_time();
        }
    }
    curstate = StateFollowing;
}

static int turnstate = 0;
static double turnstatetime = 0;

static void turnaround() {
    gas = 0;
    turn = 0.25;
    lost_time = 0;
    if (curstate != prevstate) {
        std::cerr << "turnaround()" << std::endl;
        prevstate = curstate;
        if (prevstate == StateStopped && stop_save_time) {
            std::cerr << "accumulate " << (enterstatetime - stop_save_time + 1) << " sec" << std::endl;
            enterstatetime = now_time() - (enterstatetime - stop_save_time + 1);
            stop_save_time = 0;
        }
        else {
            enterstatetime = now_time();
        }
        turnstatetime = enterstatetime;
    }
    curstate = StateTurningAround;
    turnstate = 0;
}

static bool find_suitable(Area full, std::vector<ColorArea> const &vec, ColorArea &oa) {
    float coverage = 0;
    int half = full.bottom() / 2;
    bool ret = false;
    for (size_t i = 0, n = vec.size(); i != n; ++i) {
        ColorArea const &f(vec[i]);
        if (f.area.area() >= 100 && f.area.area() <= 20000 && f.weight > 0.5) {
            int top = f.area.top;
            int bottom = f.area.bottom();
            int left = f.area.left;
            int right = f.area.right();
            if (bottom <= half) {
                continue;
            }
            if (top < half) {
                top = half;
            }
            float fa = float(bottom - top) * float(right - left);
            if (fa > coverage) {
                oa = f;
                ret = true;
                coverage = fa;
            }
        }
    }
    return ret;
}

static void handle_areas(int state, Area total_area, std::vector<ColorArea> &yellow_areas, std::vector<ColorArea> &orange_areas) {
    ColorArea ay, ao;
    bool got_y = find_suitable(total_area, yellow_areas, ay);
    bool got_o = find_suitable(total_area, orange_areas, ao);
    //  move forward when yellow is left, orange is right
#if ALWAYS_FOLLOW
    bool y_left_of_o = true;
#else
    bool y_left_of_o = ay.area.left < ao.area.left;
#endif
    switch (state) {
        case StateLooking:
            gas = 0;
            turn = 0;
            if (got_y && got_o && y_left_of_o) {
                lastlock_orange = ao.area;
                lastlock_yellow = ay.area;
                follow();
            }
            else if (got_y && got_o && !y_left_of_o) {
                lastlock_orange = ao.area;
                lastlock_yellow = ay.area;
                std::cerr << "reverse detected" << std::endl;
                turnaround();
            }
            break;
        case StateFollowing:
            if (!got_y || !got_o) {
                //  lost at least one of them?
                gas = std::min(gas, LOW_GAS);
                if (!lost_time) {
                    lost_time = now_time();
                }
                else if (now_time() - lost_time > 1) {
                    std::cerr << "lost lock for 1 s ... looking" << std::endl;
                    looking();
                }
            }
            else {
                double area = std::max(ay.area.area(), ao.area.area());
                if (area > 10000) {
                    //  close
                    gas = LOW_GAS;
                }
                else if (area > 3000) {
                    gas = MEDIUM_GAS;
                }
                else {
                    //  full throttle to catch up!
                    gas = HIGH_GAS;
                }
                double cx = (ay.area.left + ay.area.right()) * ay.area.area() +
                    (ao.area.left + ao.area.right()) * ao.area.area();
                cx = cx / (ay.area.area() + ao.area.area());
                cx = cx * 0.5;
                if (cx < total_area.width * 0.45) {
                    turn = cx / total_area.width - 0.5;
                }
                else if (cx > total_area.width * 0.55) {
                    turn = cx / total_area.width - 0.5;
                }
                else {
                    turn = 0;
                }
                if (turn < -0.25) {
                    turn = -0.25;
                }
                if (turn > 0.25) {
                    turn = 0.25;
                }
            }
            break;
    }
}

double old_gas;
double old_steer;
int old_state;

double last_now_time;

void Brain::step() {
    double ntt = now_time();
    if (ntt - last_now_time < 0.015) {
        double tosleep = 0.015 - (ntt - last_now_time);
        if (tosleep < 0 || tosleep > 0.015) {
            tosleep = 0.015;
        }
        usleep((long)(tosleep * 1000000));
    }
    last_now_time = now_time();
    time_t t;
    time(&t);
    if (dirty_) {
        boost::shared_ptr<Image> rpic(rightCam_.prop_->get<boost::shared_ptr<Image>>());
#if USE_LEFT_CAM
        boost::shared_ptr<Image> lpic(leftCam_.prop_->get<boost::shared_ptr<Image>>());
#endif
        //  dump images every so often
        if (t != lastpic) {
            lastpic = t;
            ++npic;
            //  take a picture every 15 seconds -- every 5 for the first 2 minutes
            if (npic == (serial < 24 ? 5 : 15)) {
                npic = 0;
                serial += 1;
                {
                    if (!!rpic) {
                        char path[128];
                        sprintf(path, "%04d-right.jpg", serial);
                        async_file_dump(path, rpic->bits(CompressedBits), rpic->size(CompressedBits));
                    }
                }
#if USE_LEFT_CAM
                {
                    if (!!lpic) {
                        char path[128];
                        sprintf(path, "%04d-left.jpg", serial);
                        async_file_dump(path, lpic->bits(CompressedBits), lpic->size(CompressedBits));
                    }
                }
#endif
            }
        }
        if (should_look(curstate) && !!rpic) {
            Pixmap pm(rpic, true);  //  small image is enough
            std::vector<ColorArea> yellow_areas;
            std::vector<ColorArea> orange_areas;
            Area interest_yellow = focus_yellow(Area(0, 0, pm.width, pm.height));
            Area interest_orange = focus_orange(Area(0, 0, pm.width, pm.height));
            pm.find_areas_of_color(interest_yellow, yellow_color2, tolerance, normalization, pm.width*pm.height/10000 + 10, yellow_areas);
            pm.find_areas_of_color(interest_yellow, yellow_color, tolerance, normalization, pm.width*pm.height/10000 + 10, yellow_areas);
            pm.find_areas_of_color(interest_orange, orange_color2, tolerance, normalization, pm.width*pm.height/10000 + 10, orange_areas);
            pm.find_areas_of_color(interest_orange, orange_color, tolerance, normalization, pm.width*pm.height/10000 + 10, orange_areas);
            handle_areas(curstate, Area(0, 0, pm.width, pm.height), yellow_areas, orange_areas);
        }
    }
    //  decide on exploration
    if (!eAllow_.get()) {   //  fresh initialize or reset board
        eAllow_.prop_->edit(1L);
    }
    allowedProp_->set<long>(selfStop_.get() == 0);

    double nt = now_time();
    switch (curstate) {
    case StateStopped:
        if (nt - enterstatetime > 2 && enterstatetime != 0) {
            if (prevstate == StateFollowing) {
                looking();
            }
            else {
                std::cerr << "long stop; turnaround" << std::endl;
                turnaround();
            }
        }
        break;
    case StateLooking:
    /*
        if (nt - enterstatetime > 60) {
            std::cerr << "looking for 60 seconds; turnaround" << std::endl;
            turnaround();
        }
    */
        break;
    case StateFollowing:
        //  I can do this forever
        break;
    case StateTurningAround:
        //  have at it for 12 seconds
        if (nt - enterstatetime > 12) {
            looking();
        }
        else {
            switch (turnstate) {
            case 0:
                turn = 0.25;
                gas = MEDIUM_GAS;
                if (nt - turnstatetime > 1.5) {
                    ++turnstate;
                    turnstatetime = now_time();
                }
                break;
            case 1:
                turn = -0.25;
                gas = -MEDIUM_GAS;
                if (nt - turnstatetime > 1) {
                    ++turnstate;
                    turnstatetime = now_time();
                }
                break;
            case 2:
                turn = 0.25;
                gas = MEDIUM_GAS;
                if (nt - turnstatetime > 1.5) {
                    ++turnstate;
                    turnstatetime = now_time();
                }
                break;
            case 3:
                turn = -0.25;
                gas = -MEDIUM_GAS;
                if (nt - turnstatetime > 1) {
                    turnstate = 0;
                    turnstatetime = now_time();
                }
                break;
            }
        }
        break;
    }


    if (gas > 0) {
        if ((leftIr_.get() < irLimit_ && rightIr_.get() < irLimit_) || downIr_.get() > irLimit_) {
            //  there is some glitch that returns 0.3 for a sample or two at times.
            ++baddetect;
            if (baddetect >= 15) {
                std::cerr << "state " << curstate << " stop: leftIr_ " << leftIr_.get() << " rightIr_ " << rightIr_.get() << " downIr_ " << downIr_.get() << std::endl;
                stop();
            }
        }
/*
    Ping sensors basically don't work at speed?
    Or is there noise?
*/
        else {
            baddetect = 0;
        }
    }
    else if (gas < 0) {
        if (backUs_.get() < usLimit_) {
            ++baddetect;
            if (baddetect >= 20) {
                gas = gas * backUs_.get() / usLimit_;
                if (gas > -0.25) {
                    std::cerr << "state " << curstate << " stop: backUs_ " << backUs_.get() << std::endl;
                    stop();
                }
            }
        }
        else {
            baddetect = 0;
        }
    }
    else if (leftIr_.get() > irLimit_ || rightIr_.get() > irLimit_ || downIr_.get() < irLimit_ || backUs_.get() > usLimit_) {
        baddetect = 0;
    }

    if (selfStop_.get()) {
        statusProp_->set(std::string("Self Stopped"));
        //  some innocuous values
        stop();
    }
    else {
        statusProp_->set(std::string(state_name(curstate)));
    }
    speedProp_->set(gas);
    turnProp_->set(turn);
    cmdPower_.prop_->edit<double>(gas);
    cmdSteer_.prop_->edit<double>(turn);

    if (old_state != curstate || old_steer != turn || old_gas != gas) {
        std::cerr << state_name(curstate) << " turn " << turn << " gas " << gas << std::endl;
        old_state = curstate;
        old_steer = turn;
        old_gas = gas;
    }
}

void Brain::on_change() {
    dirty_ = true;
}


