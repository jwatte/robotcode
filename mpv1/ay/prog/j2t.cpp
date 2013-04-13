#include "ay.h"
#include <stdexcept>
#include <iostream>


int main(int argc, char const *argv[]) {
  try {
    if (argc != 3) {
      throw std::runtime_error("usage: j2t input.jpg output.tga");
    }
    ImagePtr img(load_image(argv[1]));
    save_image(img, argv[2]);
  }
  catch (std::exception const &err) {
    std::cerr << err.what() << std::endl;
    return 1;
  }
  return 0;
}
