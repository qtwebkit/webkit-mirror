/*
 * Copyright (C) 2012 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GLContextEGL.h"

#if USE(EGL)

#include "GraphicsContext3D.h"

#if USE(CAIRO)
#include <cairo.h>
#endif

#if USE(OPENGL_ES_2)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include "OpenGLShims.h"
#endif

#if PLATFORM(X11)
#include "PlatformDisplayX11.h"
#include "XErrorTrapper.h"
#include "XUniquePtr.h"
#include <X11/Xlib.h>
#endif

#if PLATFORM(WAYLAND)
#include "PlatformDisplayWayland.h"
#include <wayland-egl.h>
#endif

#if ENABLE(ACCELERATED_2D_CANVAS)
// cairo-gl.h includes some definitions from GLX that conflict with
// the ones provided by us. Since GLContextEGL doesn't use any GLX
// functions we can safely disable them.
#undef CAIRO_HAS_GLX_FUNCTIONS
#include <cairo-gl.h>
#endif

namespace WebCore {

static const EGLint gContextAttributes[] = {
#if USE(OPENGL_ES_2)
    EGL_CONTEXT_CLIENT_VERSION, 2,
#endif
    EGL_NONE
};

#if USE(OPENGL_ES_2)
static const EGLenum gEGLAPIVersion = EGL_OPENGL_ES_API;
#else
static const EGLenum gEGLAPIVersion = EGL_OPENGL_API;
#endif

bool GLContextEGL::getEGLConfig(EGLDisplay display, EGLConfig* config, EGLSurfaceType surfaceType)
{
    EGLint attributeList[] = {
#if USE(OPENGL_ES_2)
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#else
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
#endif
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_STENCIL_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_NONE,
        EGL_NONE
    };

    switch (surfaceType) {
    case GLContextEGL::PbufferSurface:
        attributeList[13] = EGL_PBUFFER_BIT;
        break;
    case GLContextEGL::PixmapSurface:
        attributeList[13] = EGL_PIXMAP_BIT;
        break;
    case GLContextEGL::WindowSurface:
    case GLContextEGL::Surfaceless:
        attributeList[13] = EGL_WINDOW_BIT;
        break;
    }

    EGLint numberConfigsReturned;
    return eglChooseConfig(display, attributeList, config, 1, &numberConfigsReturned) && numberConfigsReturned;
}

std::unique_ptr<GLContextEGL> GLContextEGL::createWindowContext(EGLNativeWindowType window, PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    EGLDisplay display = platformDisplay.eglDisplay();
    EGLConfig config;
    if (!getEGLConfig(display, &config, WindowSurface))
        return nullptr;

    EGLContext context = eglCreateContext(display, config, sharingContext, gContextAttributes);
    if (context == EGL_NO_CONTEXT)
        return nullptr;

    EGLSurface surface = eglCreateWindowSurface(display, config, window, 0);
    if (surface == EGL_NO_SURFACE) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    return std::unique_ptr<GLContextEGL>(new GLContextEGL(platformDisplay, context, surface, WindowSurface));
}

std::unique_ptr<GLContextEGL> GLContextEGL::createPbufferContext(PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    EGLDisplay display = platformDisplay.eglDisplay();
    EGLConfig config;
    if (!getEGLConfig(display, &config, PbufferSurface))
        return nullptr;

    EGLContext context = eglCreateContext(display, config, sharingContext, gContextAttributes);
    if (context == EGL_NO_CONTEXT)
        return nullptr;

    static const int pbufferAttributes[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLSurface surface = eglCreatePbufferSurface(display, config, pbufferAttributes);
    if (surface == EGL_NO_SURFACE) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    return std::unique_ptr<GLContextEGL>(new GLContextEGL(platformDisplay, context, surface, PbufferSurface));
}

std::unique_ptr<GLContextEGL> GLContextEGL::createSurfacelessContext(PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    EGLDisplay display = platformDisplay.eglDisplay();
    if (display == EGL_NO_DISPLAY)
        return nullptr;

    const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
    if (!GLContext::isExtensionSupported(extensions, "EGL_KHR_surfaceless_context") && !GLContext::isExtensionSupported(extensions, "EGL_KHR_surfaceless_opengl"))
        return nullptr;

    EGLConfig config;
    if (!getEGLConfig(display, &config, Surfaceless))
        return nullptr;

    EGLContext context = eglCreateContext(display, config, sharingContext, gContextAttributes);
    if (context == EGL_NO_CONTEXT)
        return nullptr;

    return std::unique_ptr<GLContextEGL>(new GLContextEGL(platformDisplay, context, EGL_NO_SURFACE, Surfaceless));
}

#if PLATFORM(X11)
std::unique_ptr<GLContextEGL> GLContextEGL::createPixmapContext(PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    EGLDisplay display = platformDisplay.eglDisplay();
    EGLConfig config;
    if (!getEGLConfig(display, &config, PixmapSurface))
        return nullptr;

    EGLContext context = eglCreateContext(display, config, sharingContext, gContextAttributes);
    if (context == EGL_NO_CONTEXT)
        return nullptr;

    EGLint visualId;
    if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &visualId)) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    Display* x11Display = downcast<PlatformDisplayX11>(platformDisplay).native();

    XVisualInfo visualInfo;
    visualInfo.visualid = visualId;
    int numVisuals = 0;
    XUniquePtr<XVisualInfo> visualInfoList(XGetVisualInfo(x11Display, VisualIDMask, &visualInfo, &numVisuals));
    if (!visualInfoList || !numVisuals) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    // We are using VisualIDMask so there must be only one.
    ASSERT(numVisuals == 1);
    XUniquePixmap pixmap = XCreatePixmap(x11Display, DefaultRootWindow(x11Display), 1, 1, visualInfoList.get()[0].depth);
    if (!pixmap) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    // Some drivers fail to create the surface producing BadDrawable X error and the default XError handler normally aborts.
    // However, if the X error is ignored, eglCreatePixmapSurface() ends up returning a surface and we can continue creating
    // the context. Since this is an offscreen context, it doesn't matter if the pixmap used is not valid because we never do
    // swap buffers. So, we use a custom XError handler here that ignores BadDrawable errors and only warns about any other
    // errors without aborting in any case.
    XErrorTrapper trapper(x11Display, XErrorTrapper::Policy::Warn, { BadDrawable });
    EGLSurface surface = eglCreatePixmapSurface(display, config, reinterpret_cast<EGLNativePixmapType>(pixmap.get()), 0);
    if (surface == EGL_NO_SURFACE) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    return std::unique_ptr<GLContextEGL>(new GLContextEGL(platformDisplay, context, surface, WTFMove(pixmap)));
}
#endif // PLATFORM(X11)

#if PLATFORM(WAYLAND)
std::unique_ptr<GLContextEGL> GLContextEGL::createWaylandContext(PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    EGLDisplay display = platformDisplay.eglDisplay();
    EGLConfig config;
    if (!getEGLConfig(display, &config, WindowSurface))
        return nullptr;

    EGLContext context = eglCreateContext(display, config, sharingContext, gContextAttributes);
    if (context == EGL_NO_CONTEXT)
        return nullptr;

    WlUniquePtr<struct wl_surface> wlSurface(downcast<PlatformDisplayWayland>(platformDisplay).createSurface());
    if (!wlSurface) {
        eglDestroyContext(display, context);
        return nullptr;
    }

    EGLNativeWindowType window = wl_egl_window_create(wlSurface.get(), 1, 1);
    EGLSurface surface = eglCreateWindowSurface(display, config, window, 0);
    if (surface == EGL_NO_SURFACE) {
        eglDestroyContext(display, context);
        wl_egl_window_destroy(window);
        return nullptr;
    }

    return std::unique_ptr<GLContextEGL>(new GLContextEGL(platformDisplay, context, surface, WTFMove(wlSurface), window));
}
#endif

std::unique_ptr<GLContextEGL> GLContextEGL::createContext(EGLNativeWindowType window, PlatformDisplay& platformDisplay)
{
    if (platformDisplay.eglDisplay() == EGL_NO_DISPLAY)
        return nullptr;

    if (eglBindAPI(gEGLAPIVersion) == EGL_FALSE)
        return nullptr;

    EGLContext eglSharingContext = platformDisplay.sharingGLContext() ? static_cast<GLContextEGL*>(platformDisplay.sharingGLContext())->m_context : EGL_NO_CONTEXT;
    auto context = window ? createWindowContext(window, platformDisplay, eglSharingContext) : nullptr;
    if (!context)
        context = createSurfacelessContext(platformDisplay, eglSharingContext);
    if (!context) {
#if PLATFORM(X11)
        if (platformDisplay.type() == PlatformDisplay::Type::X11)
            context = createPixmapContext(platformDisplay, eglSharingContext);
#endif
#if PLATFORM(WAYLAND)
        if (platformDisplay.type() == PlatformDisplay::Type::Wayland)
            context = createWaylandContext(platformDisplay, eglSharingContext);
#endif
    }
    if (!context)
        context = createPbufferContext(platformDisplay, eglSharingContext);

    return context;
}

std::unique_ptr<GLContextEGL> GLContextEGL::createSharingContext(PlatformDisplay& platformDisplay)
{
    if (platformDisplay.eglDisplay() == EGL_NO_DISPLAY)
        return nullptr;

    if (eglBindAPI(gEGLAPIVersion) == EGL_FALSE)
        return nullptr;

    auto context = createSurfacelessContext(platformDisplay);
    if (!context) {
#if PLATFORM(X11)
        if (platformDisplay.type() == PlatformDisplay::Type::X11)
            context = createPixmapContext(platformDisplay);
#endif
#if PLATFORM(WAYLAND)
        if (platformDisplay.type() == PlatformDisplay::Type::Wayland)
            context = createWaylandContext(platformDisplay);
#endif
    }
    if (!context)
        context = createPbufferContext(platformDisplay);

    return context;
}

GLContextEGL::GLContextEGL(PlatformDisplay& display, EGLContext context, EGLSurface surface, EGLSurfaceType type)
    : GLContext(display)
    , m_context(context)
    , m_surface(surface)
    , m_type(type)
{
    ASSERT(type != PixmapSurface);
    ASSERT(type == Surfaceless || surface != EGL_NO_SURFACE);
}

#if PLATFORM(X11)
GLContextEGL::GLContextEGL(PlatformDisplay& display, EGLContext context, EGLSurface surface, XUniquePixmap&& pixmap)
    : GLContext(display)
    , m_context(context)
    , m_surface(surface)
    , m_type(PixmapSurface)
    , m_pixmap(WTFMove(pixmap))
{
}
#endif

#if PLATFORM(WAYLAND)
GLContextEGL::GLContextEGL(PlatformDisplay& display, EGLContext context, EGLSurface surface, WlUniquePtr<struct wl_surface>&& wlSurface, EGLNativeWindowType wlWindow)
    : GLContext(display)
    , m_context(context)
    , m_surface(surface)
    , m_type(WindowSurface)
    , m_wlSurface(WTFMove(wlSurface))
    , m_wlWindow(wlWindow)
{
}
#endif

GLContextEGL::~GLContextEGL()
{
#if USE(CAIRO)
    if (m_cairoDevice)
        cairo_device_destroy(m_cairoDevice);
#endif

    EGLDisplay display = m_display.eglDisplay();
    if (m_context) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, m_context);
    }

    if (m_surface)
        eglDestroySurface(display, m_surface);

#if PLATFORM(WAYLAND)
    if (m_wlWindow)
        wl_egl_window_destroy(m_wlWindow);
#endif
}

bool GLContextEGL::canRenderToDefaultFramebuffer()
{
    return m_type == WindowSurface;
}

IntSize GLContextEGL::defaultFrameBufferSize()
{
    if (!canRenderToDefaultFramebuffer())
        return IntSize();

    EGLDisplay display = m_display.eglDisplay();
    EGLint width, height;
    if (!eglQuerySurface(display, m_surface, EGL_WIDTH, &width)
        || !eglQuerySurface(display, m_surface, EGL_HEIGHT, &height))
        return IntSize();

    return IntSize(width, height);
}

bool GLContextEGL::makeContextCurrent()
{
    ASSERT(m_context);

    GLContext::makeContextCurrent();
    if (eglGetCurrentContext() == m_context)
        return true;

    return eglMakeCurrent(m_display.eglDisplay(), m_surface, m_surface, m_context);
}

void GLContextEGL::swapBuffers()
{
    ASSERT(m_surface);
    eglSwapBuffers(m_display.eglDisplay(), m_surface);
}

void GLContextEGL::waitNative()
{
    eglWaitNative(EGL_CORE_NATIVE_ENGINE);
}

void GLContextEGL::swapInterval(int interval)
{
    ASSERT(m_surface);
    eglSwapInterval(m_display.eglDisplay(), interval);
}

#if USE(CAIRO)
cairo_device_t* GLContextEGL::cairoDevice()
{
    if (m_cairoDevice)
        return m_cairoDevice;

#if ENABLE(ACCELERATED_2D_CANVAS)
    m_cairoDevice = cairo_egl_device_create(m_display.eglDisplay(), m_context);
#endif

    return m_cairoDevice;
}
#endif

#if ENABLE(GRAPHICS_CONTEXT_3D)
PlatformGraphicsContext3D GLContextEGL::platformContext()
{
    return m_context;
}
#endif

} // namespace WebCore

#endif // USE(EGL)
