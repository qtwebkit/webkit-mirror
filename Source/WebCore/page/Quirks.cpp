/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "Quirks.h"

#include "CustomHeaderFields.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "HTMLMetaElement.h"
#include "HTMLObjectElement.h"
#include "LayoutUnit.h"
#include "Settings.h"

namespace WebCore {

static inline OptionSet<AutoplayQuirk> allowedAutoplayQuirks(Document& document)
{
    auto* loader = document.loader();
    if (!loader)
        return { };

    return loader->allowedAutoplayQuirks();
}

Quirks::Quirks(Document& document)
    : m_document(makeWeakPtr(document))
{
}

Quirks::~Quirks() = default;

inline bool Quirks::needsQuirks() const
{
    return m_document && m_document->settings().needsSiteSpecificQuirks();
}

bool Quirks::shouldIgnoreInvalidSignal() const
{
    return needsQuirks();
}

bool Quirks::needsFormControlToBeMouseFocusable() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return equalLettersIgnoringASCIICase(host, "ceac.state.gov") || host.endsWithIgnoringASCIICase(".ceac.state.gov");
#else
    return false;
#endif
}

bool Quirks::needsAutoplayPlayPauseEvents() const
{
    if (!needsQuirks())
        return false;

    if (allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::SynthesizedPauseEvents))
        return true;

    return allowedAutoplayQuirks(m_document->topDocument()).contains(AutoplayQuirk::SynthesizedPauseEvents);
}

bool Quirks::needsSeekingSupportDisabled() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "netflix.com") || host.endsWithIgnoringASCIICase(".netflix.com");
}

bool Quirks::needsPerDocumentAutoplayBehavior() const
{
#if PLATFORM(MAC)
    ASSERT(m_document == &m_document->topDocument());
    return needsQuirks() && allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::PerDocumentAutoplayBehavior);
#else
    return false;
#endif
}

bool Quirks::shouldAutoplayForArbitraryUserGesture() const
{
#if PLATFORM(MAC)
    return needsQuirks() && allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::ArbitraryUserGestures);
#else
    return false;
#endif
}

bool Quirks::hasBrokenEncryptedMediaAPISupportQuirk() const
{
    if (!needsQuirks())
        return false;

    if (m_hasBrokenEncryptedMediaAPISupportQuirk)
        return m_hasBrokenEncryptedMediaAPISupportQuirk.value();

    auto domain = m_document->securityOrigin().domain().convertToASCIILowercase();

    m_hasBrokenEncryptedMediaAPISupportQuirk = domain == "starz.com"
        || domain.endsWith(".starz.com")
        || domain == "youtube.com"
        || domain.endsWith(".youtube.com")
        || domain == "hulu.com"
        || domain.endsWith("hulu.com");

    return m_hasBrokenEncryptedMediaAPISupportQuirk.value();
}

bool Quirks::isTouchBarUpdateSupressedForHiddenContentEditable() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "docs.google.com");
#else
    return false;
#endif
}

bool Quirks::isNeverRichlyEditableForTouchBar() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    auto& url = m_document->topDocument().url();
    auto host = url.host();

    if (equalLettersIgnoringASCIICase(host, "twitter.com"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "onedrive.live.com"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "trix-editor.org"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.icloud.com")) {
        auto path = url.path();
        if (path.contains("notes") || url.fragmentIdentifier().contains("notes"))
            return true;
    }
#endif

    return false;
}

static bool shouldSuppressAutocorrectionAndAutocaptializationInHiddenEditableAreasForHost(const StringView& host)
{
#if PLATFORM(IOS_FAMILY)
    return equalLettersIgnoringASCIICase(host, "docs.google.com");
#else
    UNUSED_PARAM(host);
    return false;
#endif
}

bool Quirks::shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const
{
    if (m_document->settings().shouldDispatchSyntheticMouseEventsWhenModifyingSelection())
        return true;

    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    if (equalLettersIgnoringASCIICase(host, "medium.com") || host.endsWithIgnoringASCIICase(".medium.com"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "weebly.com") || host.endsWithIgnoringASCIICase(".weebly.com"))
        return true;

    return false;
}

bool Quirks::needsYouTubeMouseOutQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (m_document->settings().shouldDispatchSyntheticMouseOutAfterSyntheticClick())
        return true;

    if (!needsQuirks())
        return false;

    return equalLettersIgnoringASCIICase(m_document->url().host(), "www.youtube.com");
#else
    return false;
#endif
}

bool Quirks::shouldSuppressAutocorrectionAndAutocaptializationInHiddenEditableAreas() const
{
    if (!needsQuirks())
        return false;

    return shouldSuppressAutocorrectionAndAutocaptializationInHiddenEditableAreasForHost(m_document->topDocument().url().host());
}

#if ENABLE(TOUCH_EVENTS)
bool Quirks::isAmazon() const
{
    auto& url = m_document->topDocument().url();
    auto host = url.host();
    return equalLettersIgnoringASCIICase(host, "amazon.com") || host.endsWithIgnoringASCIICase(".amazon.com");
}

