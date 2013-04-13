#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>


struct Hdr {
    unsigned int what;
    unsigned int size;
    double time;
};

class IParser {
public:
    virtual void parse(std::ifstream &is) = 0;
};
IParser *fmt;
struct sertype {
    unsigned char type;
    unsigned char hlen;
    unsigned char olen;
};

sertype sertypes[] = {
    { 'D', 3, 2 },
    { 'O', 1, 0 },
};

class Serial : public IParser {
public:
    virtual void parse(std::ifstream &is) {
        unsigned char ch;
        bool ok = true;
        while (!!is) {
            ch = is.get();
            if (ch != 0xed) {
                if (ok) {
                    std::cout << std::endl;
                }
                ok = false;
                std::cout << " *0x" << std::hex << (unsigned int)ch;
                continue;
            }
            else {
                if (!ok) {
                    ok = true;
                    std::cout << std::endl;
                }
                std::cout << "0x" << std::hex << (unsigned int)ch;
                ch = is.get();
                for (size_t i = 0; i != sizeof(sertypes)/sizeof(sertypes[0]); ++i) {
                    if (sertypes[i].type == ch) {
                        unsigned char buf[256];
                        buf[0] = ch;
                        unsigned char nb = sertypes[i].hlen;
                        unsigned char of = 1;
                        while (of < nb) {
                            ch = is.get();
                            buf[of] = ch;
                            ++of;
                        }
                        if (sertypes[i].olen) {
                            assert(sertypes[i].olen < sertypes[i].hlen);
                            nb += buf[sertypes[i].olen];
                        }
                        while (of < nb) {
                            ch = is.get();
                            buf[of] = ch;
                            ++of;
                        }
                        for (size_t i = 0; i != of; ++i) {
                            std::cout << " 0x" << std::hex << (unsigned int)buf[i];
                        }
                        goto done;
                    }
                }
                ok = false;
                std::cout << std::endl;
                std::cout << " *0x" << std::hex << (unsigned int)ch;
                continue;
done:
                std::cout << std::endl;
            }
        }
    }
};
Serial serial;

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        std::cerr << "usage: capdump type filename.bin" << std::endl;
        return 1;
    }
    if (argv[1] == std::string("serial")) {
        fmt = &serial;
    }
    else {
        std::cerr << "unknown format: " << argv[1] << std::endl;
        return 1;
    }
    std::ifstream is(argv[2], std::ios_base::binary);
    fmt->parse(is);
    return 0;
}
