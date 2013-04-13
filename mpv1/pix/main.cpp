
#include <stdio.h>
#include "analysis.h"


static Color the_color(0xf2, 0x7a, 0x72);
static Color the_color2(0xea, 0xf7, 0x93);
//  any higher than 0.0025 and skin tone is picked up for orange
static float tolerance = 0.0025;
static float normalization = 0.5;

int main(int argc, char const *argv[]) {
    if (argc != 2 || argv[1][0] == '-') {
        fprintf(stderr, "usage: pix input.jpg (creates .tga)\n");
        return 1;
    }

    ImagePtr ip(new Image());
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "cannot open '%s'\n", argv[1]);
        return 1;
    }
    fseek(f, 0, 2);
    long l = ftell(f);
    fseek(f, 0, 0);
    void *data = ip->alloc_compressed(l, true);
    fread(data, l, 1, f);
    fclose(f);
    ip->complete_compressed(l);

    Pixmap pm(ip, false);
    fprintf(stderr, "%dx%d\n", pm.width, pm.height);
    //pm.color_correct();
    std::vector<ColorArea> areas;
    pm.find_areas_of_color(Area(0, 0, pm.width, pm.height), the_color, 
        tolerance, normalization, pm.width*pm.height/10000 + 2, areas);
    Color complement(the_color.complement());
    fprintf(stderr, "%ld orange clusters\n", (long)areas.size());
    for (size_t i = 0, n = areas.size(); i != n; ++i) {
        Area a(areas[i].area);
        if (a.left > 0) {
            a.left--;
            a.width++;
        }
        if (a.top > 0) {
            a.top--;
            a.height++;
        }
        if (a.right() < pm.width-1) {
            a.width++;
        }
        if (a.bottom() < pm.height-1) {
            a.height++;
        }
        pm.frame_rect(a, complement);
        fprintf(stderr, "%d,%d-%d,%d  sz %g  wt %.3f\n", a.left, a.top, a.right(), a.bottom(), a.area(), areas[i].weight);
    }

    areas.clear();
    pm.find_areas_of_color(Area(0, 0, pm.width, pm.height), the_color2, 
        tolerance, normalization, pm.width*pm.height/10000 + 2, areas);
    complement = the_color2.complement();
    fprintf(stderr, "%ld yellow clusters\n", (long)areas.size());
    for (size_t i = 0, n = areas.size(); i != n; ++i) {
        Area a(areas[i].area);
        if (a.left > 0) {
            a.left--;
            a.width++;
        }
        if (a.top > 0) {
            a.top--;
            a.height++;
        }
        if (a.right() < pm.width-1) {
            a.width++;
        }
        if (a.bottom() < pm.height-1) {
            a.height++;
        }
        pm.frame_rect(a, complement);
        fprintf(stderr, "%d,%d-%d,%d  sz %g  wt %.3f\n", a.left, a.top, a.right(), a.bottom(), a.area(), areas[i].weight);
    }

    std::string opath(argv[1]);
    opath = opath.substr(0, opath.find_last_of("."));
    opath += ".tga";
    f = fopen(opath.c_str(), "wb");
    if (!f) {
        fprintf(stderr, "cannot create '%s'\n", opath.c_str());
        return 2;
    }
    char hdr[256];
    size_t sz = pm.get_tga_header(hdr, sizeof(hdr));
    fwrite(hdr, sz, 1, f);
    pm.to_tga_order();
    fwrite(pm.bits, pm.rowbytes, pm.height, f);
    fclose(f);

    return 0;
}

