#if !defined(rl2_testing_h)
#define rl2_testing_h

#include <string.h>
#include <boost/lexical_cast.hpp>

template<typename T>
inline std::string assert_eval(T t) {
    return boost::lexical_cast<std::string>(t);
}

template<typename T>
inline bool _assert_equal(T a, T b) {
    return a == b;
}
template<>
inline bool _assert_equal(char const * a, char const * b) {
    return !strcmp(a, b);
}

#define assert_equal(a, b) \
    (_assert_equal(a, b) || assert_fail(__FILE__, __LINE__, assert_eval(a) + " != " + assert_eval(b) + "; " + #a + " == " + #b))

bool assert_fail(char const *file, int line, std::string const &expr);


#endif  //  rl2_testing_h
