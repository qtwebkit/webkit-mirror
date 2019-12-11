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

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineLine.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class LineLayout {
public:
    struct LineInput;
    LineLayout(const InlineFormattingContext&, Line::SkipAlignment, const LineInput&);

    struct LineContent;
    LineContent layout();

    struct PartialContent {
        // This will potentially gain some more members. 
        unsigned length;
    };
    struct LineInput {
        LineInput(const Line::InitialConstraints&, TextAlignMode horizontalAlignment, const InlineItems&, unsigned leadingInlineItemIndex, Optional<PartialContent> leadingPartialContent);

        Line::InitialConstraints initialConstraints;
        TextAlignMode horizontalAlignment;
        const InlineItems& inlineItems;
        unsigned leadingInlineItemIndex { 0 };
        Optional<PartialContent> leadingPartialContent;
        Optional<LayoutUnit> floatMinimumLogicalBottom;
    };

    struct LineContent {
        Optional<unsigned> trailingInlineItemIndex;
        Optional<PartialContent> trailingPartialContent;
        Vector<WeakPtr<InlineItem>> floats;
        const Line::RunList runList;
        const LineBox lineBox;
    };

    struct Run {
        const InlineItem& inlineItem;
        LayoutUnit logicalWidth;
    };

    using RunList = Vector<Run, 30>;

private:
    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    enum class IsEndOfLine { No, Yes };
    IsEndOfLine placeInlineItem(const InlineItem&);
    void commitPendingContent();
    LineContent close();
    bool shouldProcessUncommittedContent(const InlineItem&) const;
    IsEndOfLine processUncommittedContent();
    
    struct UncommittedContent {
        void add(const InlineItem&, LayoutUnit logicalWidth);
        void reset();
        void trim(unsigned newSize);

        RunList& runs() { return m_uncommittedRuns; }
        const RunList& runs() const { return m_uncommittedRuns; }
        bool isEmpty() const { return m_uncommittedRuns.isEmpty(); }
        unsigned size() const { return m_uncommittedRuns.size(); }
        LayoutUnit width() const { return m_width; }

    private:
        RunList m_uncommittedRuns;
        LayoutUnit m_width;
    };

    const InlineFormattingContext& m_inlineFormattingContext;
    const LineInput& m_lineInput;
    Line m_line;
    bool m_lineHasIntrusiveFloat { false };
    UncommittedContent m_uncommittedContent;
    unsigned m_committedInlineItemCount { 0 };
    Vector<WeakPtr<InlineItem>> m_floats;
    std::unique_ptr<InlineTextItem> m_leadingPartialTextItem;
    std::unique_ptr<InlineTextItem> m_trailingPartialTextItem;
    Optional<unsigned> m_overflowTextLength;
};

}
}
#endif
