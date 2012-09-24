#if !defined(rl2_Module_h)
#define rl2_Module_h

#include "cast_as.h"
#include <string>
#include <boost/shared_ptr.hpp>

class Property;

class Module : public cast_as_base {
public:
    virtual void step() = 0;
    virtual std::string const &name() = 0;
    virtual size_t num_properties() = 0;
    virtual boost::shared_ptr<Property> get_property_at(size_t ix) = 0;
    virtual boost::shared_ptr<Property> get_property_named(std::string const &str);
};

#endif  //  rl2_Module_h

