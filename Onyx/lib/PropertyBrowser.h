#if !defined(rl2_PropertyBrowser_h)
#define rl2_PropertyBrowser_h

#include <FL/Fl_Group.H>

class PropertyBrowser : public Fl_Group {
public:
    PropertyBrowser(int x, int y, int w, int h) :
        Fl_Group(x, y, w, h) {
    }
protected:
};

#endif  //  rl2_PropertyBrowser_h
