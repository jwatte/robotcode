#if !defined(rl2_Listener_h)
#define rl2_Listener_h

#include <boost/enable_shared_from_this.hpp>
#include "cast_as.h"

class Listener : public cast_as_base, public boost::enable_shared_from_this<Listener> {
public:
    virtual void on_change() = 0;
    virtual ~Listener() {}
protected:
};

class Listenable;

class ListenerTramp : public Listener {
public:
    ListenerTramp(Listener *l);
    void on_change();
    void attach(boost::shared_ptr<Listenable> a);
    void detach();
private:
    Listener *l_;
    boost::shared_ptr<Listenable> a_;
};

#endif  //  rl2_Listener_h
