#if !defined(rl2_BrowserWindow_h)
#define rl2_BrowserWindow_h

#include "Listener.h"
#include "WindowOwner.h"
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Dial.H>
#include <boost/shared_ptr.hpp>
#include <map>

class ModuleList;
class Module;
class GraphWidget;

class BrowserWindow : public Fl_Double_Window, public Listener, public WindowOwner {
public:
    BrowserWindow(boost::shared_ptr<ModuleList> const &modules);
    ~BrowserWindow();

    void window_close(boost::shared_ptr<OwnedWindow> w);

protected:
    boost::shared_ptr<ModuleList> modules_;
    boost::shared_ptr<ListenerTramp> tramp_;
    std::map<boost::shared_ptr<Module>, boost::shared_ptr<OwnedWindow>> windows_;
    typedef std::map<boost::shared_ptr<Module>, boost::shared_ptr<OwnedWindow>>::iterator itertype;

    Fl_Hold_Browser *hold_;
    Fl_Dial *cbatt_;
    Fl_Dial *mbatt_;
    GraphWidget *cgraph_;
    GraphWidget *mgraph_;
    
    void on_change();
    static void select_callback(Fl_Widget *, void *);
    void click();
    void open_window(boost::shared_ptr<Module> mod);
    void sync_windows();

    static void on_timeout(void *);
    void timeout();
};

#endif  //  rl2_BrowserWindow_h

