
#include "gui.h"
#include "Image.h"
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

#define test_glerror() test_glerror_(__LINE__, __FILE__)

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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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

void draw_sprite(GLuint texture, int size, int x, int y) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0);
        glVertex2f(x, y+size);
        glTexCoord2f(0, 1);
        glVertex2f(x, y);
        glTexCoord2f(1, 0);
        glVertex2f(x+size, y+size);
        glTexCoord2f(1, 1);
        glVertex2f(x+size, y);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    test_glerror();
}

static void do_display() {
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
        glDisable(GL_TEXTURE_2D);
        test_glerror();
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glColor4ub(255, 255, 255, 255);
    test_glerror();
    float hpscale = g_state.hitpoints / 24.0f;
    if (hpscale > 1) hpscale = 1;
    if (hpscale < 0) hpscale = 0;
    glColor4f(1 - hpscale, hpscale, 0, 1);
    for (int i = 0; i < 25; ++i) {
        draw_sprite(g_ring, 24, 1200, 30 + i * 24);
    }
    for (int i = 0; i < g_state.hitpoints; ++i) {
        draw_sprite(g_bubble, 24, 1200, 30 + i * 24);
    }
    glDisable(GL_BLEND);
    glColor4f(0.8f, 1.0f, 0.8f, 1.0f);
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



