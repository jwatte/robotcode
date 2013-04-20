#include "analysis.h"

static float tolerance = 0.02;
static float normalization = 0.5;

//  f5 1b 34   cd 45 51   c9 45 52  f9 a1 8b  e7 26 41
static Color orange_color(0xf9, 0xa1, 0x8b);
static Color orange_color2(0xe7, 0x26, 0x41);
static Color orange_color3((0xcd + 0xc9) / 2, (0x45 + 0x45) / 2,
    (0x51 + 0x52) / 2);


bool handle_areas(Area &out, float &dir, float &size, std::vector<ColorArea> &candidates) {
    if (!candidates.size()) {
        return 0;
    }
    float ma = 0;
    size_t mi = 0;
    for (auto ptr(candidates.begin()), end(candidates.end()); ptr != end; ++ptr) {
        float curWt = (*ptr).area.width * (*ptr).area.height;
        if (curWt > ma) {
            ma = curWt;
            mi = ptr - candidates.begin();
        }
    }
    //  significantly better than alternatives?
    if (ma > out.width * out.height / 4096) {
        dir = ((float)candidates[mi].cog.left - (out.left + out.width) / 2)
            / float(out.width / 2);
        ColorArea ca(candidates[mi]);
        out = ca.area;
        size = candidates[mi].area.width * candidates[mi].area.height;
        return true;
    }
    return false;
}

bool find_a_cone(RPixmap &pm, Area &out, float &dir, float &size, bool paint) {
    std::vector<ColorArea> orange_areas;
    Area interest_orange(0, 0, pm.width, pm.height);
    pm.find_areas_of_color(interest_orange, orange_color3, tolerance, normalization, pm.width*pm.height/10000 + 10, orange_areas, paint, Color(255, 0, 0));
    pm.find_areas_of_color(interest_orange, orange_color2, tolerance, normalization, pm.width*pm.height/10000 + 10, orange_areas, paint, Color(0, 255, 0));
    pm.find_areas_of_color(interest_orange, orange_color, tolerance, normalization, pm.width*pm.height/10000 + 10, orange_areas, paint, Color(0, 0, 255));

    return handle_areas(out, dir, size, orange_areas);
}

