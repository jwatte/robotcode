
#include <stdio.h>
#include "analysis.h"

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
    pm.color_correct();
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

