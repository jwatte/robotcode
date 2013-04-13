#if !defined(rl2_PropertyWindow_h)
#define rl2_PropertyWindow_h

#include "OwnedWindow.h"
#include <boost/shared_ptr.hpp>
class Module;
class PropertyBrowser;

class PropertyWindow : public cast_as_impl<OwnedWindow, PropertyWindow> {
public:
    PropertyWindow(WindowOwner *owner, boost::shared_ptr<Module> mod);
protected:
    boost::shared_ptr<Module> mod_;
    PropertyBrowser *browser_;
    void build();
};

#endif  //  rl2_PropertyWindow_h
