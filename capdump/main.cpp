#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>


struct Hdr {
    unsigned int what;
    unsigned int size;
    double time;
};

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        std::cerr << "usage: capdump filename.bin" << std::endl;
        return 1;
    }
    std::ifstream is(argv[1], std::ios_base::binary);
    while (!!is) {
        Hdr hdr;
        std::streampos sp = is.tellg();
        is.read((char *)&hdr, sizeof(hdr));
        std::cout << std::hex << hdr.what << " " <<
            std::hex << hdr.size << " " <<
            std::dec << hdr.time << std::endl;
        is.seekg(hdr.size + sizeof(hdr) + sp);
    }
    return 0;
}
