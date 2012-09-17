#if !defined(rl2_WindowOwner_h)
#define rl2_WindowOwner_h

#include <boost/shared_ptr.hpp>

class OwnedWindow;

class WindowOwner {
public:
    virtual void window_close(boost::shared_ptr<OwnedWindow> w) = 0;
};

#endif  //  rl2_WindowOwner_h
