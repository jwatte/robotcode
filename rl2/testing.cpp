
#include "testing.h"
#include <iostream>
#include <stdlib.h>


bool assert_fail(char const *file, int line, std::string const &expr) {
    std::cerr << file << ":" << line << ": assertion failed: " << expr << std::endl;
    exit(1);
    return false;
}


