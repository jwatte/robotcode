
#include "OwnedWindow.h"
#include "WindowOwner.h"
#include <FL/Fl_Window.H>
#include <iostream>


static void close_cb(Fl_Widget *win, void *ow) {
    OwnedWindow *owned = reinterpret_cast<OwnedWindow *>(ow);
    owned->release();
}

OwnedWindow::OwnedWindow(WindowOwner *owner, Fl_Window *win) :
    owner_(owner),
    win_(win) {
    win_->callback(close_cb, this);
    std::cerr << "OwnedWindow created: " << win->label() << std::endl;
}

OwnedWindow::~OwnedWindow() {
    std::cerr << "OwnedWindow destroyed: " << win_->label() << std::endl;
    delete win_;
}

void OwnedWindow::release() {
    owner_->window_close(shared_from_this());
}

void OwnedWindow::show() {
    win_->show();
}

