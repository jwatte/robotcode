#if !defined(Talker_h)
#define Talker_h

#include <vector>

class Listener
{
public:
    virtual void invalidate() = 0;
protected:
    virtual ~Listener() {}
};

class Talker : public Listener
{
public:
    Talker();
    ~Talker();
    void add_listener(Listener *l);
    void remove_listener(Listener *l);
    void invalidate();
protected:
    std::vector<Listener *> listeners_;
};

#endif  //  Talker_h
