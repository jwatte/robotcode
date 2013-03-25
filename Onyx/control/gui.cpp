
#include "gui.h"
#include "Image.h"
#include "mwscore.h"
#include "util.h"
#include <stdexcept>
#include <GL/freeglut.h>
#include <string.h>
#include <stdio.h>

static ITime *itime;
static GuiState g_state;
static int g_win = -1;
static bool to_upload = false;
static GLuint g_imgTex = 0;
static GLuint g_bubble = 0;
static GLuint g_ring = 0;
static GLuint g_vmeter = 0;
static GLuint g_bar = 0;
static GLuint g_crouch = 0;
static GLuint g_stretch = 0;
static GLuint g_noise = 0;

#define test_glerror() test_glerror_(__LINE__, __FILE__)
static MWScore gui_score;
static bool showing_score;

static void test_glerror_(int line, char const *file) {
    int r = glGetError();
    if (r != 0) {
        std::cerr << file << ":" << line << ": GL error: " << r << std::endl;
        throw std::runtime_error("Unexpected GL error");
    }
}

static void make_texture(GLuint *tex, int width, int height, bool alpha) {
    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, alpha ? GL_RGBA : GL_RGB, width, height, 0, alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    test_glerror();
}

void load_tga(char const *name, GLuint *tex) {
    FILE *f = fopen(name, "rb");
    if (!f) {
        throw std::runtime_error(std::string("Could not find file: ") + name);
    }
    unsigned short hdr[9];
    fread(hdr, 2, 9, f);
    int width = hdr[6];
    int height = hdr[7];
    unsigned char itype = hdr[1] & 0xff;
    if (itype != 2) {
        throw std::runtime_error(std::string("Only RGB TGA supported: ") + name);
    }
    bool alpha = (hdr[8] & 0x3f) == 0x20;
    unsigned char *bytes = new unsigned char[width * height * (alpha ? 4 : 3)];
    fread(bytes, alpha ? 4 : 3, width * height, f);
    fclose(f);
    make_texture(tex, width, height, alpha);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, bytes);
    delete[] bytes;
}

static void draw_vstring(char const *str, int left, int bot) {
    glLoadIdentity();
    glTranslatef(-left, -bot, 0);
    glScalef(0.1f, 0.1f, 0.1f);
    glLineWidth(1.0f);
    while (*str) {
        glutStrokeCharacter(GLUT_STROKE_ROMAN, *str);
        ++str;
    }
}

template<typename T> static void draw_val(char const *fmt, T val, int left, int bot) {
    char buf[128];
    sprintf(buf, fmt, val);
    glRasterPos2f(left, bot);
    char *ptr = buf;
    while (*ptr) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *ptr);
        ++ptr;
    }
    test_glerror();
}

void draw_sprite(GLuint texture, int sizex, int sizey, int x, int y) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0);
        glVertex2f(x, y+sizey);
        glTexCoord2f(0, 1);
        glVertex2f(x, y);
        glTexCoord2f(1, 0);
        glVertex2f(x+sizex, y+sizey);
        glTexCoord2f(1, 1);
        glVertex2f(x+sizex, y);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    test_glerror();
}

