#if !defined(rl2_Listenable_h)
#define rl2_Listenable_h

#include "Listener.h"

class Listenable : public cast_as_impl<Listener, Listenable> {
public:
    virtual void add_listener(boost::shared_ptr<Listener> const &l) = 0;
    virtual void remove_listener(boost::shared_ptr<Listener> const &l) = 0;
protected:
    virtual ~Listenable() {};
};

#endif //rl2_Listenable_h
