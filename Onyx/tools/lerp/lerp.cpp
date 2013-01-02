
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
    int n = 0;
    unsigned char oldStatus = 0;
    while (true) {
        ss.step();
        usleep(1000);
        ++n;
        if (n == 3500) {
            cmd_pose pose[12];
            for (int i = 1; i <= 12; ++i) {
                pose[i-1].id = i;
                pose[i-1].pose = (rand() & 511) + table[i-1].normal - 256;
            }
            int time = (rand() & 0x100) ? ((rand() & 1023) + 1000) : ((rand() & 255) + 100);
            ss.lerp_pose(time, pose, 12);
            std::cerr << "sent new lerp() for " << time << " milliseconds." << std::endl;
            n = 0;
        }
        unsigned char st[33];
        unsigned char status = ss.get_status(st, sizeof(st));
        if (status != oldStatus) {
            std::cerr << "new status: 0x" << std::hex << (int)status << "." << std::dec << std::endl;
            oldStatus = status;
        }
    }
}