bool Quirks::shouldDispatchSimulatedMouseEvents() const
{
    if (!needsQuirks())
        return false;

    auto* loader = m_document->loader();
    if (!loader || loader->simulatedMouseEventsDispatchPolicy() != SimulatedMouseEventsDispatchPolicy::Allow)
        return false;

    if (isAmazon())
        return true;

    auto& url = m_document->topDocument().url();
    auto host = url.host();

    if (equalLettersIgnoringASCIICase(host, "wix.com") || host.endsWithIgnoringASCIICase(".wix.com"))
        return true;
    if ((equalLettersIgnoringASCIICase(host, "desmos.com") || host.endsWithIgnoringASCIICase(".desmos.com")) && url.path().startsWithIgnoringASCIICase("/calculator/"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "figma.com") || host.endsWithIgnoringASCIICase(".figma.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "trello.com") || host.endsWithIgnoringASCIICase(".trello.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "airtable.com") || host.endsWithIgnoringASCIICase(".airtable.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "msn.com") || host.endsWithIgnoringASCIICase(".msn.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "flipkart.com") || host.endsWithIgnoringASCIICase(".flipkart.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "www.google.com") && url.path().startsWithIgnoringASCIICase("/maps/"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "trailers.apple.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "naver.com"))
        return true;
    // Disable the quirk for tv.naver.com subdomain to be able to simulate hover on videos.
    if (host.endsWithIgnoringASCIICase(".naver.com"))
        return !equalLettersIgnoringASCIICase(host, "tv.naver.com");
    return false;
}

bool Quirks::shouldDispatchedSimulatedMouseEventsAssumeDefaultPrevented(EventTarget* target) const
{
    if (!needsQuirks() || !shouldDispatchSimulatedMouseEvents())
        return false;

    if (isAmazon() && is<Element>(target)) {
        // When panning on an Amazon product image, we're either touching on the #magnifierLens element
        // or its previous sibling.
        auto* element = downcast<Element>(target);
        if (element->getIdAttribute() == "magnifierLens")
            return true;
        if (auto* sibling = element->nextElementSibling())
            return sibling->getIdAttribute() == "magnifierLens";
    }

    return false;
}

Optional<Event::IsCancelable> Quirks::simulatedMouseEventTypeForTarget(EventTarget* target) const
{
    if (!needsQuirks() || !shouldDispatchSimulatedMouseEvents())
        return { };

    // On Google Maps, we want to limit simulated mouse events to dragging the little man that allows entering into Street View.
    auto& url = m_document->topDocument().url();
    auto host = url.host();
    if (equalLettersIgnoringASCIICase(host, "www.google.com") && url.path().startsWithIgnoringASCIICase("/maps/")) {
        if (is<Element>(target) && downcast<Element>(target)->getAttribute("class") == "widget-expand-button-pegman-icon")
            return Event::IsCancelable::Yes;
        return { };
    }

    if (equalLettersIgnoringASCIICase(host, "desmos.com") || host.endsWithIgnoringASCIICase(".desmos.com"))
        return Event::IsCancelable::No;

    return Event::IsCancelable::Yes;
}
#endif

bool Quirks::shouldAvoidResizingWhenInputViewBoundsChange() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    if (equalLettersIgnoringASCIICase(host, "live.com") || host.endsWithIgnoringASCIICase(".live.com"))
        return true;

    if (host.endsWithIgnoringASCIICase(".sharepoint.com"))
        return true;

    return false;
}

bool Quirks::shouldDisablePointerEventsQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    auto& url = m_document->topDocument().url();
    auto host = url.host();
    if (equalLettersIgnoringASCIICase(host, "mailchimp.com") || host.endsWithIgnoringASCIICase(".mailchimp.com"))
        return true;
#endif
    return false;
}

bool Quirks::needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommand() const
{
#if PLATFORM(IOS_FAMILY)
    if (m_document->settings().needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk())
        return true;

    if (!needsQuirks())
        return false;

    auto& url = m_document->topDocument().url();
    return equalLettersIgnoringASCIICase(url.host(), "docs.google.com") && url.path().startsWithIgnoringASCIICase("/spreadsheets/");
#else
    return false;
#endif
}

// FIXME(<rdar://problem/50394969>): Remove after desmos.com adopts inputmode="none".
bool Quirks::needsInputModeNoneImplicitly(const HTMLElement& element) const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (element.hasTagName(HTMLNames::inputTag)) {
        if (!equalLettersIgnoringASCIICase(m_document->url().host(), "calendar.google.com"))
            return false;
        static NeverDestroyed<QualifiedName> dataInitialValueAttr(nullAtom(), "data-initial-value", nullAtom());
        static NeverDestroyed<QualifiedName> dataPreviousValueAttr(nullAtom(), "data-previous-value", nullAtom());

        return equalLettersIgnoringASCIICase(element.attributeWithoutSynchronization(HTMLNames::autocompleteAttr), "off")
            && element.hasAttributeWithoutSynchronization(dataInitialValueAttr)
            && element.hasAttributeWithoutSynchronization(dataPreviousValueAttr);
    }

    if (!element.hasTagName(HTMLNames::textareaTag))
        return false;

    auto& url = m_document->url();
    auto host = url.host();
    if (!host.endsWithIgnoringASCIICase(".desmos.com"))
        return false;

    return element.parentElement() && element.parentElement()->classNames().contains("dcg-mq-textarea");
#else
    UNUSED_PARAM(element);
    return false;
#endif
}

// FIXME: Remove after the site is fixed, <rdar://problem/50374200>
bool Quirks::needsGMailOverflowScrollQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (!m_needsGMailOverflowScrollQuirk)
        m_needsGMailOverflowScrollQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "mail.google.com");

    return *m_needsGMailOverflowScrollQuirk;
#else
    return false;
#endif
}

// FIXME: Remove after the site is fixed, <rdar://problem/50374311>
bool Quirks::needsYouTubeOverflowScrollQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (!m_needsYouTubeOverflowScrollQuirk)
        m_needsYouTubeOverflowScrollQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "www.youtube.com");

    return *m_needsYouTubeOverflowScrollQuirk;
#else
    return false;
#endif
}

}
