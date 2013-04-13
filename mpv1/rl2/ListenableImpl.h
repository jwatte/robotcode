#if !defined(rl2_ListenableImpl_h)
#define rl2_ListenableImpl_h

#include "Listenable.h"
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <assert.h>

template<typename T = Listenable> class ListenableImpl : public T {
public:
    ListenableImpl() : T() {}
    template<typename A1> ListenableImpl(A1 a1) : T(a1) {}
    template<typename A1, typename A2> ListenableImpl(A1 a1, A2 a2) : T(a1, a2) {}
    template<typename A1, typename A2, typename A3> ListenableImpl(A1 a1, A2 a2, A3 a3) : T(a1, a2, a3) {}
    virtual void add_listener(boost::shared_ptr<Listener> const &l) {
#if !defined(NDEBUG)
        std::vector<boost::shared_ptr<Listener>>::iterator ptr(
            std::find(listeners_.begin(), listeners_.end(), l));
        assert(ptr == listeners_.end());
#endif
        listeners_.push_back(l);
    }
    virtual void remove_listener(boost::shared_ptr<Listener> const &l) {
        std::vector<boost::shared_ptr<Listener>>::iterator ptr(
            std::find(listeners_.begin(), listeners_.end(), l));
#if !defined(NDEBUG)
        assert(ptr != listeners_.end());
#endif
        listeners_.erase(ptr);
    }
    virtual void on_change() {
        //  allow changing the list while iterating
        std::vector<boost::shared_ptr<Listener>> copy(listeners_);
        BOOST_FOREACH(boost::shared_ptr<Listener> const &ptr, copy) {
            ptr->on_change();
        }
    }
protected:
    std::vector<boost::shared_ptr<Listener>> listeners_;
};

#endif  //  rl2_ListenableImpl_h
