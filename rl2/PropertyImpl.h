#if !defined(rl2_PropertyImpl_h)
#define rl2_PropertyImpl_h

#include "Property.h"
#include "ListenableImpl.h"
#include <stdexcept>

template<typename T>
class PropertyImpl : public cast_as_impl<ListenableImpl<Property>, PropertyImpl<T>> {
public:
    PropertyImpl(std::string const &name, 
        boost::shared_ptr<Listener> const &editable = boost::shared_ptr<Listener>()) :
        name_(name),
        value_(property_type<T>::default_value()),
        editable_(editable) {
    }
    PropertyType type() {
        return property_type<T>::instance().get_type();
    }
    std::string const &name() {
        return name_;
    }
    bool editable() {
        return editable_ != 0;
    }
    void get_value(property_type_base const &base, void *ref) {
        if (base.get_type() != property_type<T>::instance().get_type()) {
            throw std::runtime_error("Wrong property type asked for for '"
                + name_ + "' in PropertyImpl::get_value()");
        }
        *(T *)ref = value_;
    }
    void set_value(property_type_base const &base, void const *t, bool is_edit) {
        if (base.get_type() != property_type<T>::instance().get_type()) {
            throw std::runtime_error("Invalid type in PropertyImpl::set_value() "
                "for '" + name_ + "'");
        }
        if (!base.equal(value_, *(T *)t)) {
            value_ = *(T *)t;
            if (editable_) {
                editable_->on_change();
            }
            this->on_change();
        }
    }
protected:
    std::string const &name_;
    T value_;
    boost::shared_ptr<Listener> editable_;
};

#endif  //  rl2_PropertyImpl_h
