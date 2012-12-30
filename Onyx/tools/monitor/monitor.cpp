
#include "ServoSet.h"
#include <iostream>
#include <fstream>
#include <boost/lexical_cast.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Value_Output.H>
#include <FL/Fl_Roller.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Native_File_Chooser.H>


struct reginfo {
    unsigned char reg;
    Fl_Value_Output *out;
};
struct info {
    ServoSet *ss;
    Servo *s;
    Fl_Roller *roll;
    std::vector<reginfo> regs;
    std::string title;
    unsigned short id;
};

struct top_info {
    ServoSet *ss;
    std::vector<info *> infos;
    Fl_Window *top_window;
};

void timeout(void *vi) {
    info &ifo = *(info *)vi;
    for (size_t i = 0, n = ifo.regs.size(); i != n; ++i) {
        unsigned char reg = ifo.regs[i].reg;
        double v = 0;
        if (reg & 0x80) {
            v = ifo.s->get_reg2(reg & 0x7f);
        }
        else {
            v = ifo.s->get_reg1(reg);
        }
        if (v != ifo.regs[i].out->value()) {
            ifo.regs[i].out->value(v);
            ifo.regs[i].out->redraw();
        }
    }
    Fl::repeat_timeout(0.030, &timeout, vi);
}

void roll_cb(Fl_Widget *wig, void *vi) {
    info &ifo = *(info *)vi;
    ifo.s->set_goal_position(ifo.roll->value());
}

void step_usb(void *vi) {
    ServoSet *ss = (ServoSet *)vi;
    ss->step();
    Fl::repeat_timeout(0.002, &step_usb, vi);
}

Fl_Native_File_Chooser *nfc;

void save_btn(Fl_Widget *w, void *arg) {
    top_info &ti = *(top_info *)arg;
    if (nfc == NULL) {
        nfc = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
        if (nfc->show() == 0) {
            std::string path(nfc->filename());
            if (path.size()) {
                std::ofstream ofs(path.c_str());
                ofs << "# pose 1.0" << std::endl;
                ofs << ti.infos.size() << std::endl;
                for (std::vector<info *>::iterator ptr(ti.infos.begin()), end(ti.infos.end());
                        ptr != end; ++ptr) {
                    ofs << (*ptr)->id << " " << (*ptr)->roll->value() << std::endl;;
                }
            }
        }
        nfc = NULL;
    }
}

void quit_btn(Fl_Widget *w, void *d) {
    exit(0);
}

bool file_exists(std::string const &path) {
    struct stat stbuf;
    if (stat(path.c_str(), &stbuf) >= 0) {
        return S_ISREG(stbuf.st_mode);
    }
    return false;
}

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        std::cerr << "usage: monitor [file.txt | <id> ...]" << std::endl;
        return 1;
    }
    ServoSet ss;
    top_info ti;
    ti.ss = &ss;
    struct initinfo {
        unsigned short id;
        unsigned short pose;
    };
    std::vector<initinfo> init;
    if (argc == 2 && file_exists(argv[1])) {
        std::ifstream ifs(argv[1]);
        char hdr[1024];
        ifs.getline(hdr, 1024);
        hdr[1023] = 0;
        if (strncmp(hdr, "# pose 1", 8)) {
            std::cerr << argv[1] << ": not a version 1 pose file" << std::endl;
            return 2;
        }
        int n; ifs >> n;
        for (int i = 0; i < n; ++i) {
            initinfo ii;
            ifs >> ii.id;
            ifs >> ii.pose;
            std::cerr << (int)ii.id << " " << ii.pose << std::endl;
            init.push_back(ii);
        }
    }
    else {
        for (int argix = 1; argix < argc; ++argix) {
            initinfo ii;
            ii.id = boost::lexical_cast<int>(argv[argix]);
            if (ii.id < 1 || ii.id > 253) {
                std::cerr << "valid id is in 1 .. 253; got " << ii.id << std::endl;
                return 1;
            }
            ii.pose = 2048;
            init.push_back(ii);
        }
    }
    for (int i = 0, n = init.size(); i != n; ++i ) {
        initinfo ii = init[i];
        info &ifo = *new info;
        ifo.id = ii.id;
        Servo &s(ss.add_servo(ii.id, ii.pose));

        Fl_Window *win = new Fl_Window(310 * (i % 4) + 10, 240 * (i / 4) + 10, 300, 200);
        ifo.title = std::string("Servo ") + boost::lexical_cast<std::string>(ii.id);
        win->label(ifo.title.c_str());
        ifo.ss = &ss;
        ifo.s = &s;
        static const struct {
            unsigned char reg;
            char const *name;
        }
        regs[] = {
            { REG_ID, "ID" },
            { REG_GOAL_POSITION | 0x80, "Goal" },
            { REG_PRESENT_POSITION | 0x80, "Pos" },
            { REG_CURRENT | 0x80, "Curr" },
            { REG_PRESENT_TEMPERATURE, "Temp" },
            { REG_PRESENT_VOLTAGE, "Volt" }
        };
        int y = 10;
        for (size_t i = 0; i < sizeof(regs)/sizeof(regs[0]); ++i) {
            reginfo ri;
            ri.reg = regs[i].reg;
            ri.out = new Fl_Value_Output(50, y, 240, 20, regs[i].name);
            ri.out->align(FL_ALIGN_LEFT);
            ri.out->value(-1);
            ifo.regs.push_back(ri);
            y += 20;
        }
        ifo.roll = new Fl_Roller(50, y, 240, 20, "Move");
        ifo.roll->type(FL_HORIZONTAL);
        ifo.roll->align(FL_ALIGN_LEFT);
        ifo.roll->value(2048);
        ifo.roll->when(FL_WHEN_CHANGED | FL_WHEN_RELEASE);
        ifo.roll->callback(&roll_cb, &ifo);
        ifo.roll->bounds(0, 4095);
        ifo.roll->precision(0);
        y += 20;

        win->end();
        win->show();
        Fl::add_timeout(0.030, &timeout, &ifo);

        ti.infos.push_back(&ifo);
    }
    Fl::add_timeout(0.002, &step_usb, &ss);
    Fl_Window *main = new Fl_Window(10, 750, 300, 200, "Controls");
    ti.top_window = main;
    Fl_Button *btn = new Fl_Button(10, 10, 120, 24, "Save");
    btn->callback(&save_btn, &ti);
    btn = new Fl_Button(10, 166, 120, 24, "Quit");
    btn->callback(&quit_btn, &ti);
    main->end();
    main->show();
    try {
        Fl::run();
    }
    catch (std::exception const &x) {
        std::cerr << "exception: " << x.what() << std::endl;
        return 1;
    }
    std::cerr << "end of main()" << std::endl;
}

