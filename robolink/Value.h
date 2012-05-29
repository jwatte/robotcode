#if !defined(Value_h)
#define Value_h

#include "Talker.h"

template<typename T>
class Value : public Talker
{
public:
    Value(unsigned char offset) :
        offset_(offset)
    {
    }
    void step(void const *src)
    {
        T const &tref = *(T const*)((char const *)src + offset_);
        if (tref != oldValue_)
        {
            oldValue_ = tref;
            invalidate();
        }
    }

    T oldValue_;
    unsigned char offset_;
};

#endif  //  Value_h

