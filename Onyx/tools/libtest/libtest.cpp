
#include <iostream>
#include "Transform.h"


void check(vec4 a, vec4 b) {
    if (fabsf(a.x-b.x) > 0.1f || fabsf(a.y-b.y) > 0.1f || fabsf(a.z-b.z) > 0.1f || fabsf(a.w-b.w) > 0.1f) {
        std::cerr << "vectors do not match: " << a << " != " << b << std::endl;
    }
}

int main() {
    Transform a(Translate(2, 0, 0));
    check(a * vec4(0, 0, 0), vec4(2, 0, 0));
    Transform b(Rotate(M_PI*0.5, 0, 1, 0));
    check(b * vec4(2, 0, 0), vec4(0, 0, -2));
    Transform c(Rotate(M_PI*0.5, 0, 1, 0)
        * Translate(2, 0, 0));
    check(c * vec4(0, 0, 0), vec4(0, 0, -2));
    return 0;
}

