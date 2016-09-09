/*
 * Copyright (C) 2012 Igalia S.L.
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
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#ifndef GLContextGLX_h
#define GLContextGLX_h

#if USE(GLX)

#include "GLContext.h"
#include "XUniquePtr.h"
#include "XUniqueResource.h"

typedef unsigned char GLubyte;
typedef unsigned long XID;
typedef void* ContextKeyType;

namespace WebCore {

class GLContextGLX final : public GLContext {
    WTF_MAKE_NONCOPYABLE(GLContextGLX);
public:
    static std::unique_ptr<GLContextGLX> createContext(XID window, PlatformDisplay&);
    static std::unique_ptr<GLContextGLX> createSharingContext(PlatformDisplay&);

    virtual ~GLContextGLX();

    bool makeContextCurrent() override;
    void swapBuffers() override;
    void waitNative() override;
    bool canRenderToDefaultFramebuffer() override;
    IntSize defaultFrameBufferSize() override;
    void swapInterval(int) override;
    cairo_device_t* cairoDevice() override;
    bool isEGLContext() const override { return false; }

#if ENABLE(GRAPHICS_CONTEXT_3D)
    PlatformGraphicsContext3D platformContext() override;
#endif

    void clear();

private:
    GLContextGLX(PlatformDisplay&, XUniqueGLXContext&&, XID);
    GLContextGLX(PlatformDisplay&, XUniqueGLXContext&&, XUniqueGLXPbuffer&&);
    GLContextGLX(PlatformDisplay&, XUniqueGLXContext&&, XUniquePixmap&&, XUniqueGLXPixmap&&);

    static std::unique_ptr<GLContextGLX> createWindowContext(XID window, PlatformDisplay&, GLXContext sharingContext = nullptr);
    static std::unique_ptr<GLContextGLX> createPbufferContext(PlatformDisplay&, GLXContext sharingContext = nullptr);
    static std::unique_ptr<GLContextGLX> createPixmapContext(PlatformDisplay&, GLXContext sharingContext = nullptr);

    XUniqueGLXContext m_context;
    XID m_window { 0 };
    XUniqueGLXPbuffer m_pbuffer;
    XUniquePixmap m_pixmap;
    XUniqueGLXPixmap m_glxPixmap;
    cairo_device_t* m_cairoDevice { nullptr };
};

} // namespace WebCore

#endif // USE(GLX)

#endif // GLContextGLX_h
