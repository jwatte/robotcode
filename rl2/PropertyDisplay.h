#if !defined(rl2_PropertyDisplay_h)
#define rl2_PropertyDisplay_h

#include "Listener.h"

#include <FL/Fl.H>
#include <FL/Fl_Group.H>

#include <boost/shared_ptr.hpp>

class Property;
class Image;
class Fl_Box;
class Fl_RGB_Image;
class Fl_Input;

class PropertyDisplay : public Fl_Group, public Listener {
public:
    static PropertyDisplay *create(boost::shared_ptr<Property> const &prop);
    ~PropertyDisplay();
    int width() { return w(); }
    int height() { return h(); }

    void on_click();

protected:
    PropertyDisplay(int x, int y, int w, int h, boost::shared_ptr<Property> const &prop);

    static void edit_tramp(Fl_Widget *, void *pd);
    void on_edit();
    void on_change();
    void update_image();

    boost::shared_ptr<Property> prop_;
    boost::shared_ptr<Listener> tramp_;
    Fl_Box *name_;
    Fl_Box *output_;
    Fl_Input *edit_;
    Fl_Box *box_;
    Fl_RGB_Image *flimage_;
    boost::shared_ptr<Image> image_;
    std::string value_;
};

#endif  //  rl2_PropertyDisplay_h

