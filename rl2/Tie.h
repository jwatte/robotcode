#if !defined(rl2_Tie_h)
#define rl2_Tie_h

#include "Module.h"
#include <map>
#include <string>

class Settings;
class ModuleList;
class TieBase;
class TieMaker;

class Tie : public cast_as_impl<Module, Tie> {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
    TieBase &base() const;
    void start(boost::shared_ptr<ModuleList> const &modules) const;
    static void register_class(std::string const &name, TieMaker *fac);
    virtual void step();
    virtual std::string const &name();
    virtual size_t num_properties();
    virtual boost::shared_ptr<Property> get_property_at(size_t ix);
    virtual void set_return(boost::shared_ptr<IReturn> const &r);
private:
    Tie(boost::shared_ptr<Settings> const &set);
    boost::shared_ptr<TieBase> base_;
protected:
    static std::map<std::string, TieMaker *> factories_;
};

class TieBase : public cast_as_impl<Module, TieBase> {
public:
    ~TieBase() {}
    virtual void start(boost::shared_ptr<ModuleList> const &modules) = 0;
protected:
    TieBase() {}
};

class TieMaker {
public:
    virtual boost::shared_ptr<TieBase> make(boost::shared_ptr<Settings> const &set) = 0;
};
template<typename T>
class TieReg : public TieMaker {
public:
    TieReg(char const *name) :
        name_(name) {
        Tie::register_class(name_, this);
    }
    boost::shared_ptr<TieBase> make(boost::shared_ptr<Settings> const &set) {
        return boost::shared_ptr<TieBase>(new T(name_, set));
    }
private:
    std::string name_;

};

#endif  //  rl2_Tie_h
