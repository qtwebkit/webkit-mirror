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
#include "WebResourceLoadStatisticsManager.h"

#include "Logging.h"
#include "WebResourceLoadStatisticsStore.h"
#include <WebCore/ResourceLoadObserver.h>
#include <WebCore/URL.h>

using namespace WebCore;

namespace WebKit {

void WebResourceLoadStatisticsManager::setPrevalentResource(const String& hostName, bool value)
{
    if (value)
        WebCore::ResourceLoadObserver::sharedObserver().setPrevalentResource(URL(URL(), hostName));
    else
        WebCore::ResourceLoadObserver::sharedObserver().clearPrevalentResource(URL(URL(), hostName));
}

bool WebResourceLoadStatisticsManager::isPrevalentResource(const String& hostName)
{
    return WebCore::ResourceLoadObserver::sharedObserver().isPrevalentResource(URL(URL(), hostName));
}
    
void WebResourceLoadStatisticsManager::setHasHadUserInteraction(const String& hostName, bool value)
{
    if (value)
        WebCore::ResourceLoadObserver::sharedObserver().logUserInteraction(URL(URL(), hostName));
    else
        WebCore::ResourceLoadObserver::sharedObserver().clearUserInteraction(URL(URL(), hostName));
}

bool WebResourceLoadStatisticsManager::hasHadUserInteraction(const String& hostName)
{
    return WebCore::ResourceLoadObserver::sharedObserver().hasHadUserInteraction(URL(URL(), hostName));
}

void WebResourceLoadStatisticsManager::setGrandfathered(const String& hostName, bool value)
{
    WebCore::ResourceLoadObserver::sharedObserver().setGrandfathered(URL(URL(), hostName), value);
}

bool WebResourceLoadStatisticsManager::isGrandfathered(const String& hostName)
{
    return WebCore::ResourceLoadObserver::sharedObserver().isGrandfathered(URL(URL(), hostName));
}

void WebResourceLoadStatisticsManager::setSubframeUnderTopFrameOrigin(const String& hostName, const String& topFrameHostName)
{
    WebCore::ResourceLoadObserver::sharedObserver().setSubframeUnderTopFrameOrigin(URL(URL(), hostName), URL(URL(), topFrameHostName));
}

void WebResourceLoadStatisticsManager::setSubresourceUnderTopFrameOrigin(const String& hostName, const String& topFrameHostName)
{
    WebCore::ResourceLoadObserver::sharedObserver().setSubresourceUnderTopFrameOrigin(URL(URL(), hostName), URL(URL(), topFrameHostName));
}

void WebResourceLoadStatisticsManager::setSubresourceUniqueRedirectTo(const String& hostName, const String& hostNameRedirectedTo)
{
    WebCore::ResourceLoadObserver::sharedObserver().setSubresourceUniqueRedirectTo(URL(URL(), hostName), URL(URL(), hostNameRedirectedTo));
}

void WebResourceLoadStatisticsManager::setTimeToLiveUserInteraction(Seconds seconds)
{
    WebCore::ResourceLoadObserver::sharedObserver().setTimeToLiveUserInteraction(seconds);
}

void WebResourceLoadStatisticsManager::setTimeToLiveCookiePartitionFree(Seconds seconds)
{
    WebCore::ResourceLoadObserver::sharedObserver().setTimeToLiveCookiePartitionFree(seconds);
}

void WebResourceLoadStatisticsManager::setMinimumTimeBetweeenDataRecordsRemoval(Seconds seconds)
{
    WebCore::ResourceLoadObserver::sharedObserver().setMinimumTimeBetweeenDataRecordsRemoval(seconds);
}

void WebResourceLoadStatisticsManager::setGrandfatheringTime(Seconds seconds)
{
    WebCore::ResourceLoadObserver::sharedObserver().setGrandfatheringTime(seconds);
}

void WebResourceLoadStatisticsManager::fireDataModificationHandler()
{
    WebCore::ResourceLoadObserver::sharedObserver().fireDataModificationHandler();
}

void WebResourceLoadStatisticsManager::fireShouldPartitionCookiesHandler()
{
    WebCore::ResourceLoadObserver::sharedObserver().fireShouldPartitionCookiesHandler();
}

void WebResourceLoadStatisticsManager::fireShouldPartitionCookiesHandlerForOneDomain(const String& hostName, bool value)
{
    if (value)
        WebCore::ResourceLoadObserver::sharedObserver().fireShouldPartitionCookiesHandler({ }, {hostName}, false);
    else
        WebCore::ResourceLoadObserver::sharedObserver().fireShouldPartitionCookiesHandler({hostName}, { }, false);
}

void WebResourceLoadStatisticsManager::fireTelemetryHandler()
{
    WebCore::ResourceLoadObserver::sharedObserver().fireTelemetryHandler();
}
    
void WebResourceLoadStatisticsManager::setNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(value);
}

void WebResourceLoadStatisticsManager::setNotifyPagesWhenTelemetryWasCaptured(bool value)
{
    WebResourceLoadStatisticsTelemetry::setNotifyPagesWhenTelemetryWasCaptured(value);
}
    
void WebResourceLoadStatisticsManager::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(value);
}

void WebResourceLoadStatisticsManager::clearInMemoryAndPersistentStore()
{
    WebCore::ResourceLoadObserver::sharedObserver().clearInMemoryAndPersistentStore();
}

void WebResourceLoadStatisticsManager::clearInMemoryAndPersistentStoreModifiedSinceHours(unsigned hours)
{
    WebCore::ResourceLoadObserver::sharedObserver().clearInMemoryAndPersistentStore(std::chrono::system_clock::now() - std::chrono::hours(hours));
}
    
void WebResourceLoadStatisticsManager::resetToConsistentState()
{
    WebCore::ResourceLoadObserver::sharedObserver().setTimeToLiveUserInteraction(24_h * 30.);
    WebCore::ResourceLoadObserver::sharedObserver().setTimeToLiveCookiePartitionFree(24_h);
    WebCore::ResourceLoadObserver::sharedObserver().setMinimumTimeBetweeenDataRecordsRemoval(1_h);
    WebCore::ResourceLoadObserver::sharedObserver().setGrandfatheringTime(1_h);
    WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(false);
    WebResourceLoadStatisticsTelemetry::setNotifyPagesWhenTelemetryWasCaptured(false);
    WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(true);
    WebCore::ResourceLoadObserver::sharedObserver().clearInMemoryStore();
}
    
} // namespace WebKit
