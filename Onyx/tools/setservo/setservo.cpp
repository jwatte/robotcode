
#include "ServoSet.h"
#include "USBLink.h"
#include <math.h>
#include <boost/lexical_cast.hpp>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/time.h>
#include <unistd.h>
#include <map>
#include <sstream>


void usage() {
    fprintf(stderr, "usage: setservo [-v] id:position ...\n");
    fprintf(stderr, "id = 1 .. 254\n");
    fprintf(stderr, "position = 0 .. 4095\n");
    exit(1);
}

std::map<int, int> positions;

class L : public Logger {
public:
    L() {}
    virtual void log_data(LogWhat what, void const *data, size_t size)
    {
        std::stringstream ss;
        ss << std::hex << (int)what;
        for (size_t i = 0; i < size; ++i)
        {
            ss << " " << (int)((unsigned char *)data)[i];
        }
        std::cerr << ss.str() << std::endl;
    }
};

int main(int argc, char const *argv[]) {
    boost::shared_ptr<Logger> logger;
    if (argv[1] && !strcmp(argv[1], "-v")) {
        logger.reset(new L());
        --argc;
        ++argv;
    }
    if (argc < 2) {
        usage();
    }
    ServoSet ss(true, logger);
    for (int i = 1; i < argc; ++i) {
        int id, pos;
        if (sscanf(argv[i], "%d:%d", &id, &pos) != 2) {
            usage();
        }
        if (id < 1 || id > 254 || pos < 0 || pos > 4095) {
            usage();
        }
        positions[id] = pos;
    }
    for (auto ptr(positions.begin()), end(positions.end()); ptr != end; ++ptr) {
        ss.add_servo((*ptr).first, (*ptr).second);
    }
    int i = 0;
    while (true) {
        ss.step();
        usleep(50000);
        ++i;
        if (i == 10) {
            ss.set_torque(900);
            for (auto ptr(positions.begin()), end(positions.end()); ptr != end; ++ptr) {
                ss.id((*ptr).first).set_goal_position((*ptr).second);
            }
        }
    }
    return 0;
}

