
#include "color.h"


const float rgb::sqrt3 = sqrtf(3.0f);
const float rgb::scale510 = 1.0f / 510;
const float rgb::pi2 = M_PI * 2;
int hcl::drgbfunc[256 * 3];

rgb::operator hcl() const {
    float alpha = (2*r - g - b) * scale510;
    float beta = (sqrt3 * (g - b)) * scale510;
    float hue = M_PI, chroma = 0;
    if (alpha != 0 || beta != 0) {
        hue = atan2(beta, alpha);
        if (hue < 0) {
            hue += pi2;
        }
        chroma = sqrtf(alpha * alpha + beta * beta);
        if (chroma > 1) {
            chroma = 1;
        }
    }
    //unsigned char lightness = (std::min(r, std::min(g, b)) +
    //    std::max(r, std::max(g, b))) >> 1;
    unsigned char lightness = (r + g + b) / 3;
    return hcl((unsigned char)(hue * 255 / pi2), 
        (unsigned char)(chroma * 255),
        lightness);
}

namespace {
    class init_func {
    public:
        init_func() {
            for (int i = 0; i < 256; ++i) {
                hcl::drgbfunc[i * 3] = (int)(170 * cosf(i * M_PI / 128));
            }
            for (int i = 0; i < 256; ++i) {
                hcl::drgbfunc[i * 3 + 1] = hcl::drgbfunc[((i - 85) & 255) * 3];
                hcl::drgbfunc[i * 3 + 2] = hcl::drgbfunc[((i + 85) & 255) * 3];
            }
        }
    };
    init_func inif;
}

