
#include <stdio.h>
#include "cones.h"
#include "analysis.h"
#include "Image.h"
#include <string.h>
#include <iostream>

int main(int argc, char const *argv[]) {
    FILE * f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "cannot open: %s\n", argv[1]);
        return 1;
    }
    fseek(f, 0, 2);
    long l = ftell(f);
    fseek(f, 0, 0);
    char *ptr = (char *)malloc(l);
    fread(ptr, 1, l, f);
    fclose(f);
    boost::shared_ptr<Image> i(new Image());
    void *d = i->alloc_compressed(l, true);
    memcpy(d, ptr, l);
    i->complete_compressed(l);

    RPixmap pm(i, false);
    Area out(0, 0, pm.width, pm.height);
    float coneSteer = 0;
    float size = 0;
    bool found = find_a_cone(pm, out, coneSteer, size, true);
    if (found) {
        std::cerr << "found cone direction " << coneSteer << " size " << size << std::endl;
        pm.frame_rect(out, Color(255, 255, 255));
    }
    pm.save_jpg("output.jpg");
    std::cerr << "wrote output.jpg" << std::endl;
    return 0;
}

