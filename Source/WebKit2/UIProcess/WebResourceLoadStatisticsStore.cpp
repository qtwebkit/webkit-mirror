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

#include "config.h"
#include "WebResourceLoadStatisticsStore.h"

#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsManager.h"
#include "WebResourceLoadStatisticsStoreMessages.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataType.h"
#include <WebCore/KeyedCoding.h>
#include <WebCore/ResourceLoadObserver.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/RunLoop.h>
#include <wtf/threads/BinarySemaphore.h>

using namespace WebCore;

namespace WebKit {

static OptionSet<WebKit::WebsiteDataType> dataTypesToRemove;
static auto notifyPages = false;
static auto shouldClassifyResourcesBeforeDataRecordsRemoval = true;

Ref<WebResourceLoadStatisticsStore> WebResourceLoadStatisticsStore::create(const String& resourceLoadStatisticsDirectory)
{
    return adoptRef(*new WebResourceLoadStatisticsStore(resourceLoadStatisticsDirectory));
}

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(const String& resourceLoadStatisticsDirectory)
    : m_resourceLoadStatisticsStore(ResourceLoadStatisticsStore::create())
    , m_statisticsQueue(WorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue"))
    , m_statisticsStoragePath(resourceLoadStatisticsDirectory)
{
}

WebResourceLoadStatisticsStore::~WebResourceLoadStatisticsStore()
{
}

void WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(bool always)
{
    notifyPages = always;
}

void WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    shouldClassifyResourcesBeforeDataRecordsRemoval = value;
}

void WebResourceLoadStatisticsStore::classifyResource(ResourceLoadStatistics& resourceStatistic)
{
    if (!resourceStatistic.isPrevalentResource
        && m_resourceLoadStatisticsClassifier.hasPrevalentResourceCharacteristics(resourceStatistic))
        resourceStatistic.isPrevalentResource = true;
}

static inline void initializeDataTypesToRemove()
{
    dataTypesToRemove |= WebsiteDataType::Cookies;
    dataTypesToRemove |= WebsiteDataType::OfflineWebApplicationCache;
    dataTypesToRemove |= WebsiteDataType::SessionStorage;
    dataTypesToRemove |= WebsiteDataType::LocalStorage;
    dataTypesToRemove |= WebsiteDataType::WebSQLDatabases;
    dataTypesToRemove |= WebsiteDataType::IndexedDBDatabases;
    dataTypesToRemove |= WebsiteDataType::MediaKeys;
    dataTypesToRemove |= WebsiteDataType::SearchFieldRecentSearches;
#if ENABLE(NETSCAPE_PLUGIN_API)
    dataTypesToRemove |= WebsiteDataType::PlugInData;
#endif
#if ENABLE(MEDIA_STREAM)
    dataTypesToRemove |= WebsiteDataType::MediaDeviceIdentifier;
#endif
}
    
void WebResourceLoadStatisticsStore::removeDataRecords()
{
    ASSERT(!isMainThread());
    
    if (!coreStore().shouldRemoveDataRecords())
        return;

    Vector<String> prevalentResourceDomains = coreStore().topPrivatelyControlledDomainsToRemoveWebsiteDataFor();
    if (!prevalentResourceDomains.size())
        return;
    
    coreStore().dataRecordsBeingRemoved();

    if (dataTypesToRemove.isEmpty())
        initializeDataTypesToRemove();

    // Switch to the main thread to get the default website data store
    RunLoop::main().dispatch([prevalentResourceDomains = CrossThreadCopier<Vector<String>>::copy(prevalentResourceDomains), this, protectedThis = makeRef(*this)] () mutable {
        WebProcessProxy::deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(dataTypesToRemove, WTFMove(prevalentResourceDomains), notifyPages, [this, protectedThis = WTFMove(protectedThis)](Vector<String> domainsWithDeletedWebsiteData) mutable {
            // But always touch the ResourceLoadStatistics store on the worker queue.
            m_statisticsQueue->dispatch([protectedThis = WTFMove(protectedThis), topDomains = CrossThreadCopier<Vector<String>>::copy(domainsWithDeletedWebsiteData)] () mutable {
                protectedThis->coreStore().updateStatisticsForRemovedDataRecords(topDomains);
                protectedThis->coreStore().dataRecordsWereRemoved();
            });
        });
    });
}

void WebResourceLoadStatisticsStore::processStatisticsAndDataRecords()
{
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] () {
        auto locker = holdLock(coreStore().statisticsLock());
        if (shouldClassifyResourcesBeforeDataRecordsRemoval) {
            coreStore().processStatistics([this] (ResourceLoadStatistics& resourceStatistic) {
                classifyResource(resourceStatistic);
            });
        }
        removeDataRecords();
        
        if (notifyPages) {
            RunLoop::main().dispatch([] () mutable {
                WebProcessProxy::notifyPageStatisticsAndDataRecordsProcessed();
            });
        }
            
        writeStoreToDisk();
    });
}

