#include "Tie.h"
#include "PropertyImpl.h"
#include "Settings.h"
#include "ModuleList.h"
#include "Boards.h"
#include "str.h"


static std::string str_r_text("r_text");

class FormatterBase {
public:
    virtual void format(boost::shared_ptr<Property> const &prop, std::string &out) = 0;
    virtual ~FormatterBase() {}
};

class DoubleFormatter : public FormatterBase {
public:
    DoubleFormatter(std::string const &fmt) :
        fmt_(fmt) {
    }
    void format(boost::shared_ptr<Property> const &prop, std::string &out) {
        char buf[100];
        snprintf(buf, 100, fmt_.c_str(), prop->get<double>());
        buf[99] = 0;
        out = buf;
    }

    std::string fmt_;
};

class TieDisplay : public TieBase, public ListenableImpl<Listenable> {
public:
    TieDisplay(std::string const &name, boost::shared_ptr<Settings> const &set);
    void start(boost::shared_ptr<ModuleList> const &modules);
    virtual void step();
    void on_change();
    virtual std::string const &name() {
        return name_;
    }
    virtual size_t num_properties() {
        return 1;
    }
    virtual boost::shared_ptr<Property> get_property_at(size_t ix) {
        if (ix != 0) {
            throw std::runtime_error("get_property_at() out of range in TieDisplay()");
        }
        return textProp_;
    }
    virtual void set_return(boost::shared_ptr<IReturn> const &r) {
    }

    std::string name_;
    boost::shared_ptr<FormatterBase> formatter_;
    boost::shared_ptr<PropertyImpl<std::string>> textProp_;
    boost::shared_ptr<Property> prop_;
    std::string path_;
    long x_;
    long y_;
    long w_;
    bool dirty_;
};

TieReg<TieDisplay> regTieDisplay("TieDisplay");


TieDisplay::TieDisplay(std::string const &name, boost::shared_ptr<Settings> const &set) :
    name_(name),
    textProp_(new PropertyImpl<std::string>(str_r_text)),
    x_(0),
    y_(0),
    w_(0),
    dirty_(true) {

    std::string val;
    if (maybe_get(set, "double", val)) {
        formatter_ = boost::shared_ptr<FormatterBase>(new DoubleFormatter(val));
    }
    else {
        throw std::runtime_error("Can't find an acceptable formatter for TieDisplay " + name);
    }

    if (!maybe_get(set, "path", path_)) {
        throw std::runtime_error("No path defined for TieDisplay " + name);
    }
    if (!maybe_get(set, "x", x_)) {
        throw std::runtime_error("No X coordinate defined for TieDisplay " + name);
    }
    if (!maybe_get(set, "y", y_)) {
        throw std::runtime_error("No Y coordinate defined for TieDisplay " + name);
    }
    if (!maybe_get(set, "w", w_)) {
        throw std::runtime_error("No width defined for TieDisplay " + name);
    }
    maybe_get(set, "title", name_);
}

void TieDisplay::start(boost::shared_ptr<ModuleList> const &modules) {
    std::vector<std::string> path;
    split(path_, '.', path);
    if (path.size() != 2) {
        throw std::runtime_error("Bad path format '" + path_ + "' in TieDisplay " + name_);
    }
    auto mod = modules->get_module_named(path[0]);
    prop_ = mod->get_property_named(path[1]);
    prop_->add_listener(boost::shared_ptr<Listener>(new ListenerTramp(this)));
}

void TieDisplay::step() {
    if (dirty_) {
        std::string out;
        formatter_->format(prop_, out);
        textProp_->set(out);
        gDisplayBoard->cast_as<DisplayBoard>()->fill_rect(x_, y_, x_ + w_, y_ + 18, 0);
        gDisplayBoard->cast_as<DisplayBoard>()->draw_text(out, x_, y_, 0xffff, 0x0000);
    }
}

void TieDisplay::on_change() {
    dirty_ = true;
}

