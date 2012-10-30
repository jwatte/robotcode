
#include "Tie.h"
#include "PropertyImpl.h"
#include "Settings.h"
#include "ModuleList.h"
#include "Boards.h"
#include "Image.h"
#include <sstream>
#include "str.h"


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
    Brain(std::string const &name, boost::shared_ptr<Settings> const &set);
    virtual void start(boost::shared_ptr<ModuleList> const &modules);
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
    PropRef<boost::shared_ptr<Image>>   leftCam_;
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
    force_(0.75),
    turning_(0.25),
    irLimit_(0.5),
    usLimit_(0.5),
    set_(set)
{
}

void Brain::start(boost::shared_ptr<ModuleList> const &modules) {
    leftCam_.setup(set_, "l_cam", "/dev/_Lcam.image", modules);
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
    leftCam_.prop_->add_listener(boost::shared_ptr<Listener>(new ListenerTramp(this)));
    rightCam_.prop_->add_listener(boost::shared_ptr<Listener>(new ListenerTramp(this)));
}

static int npic = 0;
static int serial = 0;
static time_t lastpic = 0;

void Brain::step() {
    time_t t;
    time(&t);
    if (dirty_) {
        //  analyze images
        if (t != lastpic) {
            lastpic = t;
            ++npic;
            if (npic == 5) {
                npic = 0;
                serial += 1;
                {
                    boost::shared_ptr<Image> rpic(rightCam_.prop_->get<boost::shared_ptr<Image>>());
                    if (!!rpic) {
                        char path[128];
                        sprintf(path, "%04d-right.jpg", serial);
                        //  todo: really shouldn't be doing this inline..
                        FILE *f = fopen(path, "wb");
                        size_t sz = rpic->size(CompressedBits);
                        void const *da = rpic->bits(CompressedBits);
                        fwrite(da, sz, 1, f);
                        fclose(f);
                        std::cerr << path << std::endl;
                    }
                }
                {
                    boost::shared_ptr<Image> lpic(leftCam_.prop_->get<boost::shared_ptr<Image>>());
                    if (!!lpic) {
                        char path[128];
                        sprintf(path, "%04d-left.jpg", serial);
                        //  todo: really shouldn't be doing this inline..
                        FILE *f = fopen(path, "wb");
                        size_t sz = lpic->size(CompressedBits);
                        void const *da = lpic->bits(CompressedBits);
                        fwrite(da, sz, 1, f);
                        fclose(f);
                        std::cerr << path << std::endl;
                    }
                }
            }
        }
    }
    //  decide on exploration
    if (!eAllow_.get()) {   //  fresh initialize or reset board
        eAllow_.prop_->edit(1L);
    }
    allowedProp_->set<long>(selfStop_.get() == 0);

    //  just some random movement
    double gas = 0;
    double turn = 0;
    switch ((t >> 1) & 7) {
        case 0: gas = 0; break;
        case 1: turn = turning_; gas = force_; break;
        case 2: gas = force_; break;
        case 3: turn = -turning_; gas = force_; break;
        case 4: gas = force_; break;
        case 5: gas = force_; break;
        case 6: turn = turning_; gas = 0; break;
        case 7: turn = turning_; gas = -force_; break;
    }

    if (gas > 0) {
        if (leftIr_.get() > irLimit_ || rightIr_.get() > irLimit_) {
            gas = 0;
        }
    }
    else if (gas < 0) {
        if (backUs_.get() < usLimit_) {
            gas *= backUs_.get();
            if (gas < 0.1) {
                gas = 0;
            }
        }
    }
    if (turn > 0) {
        if (rightUs_.get() < usLimit_) {
            gas = 0;
        }
    }
    else if (turn < 0) {
        if (leftUs_.get() < 0.5) {
            gas = 0;
        }
    }
    speedProp_->set(gas);
    turnProp_->set(turn);
    if (selfStop_.get()) {
        statusProp_->set(std::string("Stopped"));
        //  some innocuous values
        gas = 0;
        turn = 0.15;
    }
    else {
        statusProp_->set(std::string("Exploring"));
    }
    cmdPower_.prop_->edit<double>(gas);
    cmdSteer_.prop_->edit<double>(turn);
}

void Brain::on_change() {
    dirty_ = true;
}


