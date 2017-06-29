/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "Connection.h"
#include "ResourceLoadStatisticsClassifier.h"
#include "WebResourceLoadStatisticsTelemetry.h"
#include "WebsiteDataRecord.h"
#include <WebCore/ResourceLoadStatisticsStore.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if HAVE(CORE_PREDICTION)
#include "ResourceLoadStatisticsClassifierCocoa.h"
#endif

namespace WTF {
class WorkQueue;
}

namespace WebCore {
class KeyedDecoder;
class KeyedEncoder;
class FileMonitor;
struct ResourceLoadStatistics;
}

namespace WebKit {

class WebProcessPool;
class WebProcessProxy;

class WebResourceLoadStatisticsStore : public IPC::Connection::WorkQueueMessageReceiver {
public:
    static Ref<WebResourceLoadStatisticsStore> create(const String&);
    static void setNotifyPagesWhenDataRecordsWereScanned(bool);
    static void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool);
    static void setMinimumTimeBetweeenDataRecordsRemoval(Seconds);
    virtual ~WebResourceLoadStatisticsStore();
    
    void setResourceLoadStatisticsEnabled(bool);
    bool resourceLoadStatisticsEnabled() const;
    void registerSharedResourceLoadObserver();
    void registerSharedResourceLoadObserver(WTF::Function<void(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)>&& shouldPartitionCookiesForDomainsHandler);
    
    void resourceLoadStatisticsUpdated(const Vector<WebCore::ResourceLoadStatistics>& origins);

    void processWillOpenConnection(WebProcessProxy&, IPC::Connection&);
    void processDidCloseConnection(WebProcessProxy&, IPC::Connection&);
    void applicationWillTerminate();

    void readDataFromDiskIfNeeded();

    WebCore::ResourceLoadStatisticsStore& coreStore() { return m_resourceLoadStatisticsStore.get(); }
    const WebCore::ResourceLoadStatisticsStore& coreStore() const { return m_resourceLoadStatisticsStore.get(); }

private:
    explicit WebResourceLoadStatisticsStore(const String&);

    void processStatisticsAndDataRecords();

    void classifyResource(WebCore::ResourceLoadStatistics&);
    void removeDataRecords();
    void startMonitoringStatisticsStorage();
    void stopMonitoringStatisticsStorage();

    String persistentStoragePath(const String& label) const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void grandfatherExistingWebsiteData();

    void writeStoreToDisk();
    void writeEncoderToDisk(WebCore::KeyedEncoder&, const String& label) const;
    std::unique_ptr<WebCore::KeyedDecoder> createDecoderFromDisk(const String& label) const;
    void platformExcludeFromBackup() const;
    void deleteStoreFromDisk();
    void clearInMemoryData();
    void syncWithExistingStatisticsStorageIfNeeded();
    void refreshFromDisk();
    void telemetryTimerFired();

    Ref<WebCore::ResourceLoadStatisticsStore> m_resourceLoadStatisticsStore;
#if HAVE(CORE_PREDICTION)
    ResourceLoadStatisticsClassifierCocoa m_resourceLoadStatisticsClassifier;
#else
    ResourceLoadStatisticsClassifier m_resourceLoadStatisticsClassifier;
#endif
    Ref<WTF::WorkQueue> m_statisticsQueue;
    RefPtr<WebCore::FileMonitor> m_statisticsStorageMonitor;
    String m_statisticsStoragePath;
    bool m_resourceLoadStatisticsEnabled { false };
    RunLoop::Timer<WebResourceLoadStatisticsStore> m_telemetryOneShotTimer;
    RunLoop::Timer<WebResourceLoadStatisticsStore> m_telemetryRepeatedTimer;
};

} // namespace WebKit
