
#include "ModuleList.h"
#include <algorithm>
#include <stdexcept>
#include <boost/foreach.hpp>

boost::shared_ptr<ModuleList> ModuleList::create() {
    return boost::shared_ptr<ModuleList>(new ModuleList());
}

void ModuleList::add(boost::shared_ptr<Module> m) {
    assert(std::find(modules_.begin(), modules_.end(), m) == modules_.end());
    modules_.push_back(m);
    on_change();
}

void ModuleList::remove(boost::shared_ptr<Module> m) {
    std::vector<boost::shared_ptr<Module>>::iterator ptr(
        std::find(modules_.begin(), modules_.end(), m));
    assert(ptr != modules_.end());
    std::swap(*ptr, *modules_.rbegin());
    modules_.pop_back();
    on_change();
}

size_t ModuleList::num_modules() {
    return modules_.size();
}

boost::shared_ptr<Module> ModuleList::get_module_at(size_t ix) {
    if (ix >= modules_.size()) {
        throw std::runtime_error("Index out of range in ModuleList::get_module_at()");
    }
    return modules_[ix];
}

bool ModuleList::contains(boost::shared_ptr<Module> m) {
    return std::find(modules_.begin(), modules_.end(), m) != modules_.end();
}

void ModuleList::step_all() {
    //  allow modifying the list while stepping
    std::vector<boost::shared_ptr<Module>> tmp(modules_);
    BOOST_FOREACH(auto mod, tmp) {
        mod->step();
    }
}


