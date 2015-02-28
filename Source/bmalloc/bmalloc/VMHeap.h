/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#ifndef VMHeap_h
#define VMHeap_h

#include "AsyncTask.h"
#include "FixedVector.h"
#include "LargeChunk.h"
#include "LargeObject.h"
#include "MediumChunk.h"
#include "Range.h"
#include "SegregatedFreeList.h"
#include "SmallChunk.h"
#include "Vector.h"
#if BOS(DARWIN)
#include "Zone.h"
#endif

namespace bmalloc {

class BeginTag;
class EndTag;
class Heap;
class SuperChunk;

class VMHeap {
public:
    VMHeap();

    SmallPage* allocateSmallPage();
    MediumPage* allocateMediumPage();
    LargeObject allocateLargeRange(size_t);
    LargeObject allocateLargeRange(size_t alignment, size_t, size_t unalignedSize);

    void deallocateSmallPage(std::unique_lock<StaticMutex>&, SmallPage*);
    void deallocateMediumPage(std::unique_lock<StaticMutex>&, MediumPage*);
    void deallocateLargeRange(std::unique_lock<StaticMutex>&, LargeObject&);

private:
    void grow();

    Vector<SmallPage*> m_smallPages;
    Vector<MediumPage*> m_mediumPages;
    SegregatedFreeList m_largeObjects;
#if BOS(DARWIN)
    Zone m_zone;
#endif
};

inline SmallPage* VMHeap::allocateSmallPage()
{
    if (!m_smallPages.size())
        grow();

    return m_smallPages.pop();
}

inline MediumPage* VMHeap::allocateMediumPage()
{
    if (!m_mediumPages.size())
        grow();

    return m_mediumPages.pop();
}

inline LargeObject VMHeap::allocateLargeRange(size_t size)
{
    LargeObject largeObject = m_largeObjects.take(size);
    if (!largeObject) {
        grow();
        largeObject = m_largeObjects.take(size);
        BASSERT(largeObject);
    }
    return largeObject;
}

inline LargeObject VMHeap::allocateLargeRange(size_t alignment, size_t size, size_t unalignedSize)
{
    LargeObject largeObject = m_largeObjects.take(alignment, size, unalignedSize);
    if (!largeObject) {
        grow();
        largeObject = m_largeObjects.take(alignment, size, unalignedSize);
        BASSERT(largeObject);
    }
    return largeObject;
}

inline void VMHeap::deallocateSmallPage(std::unique_lock<StaticMutex>& lock, SmallPage* page)
{
    lock.unlock();
    vmDeallocatePhysicalPages(page->begin()->begin(), vmPageSize);
    lock.lock();
    
    m_smallPages.push(page);
}

inline void VMHeap::deallocateMediumPage(std::unique_lock<StaticMutex>& lock, MediumPage* page)
{
    lock.unlock();
    vmDeallocatePhysicalPages(page->begin()->begin(), vmPageSize);
    lock.lock();
    
    m_mediumPages.push(page);
}

inline void VMHeap::deallocateLargeRange(std::unique_lock<StaticMutex>& lock, LargeObject& largeObject)
{
    // Temporarily mark this range as allocated to prevent clients from merging
    // with it and then reallocating it while we're messing with its physical pages.
    largeObject.setFree(false);

    lock.unlock();
    vmDeallocatePhysicalPagesSloppy(largeObject.begin(), largeObject.size());
    lock.lock();

    largeObject.setFree(true);
    largeObject.setHasPhysicalPages(false);

    m_largeObjects.insert(largeObject);
}

} // namespace bmalloc

#endif // VMHeap_h
