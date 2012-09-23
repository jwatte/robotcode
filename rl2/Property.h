#if !defined(rl2_Property_h)
#define rl2_Property_h

#include "Listenable.h"
#include "cast_as.h"
#include <typeinfo>

class Image;

enum PropertyType {
    TypeLong,       //  long
    TypeDouble,     //  double
    TypeString,     //  std::string
    TypeImage       //  boost::shared_ptr<Image>
};

template<typename T> class property_type;
class property_type_base;

class Property : public cast_as_impl<Listenable, Property> {
public:
    Property() {}
    virtual std::string const &name() = 0;
    virtual PropertyType type() = 0;
    template<typename T> T get() {
        char data[sizeof(T)];
        T *ret = new(data) T();
        get_value(property_type<T>::instance(), &data);
        return *ret;
    }
    template<typename T> void set(T const &v) {
        set_value(property_type<T>::instance(), &v);
    }
protected:
    virtual void get_value(property_type_base const &base, void *ref) = 0;
    virtual void set_value(property_type_base const &base, void const *t) = 0;
};



class property_type_base {
public:
    PropertyType get_type() const { return ptype_; }
protected:
    property_type_base(PropertyType pt) : ptype_(pt) {}
    PropertyType ptype_;
};

template<> class property_type<double> : public property_type_base {
public:
    typedef double proptype;
    typedef property_type<proptype> type;
    static type const &instance() {
        static type it;
        return it;
    }
    static inline proptype default_value() { return 0; }
protected:
    property_type() : property_type_base(TypeDouble) {}
};

template<> class property_type<long> : public property_type_base {
public:
    typedef long proptype;
    typedef property_type<proptype> type;
    static type const &instance() {
        static type it;
        return it;
    }
    static inline proptype default_value() { return 0; }
protected:
    property_type() : property_type_base(TypeLong) {}
};

template<> class property_type<std::string> : public property_type_base {
public:
    typedef std::string proptype;
    typedef property_type<proptype> type;
    static type const &instance() {
        static type it;
        return it;
    }
    static inline proptype default_value() { return proptype(); }
protected:
    property_type() : property_type_base(TypeString) {}
};

template<> class property_type<boost::shared_ptr<Image>> : public property_type_base {
public:
    typedef boost::shared_ptr<Image> proptype;
    typedef property_type<proptype> type;
    static type const &instance() {
        static type it;
        return it;
    }
    static inline proptype default_value() { return proptype(); }
protected:
    property_type() : property_type_base(TypeImage) {}
};

#endif  //  rl2_Property_h
