#include "Board.h"
#include "PropertyImpl.h"
#include <boost/foreach.hpp>


class PropUpdate : public Listener {
public:
    virtual void update() = 0;
    virtual void set_prop(boost::shared_ptr<Property> const &pr) = 0;
};

template<typename BaseType, typename PropType>
class PropUpdateT : public PropUpdate {
public:
    PropUpdateT(boost::shared_ptr<Property> const &prop, void const *src, 
        boost::shared_ptr<Translator<PropType>> xlat) :
        prop_(prop),
        src_(src),
        xlat_(xlat) {
    }
    boost::shared_ptr<Property> prop_;
    void const *src_;
    boost::shared_ptr<Translator<PropType>> xlat_;

    void update() {
        BaseType bt;
        memcpy(&bt, src_, sizeof(bt));
        PropType pt = xlat_->translate(&bt);
        prop_->set<PropType>(pt);
    }

    virtual void set_prop(boost::shared_ptr<Property> const &pr) {
        throw std::runtime_error("Should not set_prop() on a Translate property");
    }
    void on_change() {
        throw std::runtime_error("Should not edit a Translate property");
    }
};

template<typename BaseType, typename PropType>
class PropUpdateImpl : public PropUpdate {
public:
    PropUpdateImpl(Board *brd, void const *src,
        double scale) :
        brd_(brd),
        src_(src),
        scale_(scale) {
    }
    Board *brd_;
    boost::shared_ptr<Property> prop_;
    void const *src_;
    double scale_;

    void set_prop(boost::shared_ptr<Property> const &prop) {
        prop_ = prop;
    }

    void update() {
        BaseType bt;
        memcpy(&bt, src_, sizeof(bt));
        PropType pt;
        if (scale_ != 0) {
            pt = (PropType)(bt * scale_);
        }
        else {
            pt = (PropType)bt;
        }
        prop_->set<PropType>(pt);
    }

    //  edit
    void on_change() {
        PropType pt(prop_->get<PropType>());
        BaseType bt(static_cast<BaseType>(pt / (scale_ != 0 ? scale_ : 1.0) + 0.5));
        brd_->edit(src_, &bt, sizeof(bt));
    }
};


unsigned char Board::type() {
    return type_;
}

void Board::on_data(unsigned char const *data, unsigned char sz) {
    if (sz != data_.size()) {
        std::cerr << "Bad update size " << (int)sz << " for board " << name_ << 
            " (expected " << data_.size() << ")" << std::endl;
        std::cerr << "  ";
        for (size_t i = 0; i < sz; ++i) {
            std::cerr << std::hex << " " << (int)data[i];
        }
        std::cerr << std::endl;
        return;
    }
    //  Only invalidate if data is actually different
    if (memcmp(&data_[0], data, sz)) {
        memcpy(&data_[0], data, sz);
        dirty_ = true;
    }
}

void Board::get_data(unsigned char *o_data, unsigned char sz) {
    if (sz != data_.size()) {
        throw std::runtime_error("Bad size in Board::get_data(): " + name_);
    }
    memcpy(o_data, &data_[0], sz);
}

unsigned char Board::data_size() {
    return data_.size();
}

void Board::step() {
    if (dirty_) {
        dirty_ = false;
        BOOST_FOREACH(auto ptr, updates_) {
            ptr->update();
        }
    }
}

std::string const &Board::name() {
    return name_;
}

size_t Board::num_properties() {
    return props_.size();
}

boost::shared_ptr<Property> Board::get_property_at(size_t ix) {
    if (ix >= sizeof(props_)) {
        throw std::runtime_error("Index out of range in Board::get_property_at: " + name_);
    }
    return props_[ix];
}

void Board::set_return(boost::shared_ptr<IReturn> const &r) {
    return_ = r;
}

void Board::edit(void const *dst, void const *src, size_t sz) {
    size_t offset = (char const *)dst - (char const *)&data_[0];
    assert(sz <= 4);
    assert(offset + sz <= data_.size());
    return_->set_data(offset, src, (unsigned char)sz);
}

Board::Board(std::string const &name, unsigned char dataSize, unsigned char type) :
    type_(type),
    dirty_(false),
    name_(name) {
    data_.resize(dataSize);
}

size_t Board::add_uchar_prop(std::string const &name, unsigned char offset, double scale, bool editable) {
    assert(offset < data_.size());
    if (scale == 0) {
        boost::shared_ptr<PropUpdate> pu(new PropUpdateImpl<unsigned char, long>(this, &data_[offset], 0));
        boost::shared_ptr<Property> prop(new PropertyImpl<long>(name, pu));
        pu->set_prop(prop);
        props_.push_back(prop);
        updates_.push_back(pu);
    }
    else {
        boost::shared_ptr<PropUpdate> pu(new PropUpdateImpl<unsigned char, double>(this, &data_[offset], scale));
        boost::shared_ptr<Property> prop(new PropertyImpl<double>(name, pu));
        pu->set_prop(prop);
        props_.push_back(prop);
        updates_.push_back(pu);
    }
    return props_.size() - 1;
}

size_t Board::add_schar_prop(std::string const &name, unsigned char offset, double scale, bool editable) {
    assert(offset < data_.size());
    if (scale == 0) {
        boost::shared_ptr<PropUpdate> pu(new PropUpdateImpl<char, long>(this, &data_[offset], 0));
        boost::shared_ptr<Property> prop(new PropertyImpl<long>(name, pu));
        pu->set_prop(prop);
        props_.push_back(prop);
        updates_.push_back(pu);
    }
    else {
        boost::shared_ptr<PropUpdate> pu(new PropUpdateImpl<char, double>(this, &data_[offset], scale));
        boost::shared_ptr<Property> prop(new PropertyImpl<double>(name, pu));
        pu->set_prop(prop);
        props_.push_back(prop);
        updates_.push_back(pu);
    }
    return props_.size() - 1;
}

size_t Board::add_sshort_prop(std::string const &name, unsigned char offset, double scale, bool editable) {
    assert(offset < data_.size());
    if (scale == 0) {
        boost::shared_ptr<PropUpdate> pu(new PropUpdateImpl<short, long>(this, &data_[offset], 0));
        boost::shared_ptr<Property> prop(new PropertyImpl<long>(name, pu));
        pu->set_prop(prop);
        props_.push_back(prop);
        updates_.push_back(pu);
    }
    else {
        boost::shared_ptr<PropUpdate> pu(new PropUpdateImpl<short, double>(this, &data_[offset], scale));
        boost::shared_ptr<Property> prop(new PropertyImpl<double>(name, pu));
        pu->set_prop(prop);
        props_.push_back(prop);
        updates_.push_back(pu);
    }
    return props_.size() - 1;
}

size_t Board::add_prop(std::string const &name, unsigned char offset, boost::shared_ptr<Translator<double>> xlat) {
    assert(offset < data_.size());
    boost::shared_ptr<Property> prop(new PropertyImpl<double>(name));
    props_.push_back(prop);
    boost::shared_ptr<PropUpdate> pu(new PropUpdateT<short, double>(
        prop, &data_[offset], xlat));
    updates_.push_back(pu);
    return props_.size() - 1;
}

size_t Board::add_prop(std::string const &name, unsigned char offset, boost::shared_ptr<Translator<long>> xlat) {
    assert(offset < data_.size());
    boost::shared_ptr<Property> prop(new PropertyImpl<long>(name));
    props_.push_back(prop);
    boost::shared_ptr<PropUpdate> pu(new PropUpdateT<short, long>(
        prop, &data_[offset], xlat));
    updates_.push_back(pu);
    return props_.size() - 1;
}