void WebResourceLoadStatisticsStore::resourceLoadStatisticsUpdated(const Vector<WebCore::ResourceLoadStatistics>& origins)
{
    coreStore().mergeStatistics(origins);
    // Fire before processing statistics to propagate user
    // interaction as fast as possible to the network process.
    coreStore().fireShouldPartitionCookiesHandler();
    processStatisticsAndDataRecords();
}

void WebResourceLoadStatisticsStore::setResourceLoadStatisticsEnabled(bool enabled)
{
    if (enabled == m_resourceLoadStatisticsEnabled)
        return;

    m_resourceLoadStatisticsEnabled = enabled;

    readDataFromDiskIfNeeded();
}

bool WebResourceLoadStatisticsStore::resourceLoadStatisticsEnabled() const
{
    return m_resourceLoadStatisticsEnabled;
}

void WebResourceLoadStatisticsStore::registerSharedResourceLoadObserver()
{
    ASSERT(isMainThread());
    
    ResourceLoadObserver::sharedObserver().setStatisticsStore(m_resourceLoadStatisticsStore.copyRef());
    ResourceLoadObserver::sharedObserver().setStatisticsQueue(m_statisticsQueue.copyRef());
    m_resourceLoadStatisticsStore->setNotificationCallback([this, protectedThis = makeRef(*this)] {
        if (m_resourceLoadStatisticsStore->isEmpty())
            return;
        processStatisticsAndDataRecords();
    });
    m_resourceLoadStatisticsStore->setWritePersistentStoreCallback([this, protectedThis = makeRef(*this)] {
        m_statisticsQueue->dispatch([this, protectedThis = protectedThis.copyRef()] {
            writeStoreToDisk();
        });
    });
    m_resourceLoadStatisticsStore->setGrandfatherExistingWebsiteDataCallback([this, protectedThis = makeRef(*this)]() {
        grandfatherExistingWebsiteData();
    });
#if PLATFORM(COCOA)
    WebResourceLoadStatisticsManager::registerUserDefaultsIfNeeded();
#endif
}
    
void WebResourceLoadStatisticsStore::registerSharedResourceLoadObserver(WTF::Function<void(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)>&& shouldPartitionCookiesForDomainsHandler)
{
    ASSERT(isMainThread());
    
    registerSharedResourceLoadObserver();
    m_resourceLoadStatisticsStore->setShouldPartitionCookiesCallback([shouldPartitionCookiesForDomainsHandler = WTFMove(shouldPartitionCookiesForDomainsHandler)] (const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst) {
        shouldPartitionCookiesForDomainsHandler(domainsToRemove, domainsToAdd, clearFirst);
    });
}

