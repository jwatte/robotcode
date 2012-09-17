#if !defined(rl2_PropertyWindow_h)
#define rl2_PropertyWindow_h

#include "OwnedWindow.h"
#include "PropertyBrowser.h"
class Module;
class Property;
class PropertyDisplay;

class PropertyWindow : public cast_as_impl<OwnedWindow, PropertyWindow> {
public:
    PropertyWindow(WindowOwner *owner, boost::shared_ptr<Module> mod);
protected:
    struct info {
        info() { magic_ = 0xb00d3333; }
        ~info() { assert(magic_ == 0xb00d3333); }
        unsigned int magic_;
        boost::shared_ptr<Property> prop;
        boost::shared_ptr<PropertyDisplay> disp;
        int height();
        int width();
        void draw(int x, int y, int w, int h);
        char const *text();
    };
    boost::shared_ptr<Module> mod_;
    PropertyBrowser<info> *browser_;
    void build();
};

#endif  //  rl2_PropertyWindow_h
