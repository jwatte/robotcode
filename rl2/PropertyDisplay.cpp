
#include "PropertyDisplay.h"
#include "Property.h"
#include "Listener.h"
#include "Image.h"

#include <stdexcept>

#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include <FL/Fl_Box.H>
#include <FL/Fl_RGB_Image.H>


PropertyDisplay *PropertyDisplay::create(boost::shared_ptr<Property> const &prop) {
    int h = 20;
    int w = 320;
    if (prop->type() == TypeImage) {
        h = 290;
        w = 480;
    }
    PropertyDisplay *ret = new PropertyDisplay(0, 0, w, h, prop);
    return ret;
}

PropertyDisplay::PropertyDisplay(int x, int y, int w, int h, 
    boost::shared_ptr<Property> const &prop) :
    Fl_Group(x, y, w, h, 0),
    prop_(prop),
    name_(0),
    output_(0),
    box_(0),
    flimage_(0) {
    begin();
    if (prop->type() == TypeImage) {
        name_ = new Fl_Box(x, y, w, 20);
        box_ = new Fl_Box(x, y + 20, w, h);
    }
    else {
        name_ = new Fl_Box(x, y, 120, 20);
        output_ = new Fl_Box(x + 120, y, w - 120, 20);
        output_->labelfont(FL_HELVETICA);
        output_->labelsize(12);
        output_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    }
    name_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    name_->labelfont(FL_HELVETICA + FL_BOLD);
    name_->labelsize(12);
    name_->label(prop->name().c_str());
    end();
    tramp_ = boost::shared_ptr<Listener>(static_cast<Listener *>(new ListenerTramp(this)));
    prop_->add_listener(tramp_);
    on_change();
}

PropertyDisplay::~PropertyDisplay() {
    static_cast<ListenerTramp &>(*tramp_).detach();
    delete flimage_;
}

void PropertyDisplay::on_change() {
    switch (prop_->type()) {
        case TypeLong:
            value_ = boost::lexical_cast<std::string>(prop_->get<long>());
            break;
        case TypeDouble:
            value_ = boost::lexical_cast<std::string>(prop_->get<double>());
            break;
        case TypeString:
            value_ = prop_->get<std::string>();
            break;
        case TypeImage:
            update_image();
            break;
        default:
            throw std::runtime_error("Unknown property type in PropertyDisplay::on_change()");
    }
    if (output_ != NULL) {
        output_->label(value_.c_str());
        output_->damage(0xff);
    }
}

void PropertyDisplay::update_image() {
    image_ = prop_->get<boost::shared_ptr<Image>>();
    delete flimage_;
    if (!!image_) {
        flimage_ = new Fl_RGB_Image(
            (unsigned char const *)image_->bits(ThumbnailBits),
            image_->width_t(),
            image_->height_t(),
            Image::BytesPerPixel);
    }
    else {
        flimage_ = new Fl_RGB_Image(
            (unsigned char const *)"~~~",
            1,
            1,
            Image::BytesPerPixel);
    }
    box_->image(flimage_);
}


