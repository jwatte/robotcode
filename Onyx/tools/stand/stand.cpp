
#include "ServoSet.h"

struct {
    unsigned char id;
    unsigned short normal;
}
table[] = {
    { 1, 2048+512 },
    { 2, 2048+512 },
    { 3, 2048+512 },

    { 4, 2048-512 },
    { 5, 2048-512 },
    { 6, 2048-512 },

    { 7, 2048-512 },
    { 8, 2048+512 },
    { 9, 2048+512 },

    { 10, 2048+512 },
    { 11, 2048-512 },
    { 12, 2048-512 },
};

int main() {
    ServoSet ss;
    for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); ++i) {
        ss.add_servo(table[i].id, table[i].normal);
    }
    while (true) {
        ss.step();
        usleep(1000);
    }
}

