/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2015, 2017 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RenderTreeBuilderMultiColumn.h"

#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderLinesClampFlow.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderTreeBuilder.h"

namespace WebCore {

RenderTreeBuilder::MultiColumn::MultiColumn(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::MultiColumn::updateAfterDescendants(RenderBlockFlow& flow)
{
    bool needsFragmentedFlow = flow.requiresColumns(flow.style().columnCount());
    bool hasFragmentedFlow = flow.multiColumnFlow();

    if (!hasFragmentedFlow && needsFragmentedFlow) {
        createFragmentedFlow(flow);
        return;
    }
    if (hasFragmentedFlow && !needsFragmentedFlow) {
        destroyFragmentedFlow(flow);
        return;
    }
}

void RenderTreeBuilder::MultiColumn::createFragmentedFlow(RenderBlockFlow& flow)
{
    flow.setChildrenInline(false); // Do this to avoid wrapping inline children that are just going to move into the flow thread.
    flow.deleteLines();
    // If this soon-to-be multicolumn flow is already part of a multicolumn context, we need to move back the descendant spanners
    // to their original position before moving subtrees around.
    auto* enclosingflow = flow.enclosingFragmentedFlow();
    if (is<RenderMultiColumnFlow>(enclosingflow)) {
        auto& spanners = downcast<RenderMultiColumnFlow>(enclosingflow)->spannerMap();
        Vector<RenderMultiColumnSpannerPlaceholder*> placeholdersToDelete;
        for (auto& spannerAndPlaceholder : spanners) {
            auto& placeholder = *spannerAndPlaceholder.value;
            if (!placeholder.isDescendantOf(&flow))
                continue;
            placeholdersToDelete.append(&placeholder);
        }
        for (auto* placeholder : placeholdersToDelete) {
            auto* spanner = placeholder->spanner();
            if (!spanner) {
                ASSERT_NOT_REACHED();
                continue;
            }
            // Move the spanner back to its original position.
            auto& spannerOriginalParent = *placeholder->parent();
            // Detaching the spanner takes care of removing the placeholder (and merges the RenderMultiColumnSets).
            auto spannerToReInsert = spanner->parent()->takeChild(m_builder, *spanner);
            m_builder.insertChild(spannerOriginalParent, WTFMove(spannerToReInsert));
        }
    }

    auto newFragmentedFlow = !flow.style().hasLinesClamp() ? WebCore::createRenderer<RenderMultiColumnFlow>(flow.document(), RenderStyle::createAnonymousStyleWithDisplay(flow.style(), BLOCK)) :  WebCore::createRenderer<RenderLinesClampFlow>(flow.document(), RenderStyle::createAnonymousStyleWithDisplay(flow.style(), BLOCK));
    newFragmentedFlow->initializeStyle();
    auto& fragmentedFlow = *newFragmentedFlow;
    m_builder.insertChildToRenderBlock(flow, WTFMove(newFragmentedFlow));

    // Reparent children preceding the fragmented flow into the fragmented flow.
    flow.moveChildrenTo(m_builder, &fragmentedFlow, flow.firstChild(), &fragmentedFlow, RenderBoxModelObject::NormalizeAfterInsertion::Yes);
    if (flow.isFieldset()) {
        // Keep legends out of the flow thread.
        for (auto& box : childrenOfType<RenderBox>(fragmentedFlow)) {
            if (box.isLegend())
                fragmentedFlow.moveChildTo(m_builder, &flow, &box, RenderBoxModelObject::NormalizeAfterInsertion::Yes);
        }
    }

    if (flow.style().hasLinesClamp()) {
        // Keep the middle block out of the flow thread.
        for (auto& element : childrenOfType<RenderElement>(fragmentedFlow)) {
            if (!downcast<RenderLinesClampFlow>(fragmentedFlow).isChildAllowedInFragmentedFlow(flow, element))
                fragmentedFlow.moveChildTo(m_builder, &flow, &element, RenderBoxModelObject::NormalizeAfterInsertion::Yes);
        }
    }

    flow.setMultiColumnFlow(fragmentedFlow);
}

void RenderTreeBuilder::MultiColumn::destroyFragmentedFlow(RenderBlockFlow& flow)
{
    auto& multiColumnFlow = *flow.multiColumnFlow();
    multiColumnFlow.deleteLines();

    // Move spanners back to their original DOM position in the tree, and destroy the placeholders.
    auto& spanners = multiColumnFlow.spannerMap();
    Vector<RenderMultiColumnSpannerPlaceholder*> placeholdersToDelete;
    for (auto& spannerAndPlaceholder : spanners)
        placeholdersToDelete.append(spannerAndPlaceholder.value.get());
    Vector<std::pair<RenderElement*, RenderPtr<RenderObject>>> parentAndSpannerList;
    for (auto* placeholder : placeholdersToDelete) {
        auto* spannerOriginalParent = placeholder->parent();
        if (spannerOriginalParent == &multiColumnFlow)
            spannerOriginalParent = &flow;
        // Detaching the spanner takes care of removing the placeholder (and merges the RenderMultiColumnSets).
        auto* spanner = placeholder->spanner();
        parentAndSpannerList.append(std::make_pair(spannerOriginalParent, spanner->parent()->takeChild(m_builder, *spanner)));
    }
    while (auto* columnSet = multiColumnFlow.firstMultiColumnSet())
        columnSet->removeFromParentAndDestroy();

    flow.clearMultiColumnFlow();
    multiColumnFlow.moveAllChildrenTo(m_builder, &flow, RenderBoxModelObject::NormalizeAfterInsertion::Yes);
    multiColumnFlow.removeFromParentAndDestroy();
    for (auto& parentAndSpanner : parentAndSpannerList)
        m_builder.insertChild(*parentAndSpanner.first, WTFMove(parentAndSpanner.second));
}


RenderObject* RenderTreeBuilder::MultiColumn::resolveMovedChild(RenderFragmentedFlow& enclosingFragmentedFlow, RenderObject* beforeChild)
{
    if (!beforeChild)
        return nullptr;

    if (!is<RenderBox>(*beforeChild))
        return beforeChild;

    if (!is<RenderMultiColumnFlow>(enclosingFragmentedFlow))
        return beforeChild;

    // We only need to resolve for column spanners.
    if (beforeChild->style().columnSpan() != ColumnSpanAll)
        return beforeChild;

    // The renderer for the actual DOM node that establishes a spanner is moved from its original
    // location in the render tree to becoming a sibling of the column sets. In other words, it's
    // moved out from the flow thread (and becomes a sibling of it). When we for instance want to
    // create and insert a renderer for the sibling node immediately preceding the spanner, we need
    // to map that spanner renderer to the spanner's placeholder, which is where the new inserted
    // renderer belongs.
    if (auto* placeholder = downcast<RenderMultiColumnFlow>(enclosingFragmentedFlow).findColumnSpannerPlaceholder(downcast<RenderBox>(beforeChild)))
        return placeholder;

    // This is an invalid spanner, or its placeholder hasn't been created yet. This happens when
    // moving an entire subtree into the flow thread, when we are processing the insertion of this
    // spanner's preceding sibling, and we obviously haven't got as far as processing this spanner
    // yet.
    return beforeChild;
}

}
