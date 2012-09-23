
#include "PropertyWindow.h"
#include "PropertyBrowser.h"
#include "PropertyDisplay.h"
#include "Property.h"
#include "Module.h"


#include <FL/Fl_Double_Window.H>


static int nxpos = 250;
static int nypos = 0;

static int next_x() {
    return nxpos;
}

static int next_y() {
    return nypos;
}

static void update_next() {
    nxpos += 500;
    if (nxpos >= 900) {
        nxpos -= 900;
        nypos += 500;
        if (nypos >= 700) {
            nypos -= 700;
        }
    }
}


PropertyWindow::PropertyWindow(WindowOwner *owner, boost::shared_ptr<Module> mod) :
    cast_as_impl<OwnedWindow, PropertyWindow>(owner,
        new Fl_Double_Window(next_x(), next_y(), 486, 384, mod->name().c_str())),
    mod_(mod) {
    update_next();
    win()->begin();
    browser_ = new PropertyBrowser(0, 0, 486, 512);
    build();
    win()->end();
}

void PropertyWindow::build() {
    int top = 0;
    browser_->begin();
    for (size_t i = 0, n = mod_->num_properties(); i != n; ++i) {
        boost::shared_ptr<Property> prop(mod_->get_property_at(i));
        PropertyDisplay *disp = PropertyDisplay::create(prop);
        disp->position(3, top);
        top += disp->h();
    }
    browser_->end();
}



