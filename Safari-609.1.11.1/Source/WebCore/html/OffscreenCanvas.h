/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(OFFSCREEN_CANVAS)

#include "AffineTransform.h"
#include "CanvasBase.h"
#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "IDLTypes.h"
#include "IntSize.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CanvasRenderingContext;
class ImageBitmap;
class WebGLRenderingContext;

#if ENABLE(WEBGL)
using OffscreenRenderingContext = RefPtr<WebGLRenderingContext>;
#endif

class OffscreenCanvas final : public RefCounted<OffscreenCanvas>, public CanvasBase, public EventTargetWithInlineData, private ContextDestructionObserver {
    WTF_MAKE_ISO_ALLOCATED(OffscreenCanvas);
public:

    struct ImageEncodeOptions {
        String type = "image/png";
        double quality = 1.0;
    };

    enum class RenderingContextType {
        _2d,
        Webgl
    };

    static Ref<OffscreenCanvas> create(ScriptExecutionContext&, unsigned width, unsigned height);
    virtual ~OffscreenCanvas();

    void setWidth(unsigned);
    void setHeight(unsigned);

    CanvasRenderingContext* renderingContext() const final { return m_context.get(); }

#if ENABLE(WEBGL)
    ExceptionOr<OffscreenRenderingContext> getContext(JSC::JSGlobalObject&, RenderingContextType, Vector<JSC::Strong<JSC::Unknown>>&& arguments);
#endif
    RefPtr<ImageBitmap> transferToImageBitmap();
    // void convertToBlob(ImageEncodeOptions options);

    void didDraw(const FloatRect&) final { }

    Image* copiedImage() const final { return nullptr; }

    using RefCounted::ref;
    using RefCounted::deref;

private:

    OffscreenCanvas(ScriptExecutionContext&, unsigned width, unsigned height);

    bool isOffscreenCanvas() const final { return true; }

    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }
    ScriptExecutionContext* canvasBaseScriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    EventTargetInterface eventTargetInterface() const final { return OffscreenCanvasEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    void refCanvasBase() final { ref(); }
    void derefCanvasBase() final { deref(); }

    void createImageBuffer() const final;

    std::unique_ptr<CanvasRenderingContext> m_context;
};

}

SPECIALIZE_TYPE_TRAITS_CANVAS(WebCore::OffscreenCanvas, isOffscreenCanvas())

#endif
