#if !defined(ay_h)
#define ay_h

#include <boost/shared_ptr.hpp>
#include <string>

class Image;
typedef boost::shared_ptr<Image> ImagePtr;

class Image {
public:

  static ImagePtr create(size_t width, size_t height, size_t rowbytes = 0, void *data = 0);

  unsigned char *data() const { return data_; }
  size_t size() const { return size_; }
  size_t rowbytes() const { return rowbytes_; }
  size_t width() const { return width_; }
  size_t height() const { return height_; }
  ~Image();

private:

  Image(size_t size, size_t w, size_t h, size_t rb, void *data);
  unsigned char *data_;
  size_t size_;
  size_t rowbytes_;
  size_t width_;
  size_t height_;
  bool owned_;
};

struct rect {
    rect() : x1(0), y1(0), x2(0), y2(0) {}
    rect(int l, int t, int r, int b) : x1(l), y1(t), x2(r), y2(b) {}
    int x1;
    int y1;
    int x2; 
    int y2;
    int width() const { return x2 - x1; }
    int height() const { return y2 - y1; }
    void width(int w) { x2 = x1 + w; }
    void height(int h) { y2 = y1 + h; }
};

ImagePtr load_image(std::string const &path);
void save_image(ImagePtr image, std::string const &path);
void flip_v(ImagePtr image);
void flip_rb(ImagePtr image);
void flip_v_and_rb(ImagePtr image);

#endif
