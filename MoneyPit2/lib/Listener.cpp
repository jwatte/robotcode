
#include "Listener.h"
#include "Listenable.h"

ListenerTramp::ListenerTramp(Listener *l) :
    l_(l) {
}

void ListenerTramp::on_change() {
    if (l_) {
        l_->on_change();
    }
}

void ListenerTramp::attach(boost::shared_ptr<Listenable> a) {
    if (a_) {
        a_->remove_listener(shared_from_this());
    }
    a_ = a;
    if (a_) {
        a_->add_listener(shared_from_this());
    }
}

void ListenerTramp::detach() {
    attach(boost::shared_ptr<Listenable>());
    l_ = 0;
}

