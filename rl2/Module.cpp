
#include "Module.h"
#include "Property.h"
#include <stdexcept>

boost::shared_ptr<Property> Module::get_property_named(std::string const &str) {
    for (size_t i = 0, n = num_properties(); i != n; ++i) {
        boost::shared_ptr<Property> prop(get_property_at(i));
        if (str == prop->name()) {
            return prop;
        }
    }
    throw std::runtime_error("Property " + str + " not found in " + name());
}

