#if !defined(rl2_Camera_h)
#define rl2_Camera_h

#include "Module.h"
#include "PropertyImpl.h"
#include "semaphore.h"
#include <boost/thread.hpp>

#include <libv4l2.h>
#include <linux/videodev2.h>

class Settings;
class Image;

class Camera : public Module {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
    void step();
    std::string const &name();

    virtual size_t num_properties();
    virtual boost::shared_ptr<Property> get_property_at(size_t ix);
    ~Camera();
private:
    Camera(std::string const &devname, unsigned int capWidth, unsigned int capHeight);
    std::string devname_;
    int fd_;
    boost::shared_ptr<boost::thread> thread_;

    //  processing thread
    static void thread_fn(void *arg);
    void process();
    void queue(size_t ix);
    void wait();

    //  dev handling
    void configure_dev();
    void configure_buffers();
    void start_capture();
    void stop_capture();
    void poll_and_queue();

    enum { NUM_BUFS = 3 };
    semaphore imageGrabbed_;
    boost::shared_ptr<Image> forGrabbing_[NUM_BUFS];
    boost::system_time lastTime_;
    boost::system_time lastStepTime_;
    boost::mutex guard_;
    unsigned int capWidth_;
    unsigned int capHeight_;
    unsigned int lastReturned_;
    unsigned int nextWaiting_;
    unsigned int lastQueued_;
    unsigned int inQueue_;
    semaphore imageReturned_;
    double fps_;
    double fpsStep_;
    long underflows_;
    v4l2_requestbuffers rbufs_;
    v4l2_buffer vbufs_[NUM_BUFS];
    struct buffer {
        buffer() : ptr(0), size(0) {}
        void *ptr;
        size_t size;
    } bufs_[NUM_BUFS];

    boost::shared_ptr<Property> imageProperty_;
    boost::shared_ptr<Property> fpsProperty_;
    boost::shared_ptr<Property> fpsStepProperty_;
    boost::shared_ptr<Property> underflowsProperty_;
};

#endif  //  rl2_Camera_h

