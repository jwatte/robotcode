
#include "PropertyDisplay.h"
#include "Property.h"
#include "Listener.h"
#include "Image.h"

#include <stdexcept>

#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include <FL/Fl_Box.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Input.H>


class ClickableBox : public Fl_Box {
public:
    ClickableBox(PropertyDisplay *owner, int x, int y, int w, int h, char const *l = 0) :
        Fl_Box(x, y, w, h, l),
        owner_(owner) {
    }
    int handle(int event) {
        if (Fl_Box::handle(event)) {
            return 1;
        }
        if (event == 1) {   //  mouse down
            owner_->on_click();
            return 1;
        }
        return 0;
    }
    PropertyDisplay *owner_;
};

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
    edit_(0),
    box_(0),
    flimage_(0) {
    begin();
    if (prop->type() == TypeImage) {
        name_ = new Fl_Box(x, y, w, 15);
        box_ = new Fl_Box(x, y + 10, w, h);
        box_->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    }
    else {
        name_ = new Fl_Box(x, y, 120, 20);
        output_ = new ClickableBox(this, x + 120, y, w - 120, 20);
        output_->labelfont(FL_HELVETICA);
        output_->labelsize(12);
        output_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        if (prop->editable()) {
            edit_ = new Fl_Input(x + 120, y, w - 120, 20);
            edit_->hide();
            edit_->when(FL_WHEN_RELEASE_ALWAYS | FL_WHEN_ENTER_KEY_ALWAYS);
            edit_->callback(&PropertyDisplay::edit_tramp, this);
        }
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

void PropertyDisplay::edit_tramp(Fl_Widget *, void *d) {
    reinterpret_cast<PropertyDisplay *>(d)->on_edit();
}

void PropertyDisplay::on_click() {
    if (edit_ != 0) {
        output_->hide();
        edit_->value(value_.c_str());
        edit_->show();
    }
}

int PropertyDisplay::handle(int event) {
    if ((event == FL_KEYDOWN || event == FL_SHORTCUT) && Fl::event_key() == FL_Escape) {
        if (edit_ != 0) {
            edit_->value(value_.c_str());
            edit_->hide();
            output_->show();
            output_->label(value_.c_str());
            output_->redraw();
            return 1;
        }
    }
    return Fl_Group::handle(event);
}

static bool set_prop_value(boost::shared_ptr<Property> const &prop, std::string const &ival, std::string &oerr) {
    try {
        switch (prop->type()) {
            case TypeLong:
                prop->edit<long>(boost::lexical_cast<long>(ival));
                break;
            case TypeDouble:
                prop->edit<double>(boost::lexical_cast<double>(ival));
                break;
            case TypeString:
                prop->edit<std::string>(ival);
                break;
            default:
                throw std::runtime_error("Unknown property type in PropertyDisplay::on_edit()");
        }
    }
    catch (std::exception const &x) {
        oerr = x.what();
        return false;
    }
    return true;
}

void PropertyDisplay::on_edit() {
    edit_->hide();
    output_->label("");
    output_->show();
    std::string val(edit_->value()), err;
    if (!set_prop_value(prop_, val, err)) {
        std::cerr << "Property value error: " << err << std::endl;
        output_->label(value_.c_str());
    }
    output_->redraw();
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
    box_->damage(0xff);
}


