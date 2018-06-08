/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DisplayLink.h"

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400

#include "DrawingAreaMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <wtf/ProcessPrivilege.h>

namespace WebKit {
    
DisplayLink::DisplayLink(WebCore::PlatformDisplayID displayID, WebPageProxy& webPageProxy)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    CVReturn error = CVDisplayLinkCreateWithCGDisplay(displayID, &m_displayLink);
    if (error) {
        WTFLogAlways("Could not create a display link: %d", error);
        return;
    }
    
    error = CVDisplayLinkSetOutputCallback(m_displayLink, displayLinkCallback, &webPageProxy);
    if (error) {
        WTFLogAlways("Could not set the display link output callback: %d", error);
        return;
    }

    error = CVDisplayLinkStart(m_displayLink);
    if (error)
        WTFLogAlways("Could not start the display link: %d", error);
}

DisplayLink::~DisplayLink()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    ASSERT(m_displayLink);
    if (!m_displayLink)
        return;

    CVDisplayLinkStop(m_displayLink);
    CVDisplayLinkRelease(m_displayLink);
}

void DisplayLink::addObserver(unsigned observerID)
{
    m_observers.add(observerID);
}

void DisplayLink::removeObserver(unsigned observerID)
{
    m_observers.remove(observerID);
}

bool DisplayLink::hasObservers() const
{
    return !m_observers.isEmpty();
}

void DisplayLink::pause()
{
    if (!CVDisplayLinkIsRunning(m_displayLink))
        return;
    CVDisplayLinkStop(m_displayLink);
}

void DisplayLink::resume()
{
    if (CVDisplayLinkIsRunning(m_displayLink))
        return;
    CVDisplayLinkStart(m_displayLink);
}

CVReturn DisplayLink::displayLinkCallback(CVDisplayLinkRef displayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* data)
{
    WebPageProxy* webPageProxy = reinterpret_cast<WebPageProxy*>(data);
    RunLoop::main().dispatch([weakPtr = webPageProxy->createWeakPtr()] {
        if (auto* proxy = weakPtr.get())
            proxy->process().send(Messages::DrawingArea::DisplayWasRefreshed(), proxy->pageID());
    });
    return kCVReturnSuccess;
}

}

#endif
