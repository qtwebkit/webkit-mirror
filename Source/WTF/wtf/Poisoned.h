/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include <utility>
#include <wtf/Assertions.h>

#define ENABLE_POISON 1
#define ENABLE_POISON_ASSERTS 0

// Older versions of gcc and clang have a bug which results in build failures
// when using template methods that take an argument of PoisonedImpl<K2, k2, T2>
// when the KeyType is a uintptr_t (i.e. when we're using the Poisoned variant
// of PoisonedImpl). This bug does not manifest for the ConstExprPoisoned variant.
// In practice, we will likely only use these methods for instantiations of the
// ConstExprPoisoned variant. Hence. this bug is not a show stopper.
// That said, we'll define ENABLE_MIXED_POISON accordingly so that we can use
// it to disable the affected tests when building with old compilers.

#if OS(DARWIN)
#define ENABLE_MIXED_POISON (__clang_major__ >= 9)
#elif defined(__clang_major__)
#define ENABLE_MIXED_POISON (__clang_major__ >= 4)
#elif defined(__GNUC__)
#include <features.h>
#define ENABLE_MIXED_POISON (__GNUC_PREREQ(7, 2))
#endif // !defined(__GNUC__)

#ifndef ENABLE_MIXED_POISON
#define ENABLE_MIXED_POISON 0 // Disable for everything else.
#endif

// Not currently supported for 32-bit or OS(WINDOWS) builds (because of missing llint support).
// Make sure it's disabled.
#if USE(JSVALUE32_64) || OS(WINDOWS)
#undef ENABLE_POISON
#define ENABLE_POISON 0
#undef ENABLE_POISON_ASSERTS
#define ENABLE_POISON_ASSERTS 0
#endif

namespace WTF {

using PoisonedBits = uintptr_t;

namespace PoisonedImplHelper {

template<typename T>
struct isFunctionPointer : std::integral_constant<bool, std::is_function<typename std::remove_pointer<T>::type>::value> { };

template<typename T>
struct isVoidPointer : std::integral_constant<bool, std::is_void<typename std::remove_pointer<T>::type>::value> { };

template<typename T>
struct isConvertibleToReference : std::integral_constant<bool, !isFunctionPointer<T>::value && !isVoidPointer<T>::value> { };

template<typename T>
typename std::enable_if_t<!isConvertibleToReference<T>::value, int>&
asReference(T) { RELEASE_ASSERT_NOT_REACHED(); }

template<typename T>
typename std::enable_if_t<isConvertibleToReference<T>::value, typename std::remove_pointer<T>::type>&
asReference(T ptr) { return *ptr; }

} // namespace PoisonedImplHelper

// FIXME: once we have C++17, we can make the key of type auto and remove the need for KeyType.
template<typename KeyType, KeyType key, typename T, typename = std::enable_if_t<std::is_pointer<T>::value>>
class PoisonedImpl {
public:
    PoisonedImpl() { }

    PoisonedImpl(T ptr)
        : m_poisonedBits(poison(ptr))
    { }

    PoisonedImpl(const PoisonedImpl&) = default;

    template<typename K2, K2 k2, typename T2>
    PoisonedImpl(const PoisonedImpl<K2, k2, T2>& other)
        : m_poisonedBits(poison<T>(other.unpoisoned()))
    { }

    PoisonedImpl(PoisonedImpl&& other)
        : m_poisonedBits(WTFMove(other.m_poisonedBits))
    { }

    explicit PoisonedImpl(PoisonedBits poisonedBits)
        : m_poisonedBits(poisonedBits)
    { }

#if ENABLE(POISON_ASSERTS)
    template<typename U = void*>
    static bool isPoisoned(U value) { return !value || (reinterpret_cast<uintptr_t>(value) & 0xffff000000000000); }
    template<typename U = void*>
    static void assertIsPoisoned(U value) { RELEASE_ASSERT(isPoisoned(value)); }
    template<typename U = void*>
    static void assertIsNotPoisoned(U value) { RELEASE_ASSERT(!isPoisoned(value)); }
#else
    template<typename U = void*> static void assertIsPoisoned(U) { }
    template<typename U = void*> static void assertIsNotPoisoned(U) { }
#endif
    void assertIsPoisoned() const { assertIsPoisoned(m_poisonedBits); }
    void assertIsNotPoisoned() const { assertIsNotPoisoned(m_poisonedBits); }

    template<typename U = T>
    U unpoisoned() const { return unpoison<U>(m_poisonedBits); }

    void clear() { m_poisonedBits = 0; }

    auto& operator*() const { ASSERT(m_poisonedBits); return PoisonedImplHelper::asReference(unpoison(m_poisonedBits)); }
    ALWAYS_INLINE T operator->() const { return unpoison(m_poisonedBits); }

    template<typename U = PoisonedBits>
    U bits() const { return bitwise_cast<U>(m_poisonedBits); }

    bool operator!() const { return !m_poisonedBits; }
    explicit operator bool() const { return !!m_poisonedBits; }

    bool operator==(const PoisonedImpl& b) const { return m_poisonedBits == b.m_poisonedBits; }
    bool operator!=(const PoisonedImpl& b) const { return m_poisonedBits != b.m_poisonedBits; }
    bool operator<(const PoisonedImpl& b) const { return m_poisonedBits < b.m_poisonedBits; }
    bool operator<=(const PoisonedImpl& b) const { return m_poisonedBits <= b.m_poisonedBits; }
    bool operator>(const PoisonedImpl& b) const { return m_poisonedBits > b.m_poisonedBits; }
    bool operator>=(const PoisonedImpl& b) const { return m_poisonedBits >= b.m_poisonedBits; }

