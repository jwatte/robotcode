
#include "OwnedWindow.h"
#include "WindowOwner.h"
#include <FL/Fl_Window.H>


OwnedWindow::OwnedWindow(WindowOwner *owner, Fl_Window *win) :
    owner_(owner),
    win_(win) {
}

OwnedWindow::~OwnedWindow() {
    owner_->window_close(shared_from_this());
    delete win_;
}

void OwnedWindow::show() {
    win_->show();
}

