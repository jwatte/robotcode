
#include "async.h"
#include "semaphore.h"
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <list>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <fstream>

#define MAX_QUEUE_SIZE 10

struct fileitem {
    std::vector<char> data;
    std::string name;
    void swap(fileitem &o) {
        data.swap(o.data);
        name.swap(o.name);
    }
};

static std::list<fileitem> files;
static boost::mutex lock;
static boost::shared_ptr<boost::thread> thread;
static boost::filesystem::path dirpath;
static semaphore ready(0);

static void thread_func() {
    try {
        while (!thread->interruption_requested()) {
            fileitem fi;
            ready.acquire();
            {
                boost::unique_lock<boost::mutex> l(lock);
                if (files.size() > 0) {
                    fi.swap(files.front());
                    files.pop_front();
                }
            }
            if (fi.data.size()) {
                boost::filesystem::ofstream of(dirpath / boost::filesystem::path(fi.name));
                of.write(&fi.data[0], fi.data.size());
            }
        }
        std::cerr << "async_file_dump thread done" << std::endl;
    }
    catch (std::exception const &x) {
        std::cerr << "async file dump exception: " << x.what() << std::endl;
    }
}

void start_async_file_dump(char const *dirname) {
    if (!!thread) {
        throw std::runtime_error("async file dump is already running");
    }
    if (ready.nonblocking_available()) {
        ready.release_n(ready.nonblocking_available());
    }
    boost::filesystem::path p(dirname);
    boost::filesystem::path p2(".");
    if (dirname[0] == '/') {
        p2 = p;
    }
    else {
        p2 /= p;
    }
    boost::filesystem::create_directories(p2);
    dirpath = p2;
    thread = boost::shared_ptr<boost::thread>(new boost::thread(thread_func));
}

void stop_async_file_dump() {
    if (!!thread) {
        thread->interrupt();
        ready.release();
        thread->join();
        thread = boost::shared_ptr<boost::thread>();
    }
}

bool async_file_dump(char const *name, void const *data, size_t size) {
    {
        boost::unique_lock<boost::mutex> l(lock);
        if (files.size() > MAX_QUEUE_SIZE) {
            return false;
        }
        files.push_back(fileitem());
        files.back().data.resize(size);
        if (size) {
            memcpy(&files.back().data[0], data, size);
        }
        files.back().name = name;
    }
    ready.release();
    return true;
}


