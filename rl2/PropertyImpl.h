#if !defined(rl2_PropertyImpl_h)
#define rl2_PropertyImpl_h

#include "Property.h"
#include "ListenableImpl.h"
#include <stdexcept>

template<typename T>
class PropertyImpl : public cast_as_impl<ListenableImpl<Property>, PropertyImpl<T>> {
public:
    PropertyImpl(std::string const &name) :
        name_(name) {
    }
    PropertyType type() {
        return property_type<T>::instance().get_type();
    }
    std::string const &name() {
        return name_;
    }
    void get_value(property_type_base const &base, void *ref) {
        if (base.get_type() != property_type<T>::instance().get_type()) {
            throw std::runtime_error("Wrong property type asked for for '"
                + name_ + "' in PropertyImpl::get_value()");
        }
        *(T *)ref = value_;
    }
    void set_value(property_type_base const &base, void const *t) {
        if (base.get_type() != property_type<T>::instance().get_type()) {
            throw std::runtime_error("Invalid type in PropertyImpl::set_value() "
                "for '" + name_ + "'");
        }
        value_ = *(T *)t;
        this->on_change();
    }
protected:
    std::string const &name_;
    T value_;
};

#endif  //  rl2_PropertyImpl_h
