/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016 Metrological Group B.V.
 * Copyright (C) 2015, 2016 Igalia, S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SourceBufferPrivateGStreamer.h"

#if ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "ContentType.h"
#include "GStreamerCommon.h"
#include "MediaPlayerPrivateGStreamerMSE.h"
#include "MediaSample.h"
#include "MediaSourceClientGStreamerMSE.h"
#include "MediaSourceGStreamer.h"
#include "NotImplemented.h"
#include "WebKitMediaSourceGStreamer.h"

GST_DEBUG_CATEGORY_EXTERN(webkit_mse_debug);
#define GST_CAT_DEFAULT webkit_mse_debug

namespace WebCore {

Ref<SourceBufferPrivateGStreamer> SourceBufferPrivateGStreamer::create(MediaSourceGStreamer* mediaSource, Ref<MediaSourceClientGStreamerMSE> client, const ContentType& contentType)
{
    return adoptRef(*new SourceBufferPrivateGStreamer(mediaSource, client.get(), contentType));
}

SourceBufferPrivateGStreamer::SourceBufferPrivateGStreamer(MediaSourceGStreamer* mediaSource, Ref<MediaSourceClientGStreamerMSE> client, const ContentType& contentType)
    : SourceBufferPrivate()
    , m_mediaSource(mediaSource)
    , m_type(contentType)
    , m_client(client.get())
{
}

void SourceBufferPrivateGStreamer::setClient(SourceBufferPrivateClient* client)
{
    m_sourceBufferPrivateClient = client;
}

void SourceBufferPrivateGStreamer::append(Vector<unsigned char>&& data)
{
    ASSERT(m_mediaSource);

    if (!m_sourceBufferPrivateClient)
        return;

    m_client->append(this, WTFMove(data));
    m_sourceBufferPrivateClient->sourceBufferPrivateAppendComplete(SourceBufferPrivateClient::ReadStreamFailed);
}

void SourceBufferPrivateGStreamer::abort()
{
    m_client->abort(this);
}

void SourceBufferPrivateGStreamer::resetParserState()
{
    m_client->resetParserState(this);
}

void SourceBufferPrivateGStreamer::removedFromMediaSource()
{
    if (m_mediaSource)
        m_mediaSource->removeSourceBuffer(this);
    m_client->removedFromMediaSource(this);
}

MediaPlayer::ReadyState SourceBufferPrivateGStreamer::readyState() const
{
    return m_mediaSource->readyState();
}

void SourceBufferPrivateGStreamer::setReadyState(MediaPlayer::ReadyState state)
{
    m_mediaSource->setReadyState(state);
}

void SourceBufferPrivateGStreamer::flush(const AtomString& trackId)
{
    m_client->flush(trackId);
}

void SourceBufferPrivateGStreamer::enqueueSample(Ref<MediaSample>&& sample, const AtomString& trackId)
{
    m_client->enqueueSample(WTFMove(sample), trackId);
}

void SourceBufferPrivateGStreamer::allSamplesInTrackEnqueued(const AtomString& trackId)
{
    m_client->allSamplesInTrackEnqueued(trackId);
}

bool SourceBufferPrivateGStreamer::isReadyForMoreSamples(const AtomString& trackId)
{
    ASSERT(WTF::isMainThread());
    bool isReadyForMoreSamples = m_client->isReadyForMoreSamples(trackId);
    GST_DEBUG("SourceBufferPrivate(%p) - isReadyForMoreSamples: %d", this, (int) isReadyForMoreSamples);
    return isReadyForMoreSamples;
}

void SourceBufferPrivateGStreamer::setActive(bool isActive)
{
    if (m_mediaSource)
        m_mediaSource->sourceBufferPrivateDidChangeActiveState(this, isActive);
}

void SourceBufferPrivateGStreamer::notifyClientWhenReadyForMoreSamples(const AtomString& trackId)
{
    ASSERT(WTF::isMainThread());
    return m_client->notifyClientWhenReadyForMoreSamples(trackId, m_sourceBufferPrivateClient);
}

void SourceBufferPrivateGStreamer::didReceiveInitializationSegment(const SourceBufferPrivateClient::InitializationSegment& initializationSegment)
{
    if (m_sourceBufferPrivateClient)
        m_sourceBufferPrivateClient->sourceBufferPrivateDidReceiveInitializationSegment(initializationSegment);
}

void SourceBufferPrivateGStreamer::didReceiveSample(MediaSample& sample)
{
    if (m_sourceBufferPrivateClient)
        m_sourceBufferPrivateClient->sourceBufferPrivateDidReceiveSample(sample);
}

void SourceBufferPrivateGStreamer::didReceiveAllPendingSamples()
{
    if (m_sourceBufferPrivateClient)
        m_sourceBufferPrivateClient->sourceBufferPrivateAppendComplete(SourceBufferPrivateClient::AppendSucceeded);
}

void SourceBufferPrivateGStreamer::appendParsingFailed()
{
    if (m_sourceBufferPrivateClient)
        m_sourceBufferPrivateClient->sourceBufferPrivateAppendComplete(SourceBufferPrivateClient::ParsingFailed);
}

}
#endif
