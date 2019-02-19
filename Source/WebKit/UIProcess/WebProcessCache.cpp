/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WebProcessCache.h"

#include "Logging.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <wtf/RAMSize.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {

static Seconds cachedProcessLifetime { 30_min };
static Seconds clearingDelayAfterApplicationResignsActive { 5_min };

WebProcessCache::WebProcessCache(WebProcessPool& processPool)
    : m_evictionTimer(RunLoop::main(), this, &WebProcessCache::clear)
{
    updateCapacity(processPool);
}

bool WebProcessCache::addProcess(const String& registrableDomain, Ref<WebProcessProxy>&& process)
{
    ASSERT(!registrableDomain.isEmpty());
    ASSERT(!process->pageCount());
    ASSERT(!process->provisionalPageCount());
    ASSERT(!process->processPool().hasSuspendedPageFor(process));

    if (!capacity())
        return false;

    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::addProcess(): Not caching process %i because we are under memory pressure", this, process->processIdentifier());
        return false;
    }

    while (m_processesPerRegistrableDomain.size() >= capacity()) {
        auto it = m_processesPerRegistrableDomain.random();
        RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::addProcess(): Evicting process %i from WebProcess cache", this, it->value->process().processIdentifier());
        m_processesPerRegistrableDomain.remove(it);
    }

    auto addResult = m_processesPerRegistrableDomain.ensure(registrableDomain, [process = process.copyRef()]() mutable {
        return std::make_unique<CachedProcess>(WTFMove(process));
    });
    if (!addResult.isNewEntry)
        return false;

    RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::addProcess: Adding process %i to WebProcess cache, cache size: [%u / %u]", this, process->processIdentifier(), size(), capacity());
    return true;
}

RefPtr<WebProcessProxy> WebProcessCache::takeProcess(const String& registrableDomain, WebsiteDataStore& dataStore)
{
    auto it = m_processesPerRegistrableDomain.find(registrableDomain);
    if (it == m_processesPerRegistrableDomain.end())
        return nullptr;

    if (&it->value->process().websiteDataStore() != &dataStore)
        return nullptr;

    auto process = it->value->takeProcess();
    m_processesPerRegistrableDomain.remove(it);
    RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::takeProcess: Taking process %i from WebProcess cache, cache size: [%u / %u]", this, process->processIdentifier(), size(), capacity());

    ASSERT(!process->pageCount());
    ASSERT(!process->provisionalPageCount());
    ASSERT(!process->processPool().hasSuspendedPageFor(process));

    return WTFMove(process);
}

void WebProcessCache::updateCapacity(WebProcessPool& processPool)
{
    if (!processPool.configuration().processSwapsOnNavigation() || !processPool.configuration().usesWebProcessCache()) {
        if (!processPool.configuration().processSwapsOnNavigation())
            RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::updateCapacity: Cache is disabled because process swap on navigation is disabled", this);
        else
            RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::updateCapacity: Cache is disabled by client", this);
        m_capacity = 0;
    } else {
        size_t memorySize = ramSize() / GB;
        if (memorySize < 3) {
            m_capacity = 0;
            RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::updateCapacity: Cache is disabled because device does not have enough RAM", this);
        } else {
            // Allow 4 processes in the cache per GB of RAM, up to 30 processes.
            m_capacity = std::min<unsigned>(memorySize * 4, 30);
            RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::updateCapacity: Cache has a capacity of %u processes", this, capacity());
        }
    }

    if (!m_capacity)
        clear();
}

void WebProcessCache::clear()
{
    if (m_processesPerRegistrableDomain.isEmpty())
        return;

    RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::clear() evicting %u processes", this, m_processesPerRegistrableDomain.size());
    m_processesPerRegistrableDomain.clear();
}

void WebProcessCache::setApplicationIsActive(bool isActive)
{
    RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::setApplicationIsActive(%d)", this, isActive);
    if (isActive)
        m_evictionTimer.stop();
    else if (!m_processesPerRegistrableDomain.isEmpty())
        m_evictionTimer.startOneShot(clearingDelayAfterApplicationResignsActive);
}

void WebProcessCache::evictProcess(WebProcessProxy& process)
{
    auto it = m_processesPerRegistrableDomain.find(process.registrableDomain());
    ASSERT(it != m_processesPerRegistrableDomain.end());
    ASSERT(&it->value->process() == &process);

    RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::evictProcess(): Evicting process %i from WebProcess cache because it expired", this, process.processIdentifier());

    m_processesPerRegistrableDomain.remove(it);
}

WebProcessCache::CachedProcess::CachedProcess(Ref<WebProcessProxy>&& process)
    : m_process(WTFMove(process))
    , m_evictionTimer(RunLoop::main(), this, &CachedProcess::evictionTimerFired)
{
    m_process->setIsInProcessCache(true);
    m_evictionTimer.startOneShot(cachedProcessLifetime);
}

WebProcessCache::CachedProcess::~CachedProcess()
{
    if (!m_process)
        return;

    ASSERT(!m_process->pageCount());
    ASSERT(!m_process->provisionalPageCount());
    ASSERT(!m_process->processPool().hasSuspendedPageFor(*m_process));

    m_process->setIsInProcessCache(false);
    m_process->shutDown();
}

Ref<WebProcessProxy> WebProcessCache::CachedProcess::takeProcess()
{
    ASSERT(m_process);
    m_evictionTimer.stop();
    m_process->setIsInProcessCache(false);
    return m_process.releaseNonNull();
}

void WebProcessCache::CachedProcess::evictionTimerFired()
{
    ASSERT(m_process);
    m_process->processPool().webProcessCache().evictProcess(*m_process);
}

} // namespace WebKit