void WebResourceLoadStatisticsStore::grandfatherExistingWebsiteData()
{
    if (dataTypesToRemove.isEmpty())
        initializeDataTypesToRemove();
    
    // Switch to the main thread to get the default website data store
    RunLoop::main().dispatch([this, protectedThis = makeRef(*this)] () mutable {
        WebProcessProxy::topPrivatelyControlledDomainsWithWebiteData(dataTypesToRemove, notifyPages, [this, protectedThis = WTFMove(protectedThis)] (HashSet<String>&& topPrivatelyControlledDomainsWithWebsiteData) mutable {
            // But always touch the ResourceLoadStatistics store on the worker queue
            m_statisticsQueue->dispatch([protectedThis = WTFMove(protectedThis), topDomains = CrossThreadCopier<HashSet<String>>::copy(topPrivatelyControlledDomainsWithWebsiteData)] () mutable {
                protectedThis->coreStore().handleFreshStartWithEmptyOrNoStore(WTFMove(topDomains));
            });
        });
    });
}

void WebResourceLoadStatisticsStore::readDataFromDiskIfNeeded()
{
    if (!m_resourceLoadStatisticsEnabled)
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        auto decoder = createDecoderFromDisk("full_browsing_session");
        if (!decoder) {
            grandfatherExistingWebsiteData();
            return;
        }
        
        auto locker = holdLock(coreStore().statisticsLock());
        coreStore().clearInMemory();
        coreStore().readDataFromDecoder(*decoder);

        if (coreStore().isEmpty())
            grandfatherExistingWebsiteData();
    });
}

void WebResourceLoadStatisticsStore::processWillOpenConnection(WebProcessProxy&, IPC::Connection& connection)
{
    connection.addWorkQueueMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName(), m_statisticsQueue.get(), this);
}

void WebResourceLoadStatisticsStore::processDidCloseConnection(WebProcessProxy&, IPC::Connection& connection)
{
    connection.removeWorkQueueMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName());
}

void WebResourceLoadStatisticsStore::applicationWillTerminate()
{
    BinarySemaphore semaphore;
    m_statisticsQueue->dispatch([&semaphore] {
        // Make sure any ongoing work in our queue is finished before we terminate.
        semaphore.signal();
    });
    semaphore.wait(WallTime::infinity());
}

String WebResourceLoadStatisticsStore::persistentStoragePath(const String& label) const
{
    if (m_statisticsStoragePath.isEmpty())
        return emptyString();

    // TODO Decide what to call this file
    return pathByAppendingComponent(m_statisticsStoragePath, label + "_resourceLog.plist");
}

void WebResourceLoadStatisticsStore::writeStoreToDisk()
{
    ASSERT(!isMainThread());
    
    auto encoder = coreStore().createEncoderFromData();
    writeEncoderToDisk(*encoder.get(), "full_browsing_session");
}

void WebResourceLoadStatisticsStore::writeEncoderToDisk(KeyedEncoder& encoder, const String& label) const
{
    ASSERT(!isMainThread());
    
    RefPtr<SharedBuffer> rawData = encoder.finishEncoding();
    if (!rawData)
        return;

    String resourceLog = persistentStoragePath(label);
    if (resourceLog.isEmpty())
        return;

    if (!m_statisticsStoragePath.isEmpty()) {
        makeAllDirectories(m_statisticsStoragePath);
        platformExcludeFromBackup();
    }

    auto handle = openFile(resourceLog, OpenForWrite);
    if (!handle)
        return;
    
    int64_t writtenBytes = writeToFile(handle, rawData->data(), rawData->size());
    closeFile(handle);

    if (writtenBytes != static_cast<int64_t>(rawData->size()))
        WTFLogAlways("WebResourceLoadStatisticsStore: We only wrote %d out of %d bytes to disk", static_cast<unsigned>(writtenBytes), rawData->size());
}

#if !PLATFORM(COCOA)
void WebResourceLoadStatisticsStore::platformExcludeFromBackup() const
{
    // Do nothing
}
#endif

std::unique_ptr<KeyedDecoder> WebResourceLoadStatisticsStore::createDecoderFromDisk(const String& label) const
{
    String resourceLog = persistentStoragePath(label);
    if (resourceLog.isEmpty())
        return nullptr;

    RefPtr<SharedBuffer> rawData = SharedBuffer::createWithContentsOfFile(resourceLog);
    if (!rawData)
        return nullptr;

    return KeyedDecoder::decoder(reinterpret_cast<const uint8_t*>(rawData->data()), rawData->size());
}

} // namespace WebKit
