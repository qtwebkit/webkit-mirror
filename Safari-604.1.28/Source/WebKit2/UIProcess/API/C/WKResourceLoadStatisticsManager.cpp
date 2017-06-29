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

#include "config.h"
#include "WKResourceLoadStatisticsManager.h"

#include "WKAPICast.h"
#include "WebResourceLoadStatisticsManager.h"
#include <wtf/Seconds.h>

using namespace WebKit;

WKTypeID WKResourceLoadStatisticsManagerGetTypeID()
{
    return toAPI(WebResourceLoadStatisticsManager::APIType);
}

void WKResourceLoadStatisticsManagerSetPrevalentResource(WKStringRef hostName, bool value)
{
    WebResourceLoadStatisticsManager::setPrevalentResource(toWTFString(hostName), value);
}

bool WKResourceLoadStatisticsManagerIsPrevalentResource(WKStringRef hostName)
{
    return WebResourceLoadStatisticsManager::isPrevalentResource(toWTFString(hostName));
}

void WKResourceLoadStatisticsManagerSetHasHadUserInteraction(WKStringRef hostName, bool value)
{
    WebResourceLoadStatisticsManager::setHasHadUserInteraction(toWTFString(hostName), value);
}

bool WKResourceLoadStatisticsManagerIsHasHadUserInteraction(WKStringRef hostName)
{
    return WebResourceLoadStatisticsManager::hasHadUserInteraction(toWTFString(hostName));
}

void WKResourceLoadStatisticsManagerSetGrandfathered(WKStringRef hostName, bool value)
{
    WebResourceLoadStatisticsManager::setGrandfathered(toWTFString(hostName), value);
}

bool WKResourceLoadStatisticsManagerIsGrandfathered(WKStringRef hostName)
{
    return WebResourceLoadStatisticsManager::isGrandfathered(toWTFString(hostName));
}

void WKResourceLoadStatisticsManagerSetSubframeUnderTopFrameOrigin(WKStringRef hostName, WKStringRef topFrameHostName)
{
    WebResourceLoadStatisticsManager::setSubframeUnderTopFrameOrigin(toWTFString(hostName), toWTFString(topFrameHostName));
}

void WKResourceLoadStatisticsManagerSetSubresourceUnderTopFrameOrigin(WKStringRef hostName, WKStringRef topFrameHostName)
{
    WebResourceLoadStatisticsManager::setSubresourceUnderTopFrameOrigin(toWTFString(hostName), toWTFString(topFrameHostName));
}

void WKResourceLoadStatisticsManagerSetSubresourceUniqueRedirectTo(WKStringRef hostName, WKStringRef hostNameRedirectedTo)
{
    WebResourceLoadStatisticsManager::setSubresourceUniqueRedirectTo(toWTFString(hostName), toWTFString(hostNameRedirectedTo));
}

void WKResourceLoadStatisticsManagerSetTimeToLiveUserInteraction(double seconds)
{
    WebResourceLoadStatisticsManager::setTimeToLiveUserInteraction(Seconds { seconds });
}

void WKResourceLoadStatisticsManagerSetTimeToLiveCookiePartitionFree(double seconds)
{
    WebResourceLoadStatisticsManager::setTimeToLiveCookiePartitionFree(Seconds { seconds });
}

void WKResourceLoadStatisticsManagerSetMinimumTimeBetweeenDataRecordsRemoval(double seconds)
{
    WebResourceLoadStatisticsManager::setMinimumTimeBetweeenDataRecordsRemoval(Seconds { seconds });
}

void WKResourceLoadStatisticsManagerSetGrandfatheringTime(double seconds)
{
    WebResourceLoadStatisticsManager::setGrandfatheringTime(Seconds { seconds });
}

void WKResourceLoadStatisticsManagerFireDataModificationHandler()
{
    WebResourceLoadStatisticsManager::fireDataModificationHandler();
}

void WKResourceLoadStatisticsManagerFireShouldPartitionCookiesHandler()
{
    WebResourceLoadStatisticsManager::fireShouldPartitionCookiesHandler();
}

void WKResourceLoadStatisticsManagerFireShouldPartitionCookiesHandlerForOneDomain(WKStringRef hostName, bool value)
{
    WebResourceLoadStatisticsManager::fireShouldPartitionCookiesHandlerForOneDomain(toWTFString(hostName), value);
}

void WKResourceLoadStatisticsManagerFireTelemetryHandler()
{
    WebResourceLoadStatisticsManager::fireTelemetryHandler();
}

void WKResourceLoadStatisticsManagerSetNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    WebResourceLoadStatisticsManager::setNotifyPagesWhenDataRecordsWereScanned(value);
}

void WKResourceLoadStatisticsManagerSetShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    WebResourceLoadStatisticsManager::setShouldClassifyResourcesBeforeDataRecordsRemoval(value);
}

void WKResourceLoadStatisticsManagerSetNotifyPagesWhenTelemetryWasCaptured(bool value)
{
    WebResourceLoadStatisticsManager::setNotifyPagesWhenTelemetryWasCaptured(value);
}

void WKResourceLoadStatisticsManagerClearInMemoryAndPersistentStore()
{
    WebResourceLoadStatisticsManager::clearInMemoryAndPersistentStore();
}

void WKResourceLoadStatisticsManagerClearInMemoryAndPersistentStoreModifiedSinceHours(unsigned hours)
{
    WebResourceLoadStatisticsManager::clearInMemoryAndPersistentStoreModifiedSinceHours(hours);
}

void WKResourceLoadStatisticsManagerResetToConsistentState()
{
    WebResourceLoadStatisticsManager::resetToConsistentState();
}
