#if !defined(rl2_cast_as_h)
#define rl2_cast_as_h

#include <typeinfo>

class cast_as_base {
public:
    template<typename T> T *cast_as() {
        void *vp = cast_as_fun(typeid(T));
        return reinterpret_cast<T *>(vp);
    }
protected:
    virtual void *cast_as_fun(std::type_info const &ti);
};

template<typename Base, typename Derived> class cast_as_impl : public Base {
public:
    cast_as_impl() : Base() {}
    template<typename A1> cast_as_impl(A1 a1) : Base(a1) {}
    template<typename A1, typename A2> cast_as_impl(A1 a1, A2 a2) : Base(a1, a2) {}
    template<typename A1, typename A2, typename A3> cast_as_impl(A1 a1, A2 a2, A3 a3) : Base(a1, a2, a3) {}
protected:
    void *cast_as_fun(std::type_info const &ti) {
        if (ti == typeid(Derived)) {
            return static_cast<Derived *>(this);
        }
        return Base::cast_as_fun(ti);
    }
};

#endif  //  rl2_cast_as_h