    template<typename U> bool operator==(U b) const { return unpoisoned<U>() == b; }
    template<typename U> bool operator!=(U b) const { return unpoisoned<U>() != b; }
    template<typename U> bool operator<(U b) const { return unpoisoned<U>() < b; }
    template<typename U> bool operator<=(U b) const { return unpoisoned<U>() <= b; }
    template<typename U> bool operator>(U b) const { return unpoisoned<U>() > b; }
    template<typename U> bool operator>=(U b) const { return unpoisoned<U>() >= b; }

    PoisonedImpl& operator=(T ptr)
    {
        m_poisonedBits = poison(ptr);
        return *this;
    }
    PoisonedImpl& operator=(const PoisonedImpl&) = default;

    template<typename K2, K2 k2, typename T2>
    PoisonedImpl& operator=(const PoisonedImpl<K2, k2, T2>& other)
    {
        m_poisonedBits = poison<T>(other.unpoisoned());
        return *this;
    }

    void swap(PoisonedImpl& o)
    {
        std::swap(m_poisonedBits, o.m_poisonedBits);
    }

    template<typename K2, K2 k2, typename T2>
    void swap(PoisonedImpl<K2, k2, T2>& o)
    {
        T t1 = this->unpoisoned();
        T2 t2 = o.unpoisoned();
        std::swap(t1, t2);
        m_poisonedBits = poison(t1);
        o = t2;
    }

    void swap(T& t2)
    {
        T t1 = this->unpoisoned();
        std::swap(t1, t2);
        m_poisonedBits = poison(t1);
    }

    template<class U>
    T exchange(U&& newValue)
    {
        T oldValue = unpoisoned();
        m_poisonedBits = poison(std::forward<U>(newValue));
        return oldValue;
    }

private:
#if ENABLE(POISON)
    template<typename U>
    ALWAYS_INLINE static PoisonedBits poison(U ptr) { return ptr ? bitwise_cast<PoisonedBits>(ptr) ^ key : 0; }
    template<typename U = T>
    ALWAYS_INLINE static U unpoison(PoisonedBits poisonedBits) { return poisonedBits ? bitwise_cast<U>(poisonedBits ^ key) : bitwise_cast<U>(0ll); }
#else
    template<typename U>
    ALWAYS_INLINE static PoisonedBits poison(U ptr) { return bitwise_cast<PoisonedBits>(ptr); }
    template<typename U = T>
    ALWAYS_INLINE static U unpoison(PoisonedBits poisonedBits) { return bitwise_cast<U>(poisonedBits); }
#endif

    PoisonedBits m_poisonedBits { 0 };
};

template<typename K1, K1 k1, typename T1, typename K2, K2 k2, typename T2>
inline void swap(PoisonedImpl<K1, k1, T1>& a, PoisonedImpl<K2, k2, T2>& b)
{
    a.swap(b);
}

template<typename K1, K1 k1, typename T1>
inline void swap(PoisonedImpl<K1, k1, T1>& a, T1& b)
{
    a.swap(b);
}

WTF_EXPORT_PRIVATE uintptr_t makePoison();

#if ENABLE(POISON)
// FIXME: once we have C++17, we can make this a lambda in makeConstExprPoison().
inline constexpr uintptr_t constExprPoisonRandom(uintptr_t seed)
{
    uintptr_t result1 = seed * 6906969069 + 1234567;
    uintptr_t result2 = seed * 8253729 + 2396403;
    return (result1 << 3) ^ (result2 << 16);
}
#endif

inline constexpr uintptr_t makeConstExprPoison(uint32_t key)
{
#if ENABLE(POISON)
    uintptr_t poison = constExprPoisonRandom(key);
    poison &= ~(static_cast<uintptr_t>(0xffff) << 48); // Ensure that poisoned bits look like a pointer.
    poison |= (1 << 2); // Ensure that poisoned bits cannot be 0.
    return poison;
#else
    return (void)key, 0;
#endif
}

template<uintptr_t& key, typename T>
using Poisoned = PoisonedImpl<uintptr_t&, key, T>;

template<uint32_t key, typename T>
using ConstExprPoisoned = PoisonedImpl<uintptr_t, makeConstExprPoison(key), T>;

template<uint32_t key, typename T>
struct ConstExprPoisonedPtrTraits {
    using StorageType = ConstExprPoisoned<key, T*>;

    template<class U> static ALWAYS_INLINE T* exchange(StorageType& ptr, U&& newValue) { return ptr.exchange(newValue); }

    template<typename K1, K1 k1, typename T1>
    static ALWAYS_INLINE void swap(PoisonedImpl<K1, k1, T1>& a, T1& b) { a.swap(b); }

    template<typename K1, K1 k1, typename T1, typename K2, K2 k2, typename T2>
    static ALWAYS_INLINE void swap(PoisonedImpl<K1, k1, T1>& a, PoisonedImpl<K2, k2, T2>& b) { a.swap(b); }

    static ALWAYS_INLINE T* unwrap(const StorageType& ptr) { return ptr.unpoisoned(); }
};

} // namespace WTF

using WTF::ConstExprPoisoned;
using WTF::Poisoned;
using WTF::PoisonedBits;
using WTF::makeConstExprPoison;
using WTF::makePoison;
