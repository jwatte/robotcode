#if !defined(rl2_Board_h)
#define rl2_Board_h

#include "Module.h"
#include <boost/shared_ptr.hpp>
#include <string>

class Settings;
class Property;
class PropUpdate;

class Board : public cast_as_impl<Module, Board> {
public:
    virtual unsigned char type();
    virtual void on_data(unsigned char const *data, unsigned char sz);
    virtual void get_data(unsigned char *o_data, unsigned char sz);
    virtual unsigned char data_size();
    virtual void step();
    virtual std::string const &name();
    virtual size_t num_properties();
    virtual boost::shared_ptr<Property> get_property_at(size_t ix);
protected:
    Board(std::string const &name, unsigned char dataSize, unsigned char type);
    size_t add_uchar_prop(std::string const &name, unsigned char offset, double scale);
    size_t add_schar_prop(std::string const &name, unsigned char offset, double scale);
    size_t add_sshort_prop(std::string const &name, unsigned char offset, double scale);
    std::vector<boost::shared_ptr<Property> > props_;
    std::vector<boost::shared_ptr<PropUpdate> > updates_;
    unsigned char type_;
    bool dirty_;
    std::string name_;
    std::vector<unsigned char> data_;
};

#endif  //  rl2_Board_h

