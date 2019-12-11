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

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayRun.h"
#include "FormattingState.h"
#include "InlineItem.h"
#include "InlineLineBox.h"
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>

namespace WebCore {
namespace Layout {

// Temp
using InlineItems = Vector<std::unique_ptr<InlineItem>, 30>;
using InlineRuns = Vector<std::unique_ptr<Display::Run>, 10>;
using LineBoxes = Vector<std::unique_ptr<LineBox>, 5>;
// InlineFormattingState holds the state for a particular inline formatting context tree.
class InlineFormattingState : public FormattingState {
    WTF_MAKE_ISO_ALLOCATED(InlineFormattingState);
public:
    InlineFormattingState(Ref<FloatingState>&&, LayoutState&);
    virtual ~InlineFormattingState();

    InlineItems& inlineItems() { return m_inlineItems; }
    const InlineItems& inlineItems() const { return m_inlineItems; }
    void addInlineItem(std::unique_ptr<InlineItem>&& inlineItem) { m_inlineItems.append(WTFMove(inlineItem)); }

    const InlineRuns& inlineRuns() const { return m_inlineRuns; }
    InlineRuns& inlineRuns() { return m_inlineRuns; }
    void addInlineRun(std::unique_ptr<Display::Run>&&, const LineBox&);
    void resetInlineRuns();

    const LineBoxes& lineBoxes() const { return m_lineBoxes; }
    LineBoxes& lineBoxes() { return m_lineBoxes; }
    void addLineBox(const LineBox& lineBox) { m_lineBoxes.append(makeUnique<LineBox>(lineBox)); }

    const LineBox& lineBoxForRun(const Display::Run& inlineRun) const { return *m_inlineRunToLineMap.get(&inlineRun); }

private:
    InlineItems m_inlineItems;
    InlineRuns m_inlineRuns;
    LineBoxes m_lineBoxes;
    // This is temporary until after we figure out the display run/line relationships.
    HashMap<const Display::Run*, const LineBox*> m_inlineRunToLineMap;
};

inline void InlineFormattingState::addInlineRun(std::unique_ptr<Display::Run>&& displayRun, const LineBox& line)
{
    m_inlineRunToLineMap.set(displayRun.get(), &line);
    m_inlineRuns.append(WTFMove(displayRun));
}

inline void InlineFormattingState::resetInlineRuns()
{
    m_inlineRuns.clear();
    // Resetting the runs means no more line boxes either.
    m_lineBoxes.clear();
    m_inlineRunToLineMap.clear();

}

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_STATE(InlineFormattingState, isInlineFormattingState())

#endif
