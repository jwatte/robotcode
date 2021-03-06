#if !defined(Value_h)
#define Value_h

#include "Talker.h"

template<typename T, int Old = 0, int New = 1>
class Value : public Talker
{
public:
    Value(unsigned char offset) :
        oldValue_(T()),
        offset_(offset)
    {
    }
    void step(void const *src)
    {
        T const &tref = *(T const*)((char const *)src + offset_);
        if (tref != oldValue_)
        {
            oldValue_ = (oldValue_ * Old + tref * New) / (Old + New);
            invalidate();
        }
    }
    T const &value() const { return oldValue_; }

protected:
    T oldValue_;
    unsigned char offset_;
};

template<typename T>
class ValueShadow : public Listener
{
public:
    ValueShadow() :
        src_(0),
        value_(T()),
        dirty_(false)
    {
    }
    ValueShadow(Value<T> *src) :
        src_(0),
        value_(T()),
        dirty_(false)
    {
        set(src);
    }
    ~ValueShadow()
    {
        set(0);
    }
    void set(Value<T> *src)
    {
        if (src == 0)
        {
            get();
        }
        if (src_ != 0)
        {
            src_->remove_listener(this);
        }
        src_ = src;
        if (src != 0)
        {
            src_->add_listener(this);
            dirty_ = true;
        }
    }
    T const &get() const
    {
        if (dirty_)
        {
            dirty_ = false;
            if (src_ != 0)
            {
                value_ = src_->value();
            }
        }
        return value_;
    }
    operator T const &() const
    {
        return get();
    }
protected:
    void invalidate()
    {
        dirty_ = true;
    }
    Value<T> *src_;
    mutable T value_;
    mutable bool dirty_;
};

#endif  //  Value_h

