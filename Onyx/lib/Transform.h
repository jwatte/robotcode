#if !defined(lib_Transform_h)
#define lib_Transform_h

#include <math.h>
#include <assert.h>


struct vec4 {
    vec4() { v[0] = v[1] = v[2] = v[3] = 0; }
    vec4(float ix, float iy, float iz, float iw=1) {
        v[0] = ix; v[1] = iy; v[2] = iz; v[3] = iw;
    }
    float dot(vec4 const &o) const {
        return v[0]*o.v[0] + v[1]*o.v[1] + v[2]*o.v[2] + v[3]*o.v[3];
    }
    union {
        float v[4];
        struct {
            float x;
            float y;
            float z;
            float w;
        };
    };
};

namespace std {
    inline ostream& operator<<(ostream& os, vec4 const &v) {
        os << v.x << "," << v.y << "," << v.z << "," << v.w;
        return os;
    }
}

struct Transform {
    public:
        Transform() {
            m[0].v[0] = 1;
            m[1].v[1] = 1;
            m[2].v[2] = 1;
            m[3].v[3] = 1;
        }
        Transform(vec4 a, vec4 b, vec4 c, vec4 d = vec4(0, 0, 0, 1)) {
            m[0] = a;
            m[1] = b;
            m[2] = c;
            m[3] = d;
        }
        vec4 m[4];  // let's call it row major
        vec4 row(size_t ix) const {
            assert(ix < 4);
            return m[ix];
        }
        vec4 col(size_t ix) const {
            assert(ix < 4);
            return vec4(m[0].v[ix], m[1].v[ix], m[2].v[ix], m[3].v[ix]);
        }
        void set(size_t r, size_t c, float v) {
            assert(r < 4);
            assert(c < 4);
            m[r].v[c] = v;
        }
        Transform operator*(Transform const &o) const {
            Transform ret;
            for (int r = 0; r != 4; ++r) {
                for (int c = 0; c != 4; ++c) {
                    ret.set(r, c, row(r).dot(o.col(c)));
                }
            }
            return ret;
        }
        vec4 operator*(vec4 const &o) const {
            return vec4(m[0].dot(o), m[1].dot(o), m[2].dot(o), m[3].dot(o));
        }
};

inline Transform Translate(float dx, float dy, float dz) {
    Transform ret;
    ret.m[0].v[3] = dx;
    ret.m[1].v[3] = dy;
    ret.m[2].v[3] = dz;
    return ret;
}

inline Transform Rotate(float rad, float ax, float ay, float az) {
    Transform ret;
    float cphi = cosf(rad);
    float sphi = sinf(rad);
    float omcphi = 1-cphi;
    //  straight from Wikipedia -- let's hope they use column vectors on the right...
    ret.m[0].v[0] = cphi + ax*ax*omcphi;
    ret.m[0].v[1] = ax*ay*omcphi - az*az*sphi;
    ret.m[0].v[2] = ax*az*omcphi + ay*sphi;
    ret.m[1].v[0] = ax*ay*omcphi + az*sphi;
    ret.m[1].v[1] = cphi + ay*ay*omcphi;
    ret.m[1].v[2] = ay*az*omcphi - az*sphi;
    ret.m[2].v[0] = az*ax*omcphi - ay*sphi;
    ret.m[2].v[1] = az*ay*omcphi + ax*sphi;
    ret.m[2].v[2] = cphi + az*az*omcphi;
    return ret;
}


#endif  //  lib_Transform_h
