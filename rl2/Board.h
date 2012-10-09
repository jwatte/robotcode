#if !defined(rl2_Board_h)
#define rl2_Board_h

#include "Module.h"
#include <boost/shared_ptr.hpp>
#include <string>

class Settings;
class Property;
class PropUpdate;

template<typename T> class Translator;

template<>
class Translator<double> {
public:
    //  src is the src data (uchar or sshort)
    virtual double translate(void const *src) = 0;
    typedef double type_t;
};

template<>
class Translator<long> {
public:
    //  src is the src data (uchar or sshort)
    virtual long translate(void const *src) = 0;
    typedef long type_t;
};


class IReturn {
public:
    virtual void set_data(unsigned char offset, void const *data, unsigned char cnt) = 0;
};

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
    virtual void set_return(IReturn *ret);
protected:
    Board(std::string const &name, unsigned char dataSize, unsigned char type);
    size_t add_uchar_prop(std::string const &name, unsigned char offset, double scale, bool editable = false);
    size_t add_schar_prop(std::string const &name, unsigned char offset, double scale, bool editable = false);
    size_t add_sshort_prop(std::string const &name, unsigned char offset, double scale, bool editable = false);
    //  For now, custom translated properties are not editable.
    //  Could fix this by having the reverse translation be passed 
    //  in if needed.
    size_t add_prop(std::string const &name, unsigned char offset, boost::shared_ptr<Translator<double>> xlat);
    size_t add_prop(std::string const &name, unsigned char offset, boost::shared_ptr<Translator<long>> xlat);
    std::vector<boost::shared_ptr<Property> > props_;
    std::vector<boost::shared_ptr<PropUpdate> > updates_;
    unsigned char type_;
    bool dirty_;
    std::string name_;
    std::vector<unsigned char> data_;
    IReturn *return_;
};

#endif  //  rl2_Board_h

