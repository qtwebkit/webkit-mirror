/*
 * Copyright (C) 2004, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "TextGranularity.h"
#include "VisiblePosition.h"
#include <wtf/EnumTraits.h>

namespace WebCore {

enum class SelectionDirection : uint8_t { Forward, Backward, Right, Left };

class VisibleSelection {
public:
    WEBCORE_EXPORT VisibleSelection();

    static constexpr auto defaultAffinity = VisiblePosition::defaultAffinity;

    VisibleSelection(const Position&, Affinity, bool isDirectional = false);
    VisibleSelection(const Position&, const Position&, Affinity = defaultAffinity, bool isDirectional = false);
    WEBCORE_EXPORT VisibleSelection(const SimpleRange&, Affinity = defaultAffinity, bool isDirectional = false);
    WEBCORE_EXPORT VisibleSelection(const VisiblePosition&, bool isDirectional = false);
    WEBCORE_EXPORT VisibleSelection(const VisiblePosition&, const VisiblePosition&, bool isDirectional = false);

    WEBCORE_EXPORT static VisibleSelection selectionFromContentsOfNode(Node*);

    void setAffinity(Affinity affinity) { m_affinity = affinity; }
    Affinity affinity() const { return m_affinity; }

    void setBase(const Position&);
    void setBase(const VisiblePosition&);
    void setExtent(const Position&);
    void setExtent(const VisiblePosition&);

    Position base() const { return m_base; }
    Position extent() const { return m_extent; }
    Position start() const { return m_start; }
    Position end() const { return m_end; }
    
    VisiblePosition visibleStart() const { return VisiblePosition(m_start, isRange() ? Affinity::Downstream : affinity()); }
    VisiblePosition visibleEnd() const { return VisiblePosition(m_end, isRange() ? Affinity::Upstream : affinity()); }
    VisiblePosition visibleBase() const { return VisiblePosition(m_base, isRange() ? (isBaseFirst() ? Affinity::Upstream : Affinity::Downstream) : affinity()); }
    VisiblePosition visibleExtent() const { return VisiblePosition(m_extent, isRange() ? (isBaseFirst() ? Affinity::Downstream : Affinity::Upstream) : affinity()); }

    operator VisiblePositionRange() const { return { visibleStart(), visibleEnd() }; }

    bool isNone() const { return m_type == Type::None; }
    bool isCaret() const { return m_type == Type::Caret; }
    bool isRange() const { return m_type == Type::Range; }
    bool isCaretOrRange() const { return !isNone(); }
    bool isNonOrphanedRange() const { return isRange() && !start().isOrphan() && !end().isOrphan(); }
    bool isNoneOrOrphaned() const { return isNone() || start().isOrphan() || end().isOrphan(); }

    bool isBaseFirst() const { return m_baseIsFirst; }
    bool isDirectional() const { return m_isDirectional; }
    void setIsDirectional(bool isDirectional) { m_isDirectional = isDirectional; }

    WEBCORE_EXPORT bool isAll(EditingBoundaryCrossingRule) const;

    void appendTrailingWhitespace();

    WEBCORE_EXPORT bool expandUsingGranularity(TextGranularity granularity);

    // We don't yet support multi-range selections, so we only ever have one range to return.
    WEBCORE_EXPORT Optional<SimpleRange> firstRange() const;

    // FIXME: Most callers probably don't want this function, and should use firstRange instead.
    // toNormalizedRange is like firstRange, but contracts the range around text and moves the caret upstream before returning the range.
    WEBCORE_EXPORT Optional<SimpleRange> toNormalizedRange() const;

    WEBCORE_EXPORT Element* rootEditableElement() const;
    WEBCORE_EXPORT bool isContentEditable() const;
    WEBCORE_EXPORT bool hasEditableStyle() const;
    WEBCORE_EXPORT bool isContentRichlyEditable() const;
    // Returns a shadow tree node for legacy shadow trees, a child of the
    // ShadowRoot node for new shadow trees, or 0 for non-shadow trees.
    Node* nonBoundaryShadowTreeRootNode() const;

    WEBCORE_EXPORT bool isInPasswordField() const;
    
    WEBCORE_EXPORT static Position adjustPositionForEnd(const Position& currentPosition, Node* startContainerNode);
    WEBCORE_EXPORT static Position adjustPositionForStart(const Position& currentPosition, Node* startContainerNode);

#if ENABLE(TREE_DEBUGGING)
    void debugPosition() const;
    void formatForDebugger(char* buffer, unsigned length) const;
    void showTreeForThis() const;
#endif

    void setWithoutValidation(const Position&, const Position&);

private:
    void validate(TextGranularity = TextGranularity::CharacterGranularity);

    // Support methods for validate()
    void setBaseAndExtentToDeepEquivalents();
    void setStartAndEndFromBaseAndExtentRespectingGranularity(TextGranularity);
    void adjustSelectionToAvoidCrossingShadowBoundaries();
    void adjustSelectionToAvoidCrossingEditingBoundaries();
    void updateSelectionType();

    // We store these as Positions because VisibleSelection is used to store values in
    // editing commands for use when undoing the command. We need to be able to store
    // a selection that, while currently invalid, will be valid once the changes are undone.

    Position m_base; // Where the first click happened.
    Position m_extent; // Where the end click happened.

    Position m_start; // Leftmost position when expanded to respect granularity.
    Position m_end; // Rightmost position when expanded to respect granularity.

    Affinity m_affinity { defaultAffinity };

    // These are cached, can be recalculated by validate()
    enum class Type : uint8_t { None, Caret, Range };
    Type m_type { Type::None };
    bool m_baseIsFirst : 1; // True if base is before the extent.
    bool m_isDirectional : 1; // Non-directional ignores m_baseIsFirst and selection always extends on shift + arrow key.
};

inline bool operator==(const VisibleSelection& a, const VisibleSelection& b)
{
    return a.start() == b.start() && a.end() == b.end() && a.affinity() == b.affinity() && a.isBaseFirst() == b.isBaseFirst() && a.isDirectional() == b.isDirectional();
}

inline bool operator!=(const VisibleSelection& a, const VisibleSelection& b)
{
    return !(a == b);
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const VisibleSelection&);

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showTree(const WebCore::VisibleSelection&);
void showTree(const WebCore::VisibleSelection*);
#endif

namespace WTF {

template<> struct EnumTraits<WebCore::SelectionDirection> {
    using values = EnumValues<
        WebCore::SelectionDirection,
        WebCore::SelectionDirection::Forward,
        WebCore::SelectionDirection::Backward,
        WebCore::SelectionDirection::Right,
        WebCore::SelectionDirection::Left
    >;
};

} // namespace WTF
