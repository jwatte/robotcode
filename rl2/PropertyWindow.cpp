
#include "PropertyWindow.h"
#include "PropertyBrowser.h"
#include "PropertyDisplay.h"
#include "Property.h"
#include "Module.h"


#include <FL/Fl_Window.H>

PropertyWindow::PropertyWindow(WindowOwner *owner, boost::shared_ptr<Module> mod) :
    cast_as_impl<OwnedWindow, PropertyWindow>(owner, new Fl_Window(486, 512, mod->name().c_str())),
    mod_(mod) {
    win()->begin();
    browser_ = new PropertyBrowser<info>(0, 0, 486, 512);
    build();
    win()->end();
}

void PropertyWindow::build() {
    int top = 0;
    for (size_t i = 0, n = mod_->num_properties(); i != n; ++i) {
        info j;
        j.prop = mod_->get_property_at(i);
        j.disp = boost::shared_ptr<PropertyDisplay>(PropertyDisplay::create(j.prop));
        j.disp->position(0, top);
        top += j.disp->h();
        browser_->add(j);
    }
}


int PropertyWindow::info::height() {
    assert(magic_ == 0xb00d3333);
    return disp->height();
}

int PropertyWindow::info::width() {
    assert(magic_ == 0xb00d3333);
    return disp->width();
}

void PropertyWindow::info::draw(int x, int y, int w, int h) {
    assert(magic_ == 0xb00d3333);
}

char const *PropertyWindow::info::text() {
    assert(magic_ == 0xb00d3333);
    return prop->name().c_str();
}

