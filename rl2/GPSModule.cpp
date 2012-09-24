#include "GPSModule.h"
#include "PropertyImpl.h"

#include <boost/bind.hpp>
#include <boost/thread/locks.hpp>

#include <gps.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>



boost::shared_ptr<Module> GPSModule::open(boost::shared_ptr<Settings> const &set) {
    return boost::shared_ptr<Module>(new GPSModule());
}

void GPSModule::step() {
    bool different = false;
    {
        boost::unique_lock<boost::mutex> lock(section_);
        if (memcmp(gps_, save_gps_, sizeof(*gps_))) {
            different = true;
            memcpy(save_gps_, gps_, sizeof(*gps_));
        }
    }
    if (different) {
        if (save_gps_->status) {
            num_sats_prop_->set<long>(save_gps_->satellites_visible);
            if (save_gps_->set & TIME_SET) {
                time_prop_->set<long>(save_gps_->fix.time);
            }
            if (save_gps_->set & LATLON_SET) {
                n_prop_->set<double>(save_gps_->fix.longitude);
                e_prop_->set<double>(save_gps_->fix.latitude);
            }
        }
        else {
            num_sats_prop_->set<long>(0);
        }
    }
}

std::string const &GPSModule::name() {
    return name_;
}

size_t GPSModule::num_properties() {
    return 4;
}

boost::shared_ptr<Property> GPSModule::get_property_at(size_t ix) {
    switch (ix) {
        case 0: return num_sats_prop_;
        case 1: return time_prop_;
        case 2: return n_prop_;
        case 3: return e_prop_;
        default: throw std::runtime_error("Invalid index in GPSModule::get_property_at()");
    }
}

GPSModule::~GPSModule() {
    thread_->interrupt();
    thread_->join();
    gps_close(gps_);
    delete gps_;
    delete save_gps_;
}

static std::string str_num_sats("num_sats");
static std::string str_time("time");
static std::string str_n("N");
static std::string str_e("E");

GPSModule::GPSModule() :
    name_("GPS"),
    gps_(new gps_data_t),
    save_gps_(new gps_data_t) {
    memset(gps_, 0, sizeof(*gps_));
    memset(save_gps_, 0, sizeof(*save_gps_));
    num_sats_prop_ = boost::shared_ptr<Property>(new PropertyImpl<long>(str_num_sats));
    time_prop_ = boost::shared_ptr<Property>(new PropertyImpl<long>(str_time));
    n_prop_ = boost::shared_ptr<Property>(new PropertyImpl<double>(str_n));
    e_prop_ = boost::shared_ptr<Property>(new PropertyImpl<double>(str_e));

    int err;
    if ((err = gps_open("localhost", "2947", gps_)) != 0) {
        throw std::runtime_error(std::string("GPSModule::open(): ") + name_
            + " error: " + gps_errstr(err));
    }
    if ((err = gps_stream(gps_, WATCH_NEWSTYLE | WATCH_SCALED, 0)) < 0) {
        throw std::runtime_error(std::string("GPSModule::open(): gps_stream() failed"));
    }
    thread_ = boost::shared_ptr<boost::thread>(
        new boost::thread(boost::bind(&GPSModule::thread_fn, this)));
}

void GPSModule::thread_fn() {
    while (!thread_->interruption_requested()) {
        if (gps_waiting(gps_, 100)) {
            boost::unique_lock<boost::mutex> lock(section_);
            if (gps_read(gps_) < 0) {
                memset(gps_, 0, sizeof(*gps_));
            }
        }
    }
}


