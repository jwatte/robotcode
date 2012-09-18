#if !defined(rl2_OwnedWindow_h)
#define rl2_OwnedWindow_h

#include "cast_as.h"
#include <boost/enable_shared_from_this.hpp>

class WindowOwner;
class Fl_Window;

class OwnedWindow : public boost::enable_shared_from_this<OwnedWindow>, public cast_as_base {
public:
    OwnedWindow(WindowOwner *owner, Fl_Window *win);
    ~OwnedWindow();

    virtual void release();
    virtual void show();

    Fl_Window *win() { return win_; }
    WindowOwner *owner() { return owner_; }

private:
    WindowOwner *owner_;
    Fl_Window *win_;
};

#endif  //  rl2_OwnedWindow_h
