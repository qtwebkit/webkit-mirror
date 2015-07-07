/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "NetworkProcessProxy.h"

#if ENABLE(NETWORK_PROCESS)

#include "AuthenticationChallengeProxy.h"
#include "CustomProtocolManagerProxyMessages.h"
#include "DownloadProxyMessages.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "WebContext.h"
#include "WebProcessMessages.h"
#include <wtf/RunLoop.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShimProxy.h"
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())

using namespace WebCore;

namespace WebKit {

PassRefPtr<NetworkProcessProxy> NetworkProcessProxy::create(WebContext& webContext)
{
    return adoptRef(new NetworkProcessProxy(webContext));
}

NetworkProcessProxy::NetworkProcessProxy(WebContext& webContext)
    : m_webContext(webContext)
    , m_numPendingConnectionRequests(0)
#if ENABLE(CUSTOM_PROTOCOLS)
    , m_customProtocolManagerProxy(this, webContext)
#endif
{
    connect();
}

NetworkProcessProxy::~NetworkProcessProxy()
{
}

void NetworkProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::NetworkProcess;
    platformGetLaunchOptions(launchOptions);
}

void NetworkProcessProxy::connectionWillOpen(IPC::Connection* connection)
{
#if ENABLE(SEC_ITEM_SHIM)
    SecItemShimProxy::shared().initializeConnection(connection);
#else
    UNUSED_PARAM(connection);
#endif
}

void NetworkProcessProxy::connectionWillClose(IPC::Connection*)
{
}

void NetworkProcessProxy::getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply)
{
    m_pendingConnectionReplies.append(reply);

    if (state() == State::Launching) {
        m_numPendingConnectionRequests++;
        return;
    }

    connection()->send(Messages::NetworkProcess::CreateNetworkConnectionToWebProcess(), 0, IPC::DispatchMessageEvenWhenWaitingForSyncReply);
}

DownloadProxy* NetworkProcessProxy::createDownloadProxy()
{
    if (!m_downloadProxyMap)
        m_downloadProxyMap = std::make_unique<DownloadProxyMap>(this);

    return m_downloadProxyMap->createDownloadProxy(m_webContext);
}

void NetworkProcessProxy::networkProcessCrashedOrFailedToLaunch()
{
    // The network process must have crashed or exited, send any pending sync replies we might have.
    while (!m_pendingConnectionReplies.isEmpty()) {
        RefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply = m_pendingConnectionReplies.takeFirst();

#if OS(DARWIN)
        reply->send(IPC::Attachment(0, MACH_MSG_TYPE_MOVE_SEND));
#elif USE(UNIX_DOMAIN_SOCKETS)
        reply->send(IPC::Attachment());
#else
        notImplemented();
#endif
    }

    // Tell the network process manager to forget about this network process proxy. This may cause us to be deleted.
    m_webContext.networkProcessCrashed(this);
}

void NetworkProcessProxy::didReceiveMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder)
{
    if (dispatchMessage(connection, decoder))
        return;

    if (m_webContext.dispatchMessage(connection, decoder))
        return;

    didReceiveNetworkProcessProxyMessage(connection, decoder);
}

void NetworkProcessProxy::didReceiveSyncMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& replyEncoder)
{
    if (dispatchSyncMessage(connection, decoder, replyEncoder))
        return;

    ASSERT_NOT_REACHED();
}

void NetworkProcessProxy::didClose(IPC::Connection*)
{
    if (m_downloadProxyMap)
        m_downloadProxyMap->processDidClose();

    // This may cause us to be deleted.
    networkProcessCrashedOrFailedToLaunch();
}

void NetworkProcessProxy::didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference, IPC::StringReference)
{
}

void NetworkProcessProxy::didCreateNetworkConnectionToWebProcess(const IPC::Attachment& connectionIdentifier)
{
    ASSERT(!m_pendingConnectionReplies.isEmpty());

    // Grab the first pending connection reply.
    RefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply = m_pendingConnectionReplies.takeFirst();

#if OS(DARWIN)
    reply->send(IPC::Attachment(connectionIdentifier.port(), MACH_MSG_TYPE_MOVE_SEND));
#elif USE(UNIX_DOMAIN_SOCKETS)
    reply->send(connectionIdentifier);
#else
    notImplemented();
#endif
}

void NetworkProcessProxy::didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, const WebCore::AuthenticationChallenge& coreChallenge, uint64_t challengeID)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    MESSAGE_CHECK(page);

    RefPtr<AuthenticationChallengeProxy> authenticationChallenge = AuthenticationChallengeProxy::create(coreChallenge, challengeID, connection());
    page->didReceiveAuthenticationChallengeProxy(frameID, authenticationChallenge.release());
}

void NetworkProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    ChildProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (IPC::Connection::identifierIsNull(connectionIdentifier)) {
        // FIXME: Do better cleanup here.
        return;
    }

    for (unsigned i = 0; i < m_numPendingConnectionRequests; ++i)
        connection()->send(Messages::NetworkProcess::CreateNetworkConnectionToWebProcess(), 0);
    
    m_numPendingConnectionRequests = 0;

#if PLATFORM(COCOA)
    if (m_webContext.processSuppressionEnabled())
        setProcessSuppressionEnabled(true);
#endif
    
#if PLATFORM(IOS) && USE(XPC_SERVICES)
    if (xpc_connection_t connection = this->connection()->xpcConnection())
        m_assertion = std::make_unique<ProcessAssertion>(xpc_connection_get_pid(connection), AssertionState::Foreground);
#endif
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
