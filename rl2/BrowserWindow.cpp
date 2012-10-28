
#include "BrowserWindow.h"
#include "ModuleList.h"
#include "PropertyWindow.h"
#include "Property.h"
#include "GraphWidget.h"
#include "Settings.h"
#include <algorithm>
#include <iostream>
#include <boost/foreach.hpp>
#include <FL/Fl_Fill_Dial.H>


template<typename T>
class PropertyListenerTramp : public Listener {
public:
    PropertyListenerTramp(T min, T max, Fl_Valuator *w, GraphWidget *gw) :
        min_(min),
        max_(max),
        w_(w),
        gw_(gw) {
        w_->minimum((double)min);
        w_->maximum((double)max);
        if (gw_) {
            gw_->range((double)min, (double)max);
        }
    }
    void attach(boost::shared_ptr<Property> const &prop) {
        prop_ = prop;
        prop->add_listener(Listener::shared_from_this());
        on_change();
    }
    void on_change() {
        T t(prop_->get<T>());
        update(t);
    }
    void update(T t) {
        double tv = (double)std::max(min_, std::min(max_, t));
        w_->value(tv);
        w_->damage(0xff);
        if (gw_) {
            gw_->value(tv);
        }
    }

    boost::shared_ptr<Property> prop_;
    T min_;
    T max_;
    Fl_Valuator *w_;
    GraphWidget *gw_;
};

void quit_program(Fl_Widget *) {
    if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) {
        return; //  don't quit on Escape
    }
    std::cerr << "quit_program() received" << std::endl;
    exit(0);
}

BrowserWindow::BrowserWindow(boost::shared_ptr<ModuleList> const &modules,
    boost::shared_ptr<Settings> const &set) :
    Fl_Double_Window(0, 0, 480, 480, "Modules"),
    modules_(modules) {
    begin();
    hold_ = new Fl_Hold_Browser(0, 0, 160, 240);
    cbatt_ = new Fl_Fill_Dial(170, 10, 140, 140, "Comp Batt");
    cbatt_->angle1(0);
    cbatt_->angle2(360);
    cbatt_->color(0xa0202000, 0x20a040ff);
    long granule = 10;
    maybe_get(set, "graph_s", granule);
    cgraph_ = new GraphWidget(170, 180, 140, 60);
    cgraph_->granule(granule);
    mbatt_ = new Fl_Fill_Dial(330, 10, 140, 140, "Loco Batt");
    mbatt_->angle1(0);
    mbatt_->angle2(360);
    mbatt_->color(0xa0202000, 0x20a040ff);
    mgraph_ = new GraphWidget(330, 180, 140, 60);
    mgraph_->granule(granule);
    end();
    hold_->callback(&BrowserWindow::select_callback, this);
    tramp_ = boost::shared_ptr<ListenerTramp>(new ListenerTramp(this));
    tramp_->attach(modules_);
    on_change();
    (boost::shared_ptr<PropertyListenerTramp<double>>(
        new PropertyListenerTramp<double>(10.5, 15.0, cbatt_, cgraph_)))->attach(
        modules->get_module_named("USB board")->get_property_named("r_voltage"));
    (boost::shared_ptr<PropertyListenerTramp<double>>(
        new PropertyListenerTramp<double>(6.5, 8.5, mbatt_, mgraph_)))->attach(
        modules->get_module_named("Motor board")->get_property_named("r_voltage"));
    Fl::add_timeout(1, &BrowserWindow::on_timeout, this);
    callback(&quit_program);
}

void BrowserWindow::on_timeout(void *b) {
    BrowserWindow *bw = reinterpret_cast<BrowserWindow *>(b);
    bw->timeout();
    Fl::add_timeout(1, &BrowserWindow::on_timeout, bw);
}

void BrowserWindow::timeout() {
    cgraph_->scroll();
    mgraph_->scroll();
}

BrowserWindow::~BrowserWindow() {
    tramp_->detach();
}

void BrowserWindow::on_change() {
    while (hold_->size()) {
        hold_->remove(hold_->size());
    }
    for (size_t i = 0, n = modules_->num_modules(); i != n; ++i) {
        hold_->add(modules_->get_module_at(i)->name().c_str(), (void *)i);
    }
    hold_->damage(0xff);
    sync_windows();
}

void BrowserWindow::select_callback(Fl_Widget *, void *arg) {
    reinterpret_cast<BrowserWindow *>(arg)->click();
}

void BrowserWindow::click() {
    int line = hold_->value();
    if (line > 0) {
        open_window(modules_->get_module_at(line-1));
    }
}

void BrowserWindow::sync_windows() {
    std::vector<boost::shared_ptr<Module>> toclose;
    for (itertype ptr(windows_.begin()), end(windows_.end());
        ptr != end; ++ptr) {
        if (!modules_->contains((*ptr).first)) {
            toclose.push_back((*ptr).first);
        }
    }
    BOOST_FOREACH(auto win, toclose) {
        windows_.erase(windows_.find(win));
    }
}

void BrowserWindow::window_close(boost::shared_ptr<OwnedWindow> w) {
    for (itertype ptr(windows_.begin()), end(windows_.end());
        ptr != end; ++ptr) {
        if ((*ptr).second == w) {
            windows_.erase(ptr);
            return;
        }
    }
}

void BrowserWindow::open_window(boost::shared_ptr<Module> mod) {
    itertype ptr(windows_.find(mod));
    if (ptr != windows_.end()) {
        (*ptr).second->show();
        return;
    }
    boost::shared_ptr<PropertyWindow> win(new PropertyWindow(this, mod));
    windows_[mod] = win;
    win->show();
}

