#if !defined(rl2_Image_h)
#define rl2_Image_h

#include <boost/noncopyable.hpp>
#include <cstdlib>
#include <vector>

enum ImageBits {
    CompressedBits,
    FullBits,
    ThumbnailBits
};

//  Image is largely intended to serve as a convenient interface 
//  for webcam MJPEG image capture. The internals of Image deals 
//  with adding the Huffman table to the incoming MJPEG data.
class Image : public boost::noncopyable {
public:
    enum { BytesPerPixel = 3 };
    Image();
    ~Image();
    //  alloc_compressed() to get a buffer of certain size
    //  then complete_compressed() when you've filled it with 
    //  JPEG data, to decompress it and calculate the thumbnail
    void *alloc_compressed(size_t size);
    void complete_compressed(size_t size);
    size_t width() const;
    size_t height() const;
    size_t width_t() const;
    size_t height_t() const;
    void const *bits(ImageBits) const;
    size_t size(ImageBits) const;
private:
    size_t width_;
    size_t height_;
    std::vector<char> compressed_;
    std::vector<char> uncompressed_;
    std::vector<char> thumbnail_;

    std::vector<char> const &vec(ImageBits ib) const;

    void decompress(size_t size);
    void make_thumbnail();
};

#endif  //  rl2_Image_h
