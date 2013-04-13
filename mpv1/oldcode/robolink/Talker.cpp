#include "Talker.h"

#include <boost/foreach.hpp>
#include <algorithm>


Talker::Talker()
{
}

Talker::~Talker()
{
}

void Talker::add_listener(Listener *l)
{
    listeners_.push_back(l);
}

void Talker::remove_listener(Listener *l)
{
    std::vector<Listener *>::iterator ptr(std::find(listeners_.begin(), listeners_.end(), l));
    if (ptr != listeners_.end())
    {
        listeners_.erase(ptr);
    }
}

void Talker::invalidate()
{
    std::vector<Listener *> tmp(listeners_);
    BOOST_FOREACH(Listener *l, tmp)
    {
        l->invalidate();
    }
}