static void do_display() {
    double now = read_clock();
    test_glerror();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1280, 0, 720, 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    test_glerror();

    /* main video */

    if (to_upload) {
        if (!!g_state.image) {
            glBindTexture(GL_TEXTURE_2D, g_imgTex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_state.image->width(),
                g_state.image->height(), GL_RGB, GL_UNSIGNED_BYTE, 
                g_state.image->bits(FullBits));
            test_glerror();
        }
        to_upload = false;
    }
    if (!!g_state.image) {
        glBindTexture(GL_TEXTURE_2D, g_imgTex);
        glEnable(GL_TEXTURE_2D);
        glColor4f(1, 1, 1, 1);
        glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(0, 0);
            glVertex2f(0, 720);
            glTexCoord2f(0, g_state.image->height() / 1024.0f);
            glVertex2f(0, 0);
            glTexCoord2f(g_state.image->width() / 2048.0f, 0);
            glVertex2f(1280, 720);
            glTexCoord2f(g_state.image->width() / 2048.0f, g_state.image->height() / 1024.0f);
            glVertex2f(1280, 0);
        glEnd();
        //  if the image is old, pretend it has distortion
        if (g_state.image_old) {
            glBindTexture(GL_TEXTURE_2D, g_noise);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            int left = 1;   //  don't rand -- a static image is more clear about the problem
            int top = 1;
            glBegin(GL_TRIANGLE_STRIP);
                glTexCoord2f(left / 1024.0f, top / 1024.0f);
                glVertex2f(0, 720);
                glTexCoord2f(left / 1024.0f, (top + 720) / 1024.0f);
                glVertex2f(0, 0);
                glTexCoord2f((left + 1280) / 1024.0f, top / 1024.0f);
                glVertex2f(1280, 720);
                glTexCoord2f((left + 1280) / 1024.0f, (top + 720) / 1024.0f);
                glVertex2f(1280, 0);
            glEnd();
        }
        glDisable(GL_TEXTURE_2D);
        test_glerror();
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glColor4ub(255, 255, 255, 255);
    test_glerror();

    /* hitpoints */
    float hpscale = g_state.hitpoints / 24.0f;
    if (hpscale > 1) hpscale = 1;
    if (hpscale < 0) hpscale = 0;
    glColor4f(1 - hpscale, hpscale, 0, 1);
    for (int i = 0; i < 25; ++i) {
        draw_sprite(g_ring, 24, 24, 1200, 30 + i * 24);
    }
    for (int i = 0; i < g_state.hitpoints; ++i) {
        draw_sprite(g_bubble, 24, 24, 1199, 31 + i * 24);
    }

    //  network loss meter
    if (g_state.loss < 5) {
        glColor4ub(192, 224, 192, 192);
    }
    else if (g_state.loss < 25) {
        glColor4ub(224, 224, 160, 192);
    }
    else if (g_state.loss < 75) {
        glColor4ub(240, 192, 160, 192);
    }
    else {
        glColor4ub(255, 128, 128, 192);
    }
    draw_sprite(g_vmeter, 32, 64, 30, 630);
    draw_sprite(g_bar, 16, g_state.loss / 4 + 1, 38, 630);

    //  battery voltage meter
    unsigned char bat = g_state.battery;
    if (bat < 128) {
        bat = 128;
    }
    if (bat > 144) {
        //  green goodness
        glColor4ub(32, 128, 96, 128);
    }
    else if (bat > 137) {
        //  yellow warning
        if ((int)(fmod(now, 4.0) * 2) % 4) {
            glColor4ub(192, 192, 128, 192);
        }
        else {
            glColor4ub(192, 192, 128, 255);
        }
    }
    else {
        //  red alert!
        if ((int)(fmod(now, 4.0) * 10) % 4) {
            glColor4ub(192, 32, 32, 192);
        }
        else {
            glColor4ub(255, 96, 32, 255);
        }
    }
    draw_sprite(g_bar, (bat - 127) * 5, 22, 200, 680);

    /* text */
    glColor4f(0, 0, 0, 0.25f);
    draw_sprite(g_bar, 100, 32, 40, 22);
    draw_sprite(g_bar, 100, 32, 160, 22);
    if (showing_score) {
        draw_sprite(g_bar, 100, 100, 1080, 520);
        if (gui_score.scores.size() > 0) {
            int w = 1070 / gui_score.scores.size();
            int l = 0;
            char buf[25];
            sprintf(buf, "%.1f", gui_score.time);
            draw_vstring(buf, 110, 600);
            for (auto ptr(gui_score.scores.begin()), end(gui_score.scores.end()); ptr != end; ++ptr) {
                int h = 0;
                for (auto i((*ptr).second.begin()), e((*ptr).second.end()); i != e; ++i) {
                    draw_vstring((*i).first.c_str(), 110 + w * l, 500 - h * 20);
                    sprintf(buf, "%d", (*i).second);
                    draw_vstring(buf, 90 + w * l, 600 - h * 20); 
                    h++;
                }
                l++;
            }
        }
    }
    glColor4f(0.8f, 0.8f, 0.8f, 0.8f);
    draw_val("%.01f V", bat / 10.0, 200, 684);
    glColor4f(0.8f, 0.8f, 0.8f, 0.8f);
    draw_sprite(g_crouch, 32, 32, 160, 22);
    draw_sprite(g_stretch, 32, 32, 228, 22);
    glColor4f(0.8f, 0.8f, 1.0f, 1.0f);
    draw_val("%.01f", g_state.trot, 80, 30);
    draw_val("%d", g_state.pose, 200, 30);

    glutSwapBuffers();
}

void open_gui(GuiState const &state, ITime *it) {
    itime = it;
    g_state = state;
    if (g_state.image) {
        to_upload = true;
    }
    if (g_win == -1) {
        glutInitWindowSize(1280, 720);
        int argc = 0;
        char *argv[1] = { 0 };
        glutInit(&argc, argv);
        g_win = glutCreateWindow("Robot Control");
        make_texture(&g_imgTex, 2048, 1024, false);
        load_tga("data/bubble-32.tga", &g_bubble);
        load_tga("data/ring-32.tga", &g_ring);
        load_tga("data/vmeter-64.tga", &g_vmeter);
        load_tga("data/bar-16.tga", &g_bar);
        load_tga("data/crouch-32.tga", &g_crouch);
        load_tga("data/stretch-32.tga", &g_stretch);
        load_tga("data/noise-256.tga", &g_noise);
        glutSetWindow(g_win);
        glutDisplayFunc(&do_display);
        glutShowWindow();
    }
}

void update_gui(GuiState const &state) {
    to_upload = true;
    g_state = state;
    if (g_win != -1) {
        glutPostWindowRedisplay(g_win);
    }
}

void step_gui() {
    glutMainLoopEvent();
}

void close_gui() {
    glutDestroyWindow(g_win);
    g_win = -1;
}

void show_gui_score(MWScore &score) {
    showing_score = true;
    gui_score = score;
}

void hide_gui_score() {
    showing_score = false;
}


