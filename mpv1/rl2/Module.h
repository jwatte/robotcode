#if !defined(rl2_Module_h)
#define rl2_Module_h

#include "cast_as.h"
#include <string>
#include <boost/shared_ptr.hpp>

class Property;
class IReturn;

class Module : public cast_as_base {
public:
    virtual void step() = 0;
    virtual std::string const &name() = 0;
    virtual size_t num_properties() = 0;
    virtual boost::shared_ptr<Property> get_property_at(size_t ix) = 0;
    virtual boost::shared_ptr<Property> get_property_named(std::string const &str);
    virtual void set_return(boost::shared_ptr<IReturn> const &r) = 0;
};

class IReturn {
public:
    virtual void set_data(unsigned char offset, void const *data, unsigned char cnt) = 0;
    virtual void raw_cmd(void const *data, unsigned char cnt) = 0;
};

#endif  //  rl2_Module_h

