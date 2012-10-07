
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <boost/shared_ptr.hpp>
#include <string>
#include <map>
#include <sstream>
#include <boost/lexical_cast.hpp>


class IWindow {
public:
    virtual void display() = 0;
    virtual bool special(int key, int x, int y) = 0;
    virtual bool mouse(int button, int state, int x, int y) = 0;
    virtual bool motion(int x, int y) = 0;

    virtual inline ~IWindow() {}
};

struct xyz {
    xyz() : x(0), y(0), z(0) {}
    float x, y, z;
};

std::map<int, boost::shared_ptr<IWindow>> gWindows;

#define assertGL(x) _assertGL(__FILE__, __LINE__)

void _assertGL(char const *file, int line) {
    int err = glGetError();
    if (!err) {
        return;
    }
    std::cerr << file << ":" << line << ": assertGL() failed: 0x" << std::hex << err << std::endl;
    exit(1);
}

static inline float absf(float a) {
    return a < 0 ? -a : a;
}

void win_display() {
    assertGL();
    glClearColor(0.1f, 0.15f, 0.2f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    gWindows[glutGetWindow()]->display();

    glutSwapBuffers();
}

void win_special(int key, int x, int y) {
    if (gWindows[glutGetWindow()]->special(key, x, y)) {
        glutPostRedisplay();
    }
}

void win_motion(int x, int y) {
    if (gWindows[glutGetWindow()]->motion(x, y)) {
        glutPostRedisplay();
    }
}

void win_mouse(int button, int state, int x, int y) {
    if (gWindows[glutGetWindow()]->mouse(button, state, x, y)) {
        glutPostRedisplay();
    }
}

void fatal_error(char const *str, ...) {
    va_list args;
    va_start(args, str);
    vfprintf(stderr, str, args);
    va_end(args);
    exit(1);
}

class TapWindow : public IWindow {
public:
    TapWindow(char const *text, size_t len, char const *title, int win) :
        text_(text),
        len_(len),
        title_(title),
        win_(win),
        parsed_(false),
        error_(false),
        maxline_(0),
        tracking_(0) {
        center_.x = 150.0f;
        center_.y = 10.0f;
        rotate_.y = 30.0f;
    }
    ~TapWindow() {
        free(const_cast<char *>(text_));
    }

    char const *text_;
    size_t len_;
    std::string title_;
    int win_;
    bool parsed_;
    bool error_;
    std::string message_;
    struct vertex {
        float left;
        float bottom;
        float yon;
        unsigned int color;
        int line;
    };
    std::vector<vertex> vbuf_;
    int maxline_;
    xyz center_;
    xyz rotate_;
    xyz *tracking_;
    xyz trackingPos_;
    xyz trackingScale_;

    struct mode {
        mode() :
            gMode(0),
            feed(1),
            speed(1000),
            arc_plane(17),
            x(0),
            y(0),
            z(0), 
            inc_xyz(false),
            inc_ijk(false),
            spindle(5),
            scale(25.6) {
        }
        int gMode;
        int feed;
        int speed;
        int arc_plane;
        float x;
        float y;
        float z;
        float i;
        float j;
        float k;
        bool inc_xyz;
        bool inc_ijk;
        int spindle;
        float scale;
    };

    std::vector<std::string> g_lines_;

    void add_line(float x0, float y0, float z0, float x1, float y1, float z1, unsigned int color, int line) {
        vertex v0 = { x0, y0, z0, color, line };
        vertex v1 = { x1, y1, z1, color, line };
        vbuf_.push_back(v0);
        vbuf_.push_back(v1);
    }

    void add_circle(float x0, float y0, float z0, float x1, float y1, float z1, 
        int arc_plane, float direction, float i, float j, float k, unsigned int color, int line) {
        switch (arc_plane) {
            case 17:    k = z0; break;
            case 18:    j = y0; break;
            case 19:    i = x0; break;
        }
        float r1 = sqrtf((i - x0) * (i - x0) + (j - y0) * (j - y0) + (k - z0) * (k - z0));
        float r2 = sqrtf((i - x1) * (i - x1) + (j - y1) * (j - y1) + (k - z1) * (k - z1));
        fprintf(stderr, "x0:%f x1:%f y0:%f y1:%f z0:%f z1:%f i:%f j:%f k:%f r1:%f, r2:%f\n", 
            x0, x1, y0, y1, z0, z1, i, j, k, r1, r2);
        if (absf(r1 - r2) > 0.003f) {
            error_ = true;
            std::stringstream ss;
            ss << title_ << ":" << line << ": bad arc stop position: " << "\n" << 
                "X" << x0 << "Y" << y0 << "Z" << z0 << "\n" <<
                "X" << x1 << "Y" << y1 << "Z" << z1 << " (radius delta = " << absf(r1 - r2) << ")";
            message_ = ss.str();
            return;
        }
        float from = 0, to = 0;
        switch (arc_plane) {
        case 17:
            from = atan2f(j - y0, i - x0);
            to = atan2f(j - y1, i - x1);
            emit_arc_xy(i, j, k, r1, from, to, direction, color, line);
            break;
        case 18:
            from = atan2f(i - x0, k - z0);
            to = atan2f(i - x1, k - z1);
            emit_arc_zx(i, j, k, r1, from, to, direction, color, line);
            break;
        case 19:
            from = atan2f(j - y0, i - x0);
            to = atan2f(j - y1, i - x1);
            emit_arc_yz(i, j, k, r1, from, to, direction, color, line);
            break;
        default:
            assert(!"bad arc plane in arc");
        }
    }

    static void setup(float &from, float &to, float &direction, int &steps, float r) {
        if (direction == 1) {   //  counterclockwise
            if (to <= from) {
                to += 2 * M_PI;
            }
            if (to > 2 * M_PI) {
                to -= 2 * M_PI;
                from -= 2 * M_PI;
            }
        }
        else {
            if (from <= to) {
                from += 2 * M_PI;
            }
            if (from > 2 * M_PI) {
                from -= 2 * M_PI;
                to -= 2 * M_PI;
            }
        }
        fprintf(stderr, "from %f  to %f  direction %f\n", from, to, direction);
        assert(from * direction <= to * direction);
        assert(from >= -2 * M_PI - 0.001f);
        assert(from <= 2 * M_PI + 0.001f);
        assert(to >= -2 * M_PI - 0.001f);
        assert(to <= 2 * M_PI + 0.001f);
        assert(absf(from - to) <= 2 * M_PI + 0.001f);
        steps = std::min(200, std::max(int(absf(from - to) * r), 4));
    }

    void emit_arc_xy(float i, float j, float k, float r, 
        float from, float to, float direction, float color, int line) {
        int steps = 0;
        setup(from, to, direction, steps, r);
        float x = i + cosf(from) * r;
        float y = j + sinf(from) * r;
        float dd = (to - from) / steps;
        for (int t = 0; t < steps; ++t) {
            from += dd;
            float nx = i + cosf(from) * r;
            float ny = j + sinf(from) * r;
            add_line(x, y, k, nx, ny, k, color, line);
            x = nx;
            y = ny;
        }
    }

    void emit_arc_zx(float i, float j, float k, float r, 
        float from, float to, float direction, float color, int line) {
        int steps = 0;
        setup(from, to, direction, steps, r);
        fprintf(stderr, "xz(%f, %f, %d)\n", from, to, steps);
        float z = k + cosf(from) * r;
        float x = i + sinf(from) * r;
        float dd = (to - from) / steps;
        for (int t = 0; t < steps; ++t) {
            from += dd;
            float nz = k + cosf(from) * r;
            float nx = i + sinf(from) * r;
            add_line(x, j, z, nx, j, nz, color, line);
            x = nx;
            z = nz;
        }
    }

    void emit_arc_yz(float i, float j, float k, float r, 
        float from, float to, float direction, float color, int line) {
        int steps = 0;
        setup(from, to, direction, steps, r);
        float y = j + cosf(from) * r;
        float z = k + sinf(from) * r;
        float dd = (to - from) / steps;
        for (int t = 0; t < steps; ++t) {
            from += dd;
            float ny = j + cosf(from) * r;
            float nz = k + sinf(from) * r;
            add_line(i, y, z, i, ny, nz, color, line);
            y = ny;
            z = nz;
        }
    }

    void draw_text(int left, int bottom, void *font, std::string const& msg) {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 800, 0, 600, -1000, 1000);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glColor4f(1.0f, 0.75f, 0.0f, 1.0f);
        glRasterPos3i(left, bottom, 0);
        glDisable(GL_DEPTH_TEST);
        for (auto ptr = msg.begin(), end = msg.end();
            ptr != end; ++ptr) {
            if (*ptr == '\n') {
                bottom = bottom - 18;
                glRasterPos3i(left, bottom, 0);
            }
            else {
                glutBitmapCharacter(font, (unsigned char)*ptr);
            }
        }
    }

    void parse(char const *data, size_t len) {
        error_ = false;
        parsed_ = true;
        int line_no = 1;
        char const *end = data + len;
        mode m;
        char const *line_start = data;
        while (data < end && !error_) {
            char const *start = data;
            while (data < end && *data == 32) {
                ++data;
            }
            while (data < end && *data != 10 && *data != 13) {
                ++data;
            }
            std::string line(start, data);
            int capline = line_no;
            while (data < end && (*data == 10 || *data == 13)) {
                if (*data == 10) {
                    g_lines_.push_back(std::string(line_start, data));
                    line_start = data + 1;
                    ++line_no;
                }
                ++data;
            }
            try {
                parse_line(line, m, capline);
            }
            catch (std::exception const &x) {
                error_ = true;
                std::stringstream ss;
                ss << title_ << ":" << capline << ": error parsing: " << x.what() << "\n"
                    << line;
                message_ = ss.str();
                break;
            }
        }
        maxline_ = g_lines_.size();
    }

    void parse_line(std::string const &str, mode &m, int line_no) {
        if (str.empty()) {
            return;
        }
        mode nm(m);
        auto data = str.begin(), end = str.end();
        while (data != end) {
            if (*data == '(') {
                //  comment
                break;
            }
            else if (*data == '%') {
                //  start
                break;
            }
            else if (*data == ' ') {
                //  ignore
            }
            else if (*data >= 'A' && *data <= 'Z') {
                //  code
                char code = *data;
                ++data;
                auto start(data);
                while (data != end) {
                    if (*data != '.' && *data != '-' && *data != ' ' &&
                        ((*data < '0') || (*data > '9'))) {
                        break;
                    }
                    if (start == data && *data == ' ') {
                        ++start;
                    }
                    ++data;
                }
                std::string arg(start, data);
                if (arg.empty()) {
                    error_ = true;
                    std::stringstream ss;
                    ss << title_ << ":" << line_no << ": no argument for code '" <<
                        (char)code << "'" << "\n" << str;
                    message_ = ss.str();
                    break;
                }
                --data;
                if (!expand_code(code, arg, nm, m)) {
                    error_ = true;
                    std::stringstream ss;
                    ss << title_ << ":" << line_no << ": unknown code (" << (int)*data << 
                        ") '" << *data << "'" << "\n" << str;
                    message_ = ss.str();
                    break;
                }
            }
            else {
                error_ = true;
                std::stringstream ss;
                ss << title_ << ":" << line_no << ": unknown character (" << (int)*data << 
                    ") '" << *data << "'" << "\n" << str;
                message_ = ss.str();
                break;
            }
            ++data;
        }
        if (!error_ &&
            ((m.x != nm.x || m.y != nm.y || m.z != nm.z) ||
                (((nm.gMode == 2) || (nm.gMode == 3)) &&
                    (nm.i != m.i || nm.j != m.j || nm.k != m.k)))) {
            if (nm.inc_ijk) {
                nm.i += m.x;
                nm.j += m.y;
                nm.k += m.z;
            }
            switch (nm.gMode) {
                case 0:
                    add_line(m.x, m.y, m.z, nm.x, nm.y, nm.z, 0xffffffff, line_no);
                    break;
                case 1:
                    add_line(m.x, m.y, m.z, nm.x, nm.y, nm.z, 0xff00ffff, line_no);
                    break;
                case 2:
                    add_circle(m.x, m.y, m.z, nm.x, nm.y, nm.z, nm.arc_plane, 1, nm.i, nm.j, nm.k, 
                        0xff00ff00, line_no);
                    break;
                case 3:
                    add_circle(m.x, m.y, m.z, nm.x, nm.y, nm.z, nm.arc_plane, -1, nm.i, nm.j, nm.k, 
                        0xffff0000, line_no);
                    break;
            }
        }
        m = nm;
        m.i = m.j = m.k= 0;
    }

    bool expand_code(char code, std::string const &arg, mode &nm, mode const &im) {
        switch (code) {
            case 'G':
                if (arg == "91.1") {
                    nm.inc_ijk = true;
                }
                else if (arg == "90.1") {
                    nm.inc_ijk = false;
                }
                else {
                    int aint = boost::lexical_cast<int>(arg);
                    switch (aint) {
                    case 0: case 1: case 2: case 3:
                        nm.gMode = aint;
                        break;
                    case 4:
                        //  dwell
                        break;
                    case 90:
                        nm.inc_ijk = false;
                        break;
                    case 91:
                        nm.inc_ijk = true;
                        break;
                    case 94:
                        //  feedrate per minute
                        break;
                    case 97:
                        //  constant spindle speed
                        break;
                    case 17: case 18: case 19:
                        nm.arc_plane = aint;
                        break;
                    case 20:    //  inches
                        nm.scale = 25.6f;
                        break;
                    case 21:    //  millimeters
                        nm.scale = 1.0f;
                        break;
                    case 28:
                        nm.x = nm.y = nm.z = 0;
                        nm.gMode = 0;
                        break;
                    default:
                        std::cerr << "unknown G mode: " << arg << std::endl;
                    }
                }
                break;
            case 'M':
                {
                    int aint = boost::lexical_cast<int>(arg);
                    switch (aint) {
                    case 0: case 1:
                        //  stops
                        break;
                    case 3: case 4: case 5:
                        nm.spindle = aint;
                        break;
                    case 7: case 8: case 9:
                        //  coolant
                        break;
                    case 13:
                        nm.spindle = 3;
                        break;
                    case 30:
                        //  end of program: how to signal?
                        nm.x = nm.y = nm.z = 0;
                        nm.i = nm.j = nm.k = 0;
                        nm.gMode = 0;
                        nm.spindle = 5;
                        break;
                    }
                }
                break;
            case 'F':
                nm.feed = boost::lexical_cast<float>(arg);
                break;
            case 'S':
                nm.speed = boost::lexical_cast<float>(arg);
                break;
            case 'X':
                if (im.inc_xyz) {
                    nm.x = im.x + boost::lexical_cast<float>(arg) * im.scale;
                }
                else {
                    nm.x = boost::lexical_cast<float>(arg) * im.scale;
                }
                break;
            case 'Y':
                if (im.inc_xyz) {
                    nm.y = im.y + boost::lexical_cast<float>(arg) * im.scale;
                }
                else {
                    nm.y = boost::lexical_cast<float>(arg) * im.scale;
                }
                break;
            case 'Z':
                if (im.inc_xyz) {
                    nm.z = im.z + boost::lexical_cast<float>(arg) * im.scale;
                }
                else {
                    nm.z = boost::lexical_cast<float>(arg) * im.scale;
                }
                break;
            case 'I':
                nm.i = boost::lexical_cast<float>(arg) * im.scale;
                break;
            case 'J':
                nm.j = boost::lexical_cast<float>(arg) * im.scale;
                break;
            case 'K':
                nm.k = boost::lexical_cast<float>(arg) * im.scale;
                break;
            default:
                return false;
        }
        return true;
    }

    virtual bool special(int key, int x, int y) {
        switch (key) {
            case GLUT_KEY_HOME:
                maxline_ = 1;
                break;
            case GLUT_KEY_END:
                maxline_ = g_lines_.size();
                break;
            case GLUT_KEY_PAGE_UP:
                maxline_ = (maxline_ > 10 ? maxline_ - 10 : 1);
                break;
            case GLUT_KEY_PAGE_DOWN:
                maxline_ = maxline_ + 10;
                break;
            case GLUT_KEY_UP:
                if (maxline_ > 1) {
                    maxline_ -= 1;
                }
                break;
            case GLUT_KEY_DOWN:
                maxline_ += 1;
                break;
            default:
                return false;
        }
        if (maxline_ > g_lines_.size()) {
            maxline_ = g_lines_.size();
        }
        return true;
    }

    virtual bool mouse(int button, int state, int x, int y) {
        if (state == GLUT_UP) {
            tracking_ = 0;
        }
        else if (button == GLUT_LEFT_BUTTON) {
            tracking_ = &rotate_;
            trackingScale_.x = -0.1f;
            trackingScale_.y = -0.1f;
            trackingScale_.z = 0.1f;
        }
        else {
            tracking_ = &center_;
            trackingScale_.x = -1.0f;
            trackingScale_.y = 1.0f;
            trackingScale_.z = 1.0f;
        }
        trackingPos_.x = x;
        trackingPos_.y = y;
        return false;
    }

    virtual bool motion(int x, int y) {
        if (tracking_) {
            tracking_->x += (x - trackingPos_.x) * trackingScale_.x;
            tracking_->y += (y - trackingPos_.y) * trackingScale_.y;
        }
        trackingPos_.x = x;
        trackingPos_.y = y;
        return true;
    }

    virtual void display() {
        if (!parsed_) {
            parse(text_, len_);
        }
        if (error_) {
            draw_text(10, 500, GLUT_BITMAP_HELVETICA_18, message_);
        }
        else {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glFrustum(-1.0f, 1.0f, -0.75f, 0.75f, 1.5f, 1000.0f);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glTranslatef(0.0f, 0.0f, -200.0f);
            glRotatef(-rotate_.y, 1.0f, 0.0f, 0.0f);
            glRotatef(-rotate_.z, 0.0f, 1.0f, 0.0f);
            glRotatef(-rotate_.x, 0.0f, 0.0f, 1.0f);
            glTranslatef(-center_.x, -center_.y, -center_.z);

            assertGL();

            glBegin(GL_LINES);
            for (auto ptr(vbuf_.begin()), end(vbuf_.end());
                ptr != end; ++ptr) {
                if ((*ptr).line <= maxline_) {
                    glColor4ubv((GLubyte *)&(*ptr).color);
                    glVertex3fv(&(*ptr).left);
                }
            }
            glEnd();

            assertGL();

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            glColor4f(0.25f, 0.25f, 0.25f, 0.5f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(0.0f, 0.0f, 0.0f);
            glVertex3f(256.0f, 0.0f, 0.0f);
            glVertex3f(256.0f, 25.6f, 0.0f);
            glVertex3f(0.0f, 25.6f, 0.0f);
            glEnd();
            glColor4f(0.125f, 0.125f, 0.125f, 0.5f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(0.0f, 0.0f, 0.0f);
            glVertex3f(0.0f, 0.0f, -25.6f);
            glVertex3f(256.0f, 0.0f, -25.6f);
            glVertex3f(256.0f, 0.0f, 0.0f);
            glEnd();

            assertGL();

            if (maxline_ < g_lines_.size()) {
                std::stringstream ss;
                ss << maxline_;
                ss << " ";
                ss << g_lines_[maxline_ - 1];
                draw_text(10, 580, GLUT_BITMAP_8_BY_13, ss.str());
            }
        }
    }
};

void load_tap(char const *name) {
    FILE *f = fopen(name, "rb");
    if (!f) {
        fatal_error("File not found: %s\n", name);
    }
    fseek(f, 0, 2);
    long l = ftell(f);
    if (l > 16 * 1024 * 1024 || l < 0) {
        fatal_error("File is too large: %s\n", name);
    }
    fseek(f, 0, 0);
    char *buf = (char *)malloc(l + 1);
    fread(buf, 1, l, f);
    buf[l] = 0;
    fclose(f);

    int win = glutCreateWindow(name);
    glutDisplayFunc(&win_display);
    glutSpecialFunc(&win_special);
    glutMouseFunc(&win_mouse);
    glutMotionFunc(&win_motion);
    boost::shared_ptr<IWindow> ptr(new TapWindow(buf, (size_t)l, name, win));
    gWindows[win] = ptr;
}

int main(int argc, char const *argv[]) {
    glutInitWindowPosition(20, 30);
    glutInitWindowSize(800, 600);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInit(&argc, const_cast<char **>(argv));
    ++argv;

    int nloaded = 0;
    while (argv[0] != NULL) {
        load_tap(argv[0]);
        ++argv;
        ++nloaded;
    }

    if (!nloaded) {
        fprintf(stderr, "usage: vgc [glut options] filename.tap\n");
        return 1;
    }

    glutMainLoop();

    return 0;
}

