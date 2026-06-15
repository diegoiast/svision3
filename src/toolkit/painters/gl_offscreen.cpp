// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#if defined(__APPLE__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <OpenGL/gl.h>
#pragma clang diagnostic pop
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <GL/gl.h>
#include <windows.h>
#else
#include <GL/gl.h>
#endif

#include "gl_offscreen.hpp"
#include "gl_painter.hpp"
#include <algorithm>
#include <cstring>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_RGBA8 0x8058
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif

namespace toolkit {

namespace {

struct FboFuncs {
    using GenFB = void (*)(GLsizei, GLuint *);
    using DelFB = void (*)(GLsizei, const GLuint *);
    using BindFB = void (*)(GLenum, GLuint);
    using GenRB = void (*)(GLsizei, GLuint *);
    using DelRB = void (*)(GLsizei, const GLuint *);
    using BindRB = void (*)(GLenum, GLuint);
    using RBStore = void (*)(GLenum, GLenum, GLsizei, GLsizei);
    using FBRB = void (*)(GLenum, GLenum, GLenum, GLuint);
    using CheckFB = GLenum (*)(GLenum);

    GenFB genFramebuffers = nullptr;
    DelFB deleteFramebuffers = nullptr;
    BindFB bindFramebuffer = nullptr;
    GenRB genRenderbuffers = nullptr;
    DelRB deleteRenderbuffers = nullptr;
    BindRB bindRenderbuffer = nullptr;
    RBStore renderbufferStorage = nullptr;
    FBRB framebufferRenderbuffer = nullptr;
    CheckFB checkFramebufferStatus = nullptr;
    bool ok = false;

    static void *load(const char *name) {
#ifdef _WIN32
        void *p = reinterpret_cast<void *>(wglGetProcAddress(name));
        if (!p) {
            HMODULE lib = GetModuleHandleW(L"opengl32.dll");
            if (lib) {
                p = reinterpret_cast<void *>(GetProcAddress(lib, name));
            }
        }
        return p;
#else
        return dlsym(RTLD_DEFAULT, name);
#endif
    }

    void init() {
        if (ok) {
            return;
        }
        genFramebuffers = reinterpret_cast<GenFB>(load("glGenFramebuffers"));
        deleteFramebuffers = reinterpret_cast<DelFB>(load("glDeleteFramebuffers"));
        bindFramebuffer = reinterpret_cast<BindFB>(load("glBindFramebuffer"));
        genRenderbuffers = reinterpret_cast<GenRB>(load("glGenRenderbuffers"));
        deleteRenderbuffers = reinterpret_cast<DelRB>(load("glDeleteRenderbuffers"));
        bindRenderbuffer = reinterpret_cast<BindRB>(load("glBindRenderbuffer"));
        renderbufferStorage = reinterpret_cast<RBStore>(load("glRenderbufferStorage"));
        framebufferRenderbuffer = reinterpret_cast<FBRB>(load("glFramebufferRenderbuffer"));
        checkFramebufferStatus = reinterpret_cast<CheckFB>(load("glCheckFramebufferStatus"));
        ok = genFramebuffers && deleteFramebuffers && bindFramebuffer && genRenderbuffers &&
             deleteRenderbuffers && bindRenderbuffer && renderbufferStorage &&
             framebufferRenderbuffer && checkFramebufferStatus;
    }
};

static FboFuncs fbo;

} // namespace

void gl_render_to_buffer(int w, int h, float scale, TextRasterizer *rasterizer, void *dst,
                         std::function<void(Painter &)> fn) {
    const size_t total = static_cast<size_t>(w) * h * 4;
    fbo.init();
    if (!fbo.ok) {
        std::memset(dst, 0, total);
        return;
    }

    GLuint fb, rb;
    fbo.genFramebuffers(1, &fb);
    fbo.bindFramebuffer(GL_FRAMEBUFFER, fb);
    fbo.genRenderbuffers(1, &rb);
    fbo.bindRenderbuffer(GL_RENDERBUFFER, rb);
    fbo.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);
    fbo.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);

    if (fbo.checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fbo.bindFramebuffer(GL_FRAMEBUFFER, 0);
        fbo.deleteRenderbuffers(1, &rb);
        fbo.deleteFramebuffers(1, &fb);
        std::memset(dst, 0, total);
        return;
    }

    float lw = static_cast<float>(w) / scale;
    float lh = static_cast<float>(h) / scale;

    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, lw, lh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLPainter painter(lh, scale, rasterizer);
    fn(painter);
    glFlush();

    glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, dst);

    // OpenGL reads bottom-to-top; flip to top-to-bottom in-place
    auto *bytes = static_cast<uint8_t *>(dst);
    const int row = w * 4;
    for (int y = 0; y < h / 2; ++y) {
        std::swap_ranges(bytes + y * row, bytes + (y + 1) * row, bytes + (h - 1 - y) * row);
    }

    fbo.bindFramebuffer(GL_FRAMEBUFFER, 0);
    fbo.deleteRenderbuffers(1, &rb);
    fbo.deleteFramebuffers(1, &fb);
}

} // namespace toolkit
