
#include "USBLink.h"
#include "Settings.h"
#include "util.h"
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <stdio.h>

#define SET_REG1 0x13
#define SET_REG2 0x14

class R : public USBReceiver {
public:
    R() {}
    void on_data(unsigned char const *info, unsigned char sz) {
        std::cout << "data " << (int)sz << " bytes" << std::endl;
    }
};

void usage() {
    fprintf(stderr, "usage: setreg [-q] [-b baudbyte] {id:reg:vv | id:reg:vvvv} ...\n");
    fprintf(stderr, "id is decimal; reg and data is hex.\n");
    exit(1);
}


unsigned char baudbyte = 1;
bool quiet = false;

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        usage();
    }
    if (!strcmp(argv[1], "-q")) {
        quiet = true;
        ++argv;
        --argc;
    }
    if (!strcmp(argv[1], "-b")) {
        if (argc < 4) {
            usage();
        }
        baudbyte = atoi(argv[2]);
        if (baudbyte < 0 || baudbyte > 210) {
            fprintf(stderr, "bad baudbyte %d\n", baudbyte);
            usage();
        }
        argv += 2;
        argc -= 2;
    }
    boost::shared_ptr<Settings> st(Settings::load("settings.ini"));
    boost::shared_ptr<Module> usbl(USBLink::open(st, boost::shared_ptr<Logger>()));
    USBLink *ul = usbl->cast_as<USBLink>();
    unsigned char init_junk = 0xfe;
    ul->raw_send(&init_junk, 1);
    //  seq num, cmd, data
    unsigned char rawmodecmd[3] = { 0, 1, baudbyte };
    ul->raw_send(rawmodecmd, 3);
    unsigned char ncmd = 1;
    while (argv[1]) {
        int nreg = 0;
        char const *str = argv[1];
        char *end = (char *)strchr(str, ':');
        if (!end) {
fmterr:
            fprintf(stderr, "not a proper argument: %s\n", str);
            usage();
        }
        int id = (int)strtol(str, &end, 10);
        if (*end != ':') {
            goto fmterr;
        }
        str = end + 1;
        if (strlen(str) == 7) {
            nreg = 2;
        }
        else if (strlen(str) == 5) {
            nreg = 1;
        }
        else {
            fprintf(stderr, "Arg doesn't match format xx;xx or xx:xxxx: %s\n", argv[1]);
            usage();
        }
        int a, b;
        if (2 != sscanf(str, "%x:%x", &a, &b)) {
            fprintf(stderr, "not proper hex data: %s\n", argv[1]);
            usage();
        }
        if (nreg == 1) {
            unsigned char cmd[5] = { ncmd, SET_REG1, (unsigned char)id, (unsigned char)a, (unsigned char)b };
            ul->raw_send(cmd, 5);
        }
        else if (nreg == 2) {
            unsigned char cmd[6] = { ncmd, SET_REG2, (unsigned char)id, (unsigned char)a, (unsigned char)(b & 0xff), (unsigned char)(b >> 8) };
            ul->raw_send(cmd, 6);
        }
        ++argv;
        --argc;
        ++ncmd;
        ul->step();
        usleep(10000);
    }
    for(int i = 0; i < 10; ++i) {
        usleep(5000);
        ul->step();
        if (!quiet) {
            size_t sz = 0;
            while (const unsigned char *ptr = ul->begin_receive(sz)) {
                for (size_t i = 0; i < sz; ++i) {
                    std::cout << " " << hexnum(ptr[i]);
                }
                std::cout << std::endl;
                ul->end_receive(sz);
            }
        }
    }
    return 0;
}

