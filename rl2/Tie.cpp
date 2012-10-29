
#include "Tie.h"
#include "Settings.h"
#include "ModuleList.h"

#include <stdexcept>

static std::map<std::string, TieMaker *> &factories() {
    static std::map<std::string, TieMaker *> it;
    return it;
}

boost::shared_ptr<Module> Tie::open(boost::shared_ptr<Settings> const &set) {
    return boost::shared_ptr<Module>(new Tie(set));
}

TieBase &Tie::base() const {
    return *base_;
}

void Tie::start(boost::shared_ptr<ModuleList> const &modules) const {
    base_->start(modules);
}

void Tie::register_class(std::string const &name, TieMaker *fac) {
    factories()[name] = fac;
}

Tie::Tie(boost::shared_ptr<Settings> const &set) {
    std::string className(set->get_value("class")->get_string());
    auto ptr(factories().find(className));
    if (ptr == factories().end()) {
        throw std::runtime_error("No tie instance of class " + className + " found.");
    }
    base_ = (*ptr).second->make(set);
}

void Tie::step() {
    base_->step();
}

std::string const &Tie::name() {
    return base_->name();
}

size_t Tie::num_properties() {
    return base_->num_properties();
}

boost::shared_ptr<Property> Tie::get_property_at(size_t ix) {
    return base_->get_property_at(ix);
}

void Tie::set_return(boost::shared_ptr<IReturn> const &r) {
    base_->set_return(r);
}


