
#include "BrowserWindow.h"
#include "ModuleList.h"
#include "PropertyWindow.h"
#include <boost/foreach.hpp>


BrowserWindow::BrowserWindow(boost::shared_ptr<ModuleList> const &modules) :
    Fl_Window(0, 0, 240, 480, "Modules"),
    modules_(modules) {
    begin();
    hold_ = new Fl_Hold_Browser(0, 0, 480, 480);
    end();
    hold_->callback(&BrowserWindow::select_callback, this);
    tramp_ = boost::shared_ptr<ListenerTramp>(new ListenerTramp(this));
    tramp_->attach(modules_);
    on_change();
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

