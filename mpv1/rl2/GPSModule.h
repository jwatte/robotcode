#if !defined(GPSModule_h)
#define GPSModule_h

#include "Module.h"
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

class Settings;
class Property;
struct gps_data_t;

class GPSModule : public cast_as_impl<Module, GPSModule> {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
    void step();
    std::string const &name();

    virtual size_t num_properties();
    virtual boost::shared_ptr<Property> get_property_at(size_t ix);
    virtual void set_return(boost::shared_ptr<IReturn> const &) {}
    ~GPSModule();
private:
    struct OutputData {
        long num_sats;
        long time;
        double n;
        double e;
    };
    GPSModule();
    std::string name_;
    boost::shared_ptr<boost::thread> thread_;
    boost::mutex section_;
    gps_data_t *gps_;
    gps_data_t *save_gps_;
    boost::shared_ptr<Property> num_sats_prop_;
    boost::shared_ptr<Property> time_prop_;
    boost::shared_ptr<Property> n_prop_;
    boost::shared_ptr<Property> e_prop_;
    void thread_fn();
};

#endif  //  GPSModule_h
