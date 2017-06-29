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

#include "APIObject.h"

#include <wtf/RefPtr.h>
#include <wtf/Seconds.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebResourceLoadStatisticsManager : public API::ObjectImpl<API::Object::Type::WebResourceLoadStatisticsManager> {
public:
    static Ref<WebResourceLoadStatisticsManager> create()
    {
        return adoptRef(*new WebResourceLoadStatisticsManager());
    }
    static void setPrevalentResource(const String& hostName, bool value);
    static bool isPrevalentResource(const String& hostName);
    static void setHasHadUserInteraction(const String& hostName, bool value);
    static bool hasHadUserInteraction(const String& hostName);
    static void setGrandfathered(const String& hostName, bool value);
    static bool isGrandfathered(const String& hostName);
    static void setSubframeUnderTopFrameOrigin(const String& hostName, const String& topFrameHostName);
    static void setSubresourceUnderTopFrameOrigin(const String& hostName, const String& topFrameHostName);
    static void setSubresourceUniqueRedirectTo(const String& hostName, const String& hostNameRedirectedTo);
    static void setTimeToLiveUserInteraction(Seconds);
    static void setTimeToLiveCookiePartitionFree(Seconds);
    static void setMinimumTimeBetweeenDataRecordsRemoval(Seconds);
    static void setGrandfatheringTime(Seconds);
    static void setReducedTimestampResolution(Seconds);
    static void fireDataModificationHandler();
    static void fireShouldPartitionCookiesHandler();
    static void fireShouldPartitionCookiesHandlerForOneDomain(const String& hostName, bool value);
    static void fireTelemetryHandler();
    static void setNotifyPagesWhenDataRecordsWereScanned(bool);
    static void setNotifyPagesWhenTelemetryWasCaptured(bool value);
    static void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value);
    static void clearInMemoryAndPersistentStore();
    static void clearInMemoryAndPersistentStoreModifiedSinceHours(unsigned);
    static void resetToConsistentState();
#if PLATFORM(COCOA)
    static void registerUserDefaultsIfNeeded();
#endif

private:
};

} // namespace WebKit
