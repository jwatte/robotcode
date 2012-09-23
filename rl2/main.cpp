
#include "main.h"
#include "str.h"
#include "ModuleList.h"
#include "Camera.h"
#include "USBLink.h"
#include "Settings.h"
#include "BrowserWindow.h"
#include "Boards.h"
#include "protocol.h"

#include <cstring>
#include <cerrno>

#include <map>
#include <vector>
#include <stdexcept>
#include <iostream>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include <FL/Fl.H>
#include <FL/Fl_Shared_Image.H>

static std::map<std::string, std::string> g_args;
static std::vector<std::string> g_files;
boost::shared_ptr<Module> gUSBLink;
boost::shared_ptr<Module> gMotorBoard;
boost::shared_ptr<Module> gInputBoard;
boost::shared_ptr<Module> gUSBBoard;


double g_pulse;


bool get_arg(std::string const &name, std::string &oval) {
    std::map<std::string, std::string>::iterator ptr(g_args.find(name));
    if (ptr == g_args.end()) {
        return false;
    }
    oval = (*ptr).second;
    return true;
}

bool get_file(size_t i, std::string &ofile) {
    if (i >= g_files.size()) {
        return false;
    }
    ofile = g_files[i];
    return true;
}


bool read_arg(char const *key, char const *value, std::map<std::string, std::string> &kv, int &i) {
    if (key[0] != '-' || key[1] != '-') {
        return false;
    }
    char const *v = strchr(key, '=');
    if (v) {
        kv[std::string(&key[2], v)] = std::string(v+1);
        i += 1;
        return true;
    }

    kv[&key[2]] = value;
    i += 2;
    return true;
}

void main_loop() {
    Fl::run();
}

template<typename ModuleType>
boost::shared_ptr<Module> setup_module(std::string const &optname, std::string const &boardname, 
    boost::shared_ptr<ModuleList> const &modules)
{
    std::string setfile(optname + ".js");
    get_arg(optname, setfile);
    boost::shared_ptr<Module> mod;
    if (setfile.size()) {
        boost::shared_ptr<Settings> settings = Settings::load(setfile);
        if (!settings || !settings->has_name(boardname)) {
            return mod;
        }
        std::cerr << "setting up " << optname << " " << boardname << std::endl;
        boost::shared_ptr<Settings> const &value(settings->get_value(boardname));
        mod = boost::shared_ptr<Module>(ModuleType::open(value));
        if (!mod) {
            std::cerr << "Could not load " << optname << ": " << setfile
                << ": " << boardname << std::endl;
        }
        else {
            modules->add(mod);
        }
    }
    return mod;
}

template<typename ModuleType>
std::vector<boost::shared_ptr<Module>> setup_module(std::string const &optname,
    boost::shared_ptr<ModuleList> const &modules) {
    std::string setfile(optname + ".js");
    get_arg(optname, setfile);
    std::vector<boost::shared_ptr<Module>> mods;
    if (setfile.size()) {
        boost::shared_ptr<Settings> settings = Settings::load(setfile);
        if (!settings) {
            return mods;
        }
        for (size_t i = 0, n = settings->num_names(); i != n; ++i) {
            std::string const &name(settings->get_name_at(i));
            std::cerr << "setting up " << optname << " " << name << std::endl;
            boost::shared_ptr<Settings> const &value(settings->get_value(name));
            boost::shared_ptr<Module> mod(ModuleType::open(value));
            if (!mod) {
                std::cerr << "Could not load " << optname << ": " << setfile
                    << ": " << name << std::endl;
            }
            else {
                modules->add(mod);
                mods.push_back(mod);
            }
        }
    }
    return mods;
}

void setup_cameras(boost::shared_ptr<ModuleList> const &modules) {
    setup_module<Camera>("cameras", modules);
}

void setup_usblinks(boost::shared_ptr<ModuleList> const &modules) {
    gUSBLink = setup_module<USBLink>("usblinks", "comm", modules);
}


void setup_boards(boost::shared_ptr<ModuleList> const &modules) {
   gMotorBoard = setup_module<MotorBoard>("boards", "motor", modules);
   gInputBoard = setup_module<InputBoard>("boards", "input", modules);
   gUSBBoard = setup_module<USBBoard>("boards", "usb", modules);

   gUSBLink->cast_as<USBLink>()->set_board(MOTOR_BOARD, gMotorBoard);
   gUSBLink->cast_as<USBLink>()->set_board(SENSOR_BOARD, gInputBoard);
   gUSBLink->cast_as<USBLink>()->set_board(USB_BOARD, gUSBBoard);
}

void setup_browser_window(boost::shared_ptr<ModuleList> const &modules) {
    (new BrowserWindow(modules))->show();
}

void step_all(boost::shared_ptr<ModuleList> *all_modules) {
    boost::this_thread::sleep(boost::posix_time::milliseconds(5));
    (*all_modules)->step_all();
}

void config_pulse(boost::shared_ptr<ModuleList> &modules) {
    Fl::add_idle((void (*)(void *))&step_all, &modules);
}

int main(int argc, char const *argv[]) {

    int i = 1;
    while (i < argc) {
        if (!read_arg(argv[i], argv[i+1], g_args, i)) {
            g_files.push_back(argv[i]);
            i += 1;
        }
    }

    Fl::visual(FL_RGB|FL_DOUBLE);
    fl_register_images();
    boost::shared_ptr<ModuleList> modules(ModuleList::create());
    sched_param parm = { .sched_priority = 10 };
    if (sched_setscheduler(getpid(), SCHED_RR, &parm) < 0) {
        std::string err(strerror(errno));
        std::cerr << "sched_setscheduler failed: " << err << std::endl;
    }
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &parm) < 0) {
        std::string err(strerror(errno));
        std::cerr << "pthread_setschedparam failed: " << err << std::endl;
    }
    config_pulse(modules);

    try {
        setup_cameras(modules);
        setup_usblinks(modules);
        setup_boards(modules);
        setup_browser_window(modules);
        main_loop();
    }
    catch (std::exception const &x) {
        std::cerr << "Caught exception in main: " << x.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Caught unknown error in main." << std::endl;
    }
    return 0;
}

