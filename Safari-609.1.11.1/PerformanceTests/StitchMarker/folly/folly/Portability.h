/*
 * Copyright 2017 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string.h>

#include <cstddef>
#include <type_traits>

#include <folly/portability/Config.h>

#include <folly/CPortability.h>

// Unaligned loads and stores
namespace folly {
#if FOLLY_HAVE_UNALIGNED_ACCESS
constexpr bool kHasUnalignedAccess = true;
#else
constexpr bool kHasUnalignedAccess = false;
#endif

namespace portability_detail {

template <typename I, I A, I B>
using integral_max = std::integral_constant<I, (A < B) ? B : A>;

template <typename I, I A, I... Bs>
struct integral_sequence_max
    : integral_max<I, A, integral_sequence_max<I, Bs...>::value> {};

template <typename I, I A>
struct integral_sequence_max<I, A> : std::integral_constant<I, A> {};

template <typename... Ts>
using max_alignment = integral_sequence_max<size_t, alignof(Ts)...>;

using max_basic_alignment = max_alignment<
    std::max_align_t,
    long double,
    double,
    float,
    long long int,
    long int,
    int,
    short int,
    bool,
    char,
    char16_t,
    char32_t,
    wchar_t,
    std::nullptr_t>;
} // namespace detail

constexpr size_t max_align_v = portability_detail::max_basic_alignment::value;

// max_align_t is a type which is aligned at least as strictly as the
// most-aligned basic type (see the specification of std::max_align_t). This
// implementation exists because 32-bit iOS platforms have a broken
// std::max_align_t (see below).
//
// You should refer to this as `::folly::max_align_t` in portable code, even if
// you have `using namespace folly;` because C11 defines a global namespace
// `max_align_t` type.
//
// To be certain, we consider every non-void fundamental type specified by the
// standard. On most platforms `long double` would be enough, but iOS 32-bit
// has an 8-byte aligned `double` and `long long int` and a 4-byte aligned
// `long double`.
//
// So far we've covered locals and other non-allocated storage, but we also need
// confidence that allocated storage from `malloc`, `new`, etc will also be
// suitable for objects with this alignment reuirement.
//
// Apple document that their implementation of malloc will issue 16-byte
// granularity chunks for small allocations (large allocations are page-size
// granularity and page-aligned). We think that allocated storage will be
// suitable for these objects based on the following assumptions:
//
// 1. 16-byte granularity also means 16-byte aligned.
// 2. `new` and other allocators follow the `malloc` rules.
//
// We also have some anecdotal evidence: we don't see lots of misaligned-storage
// crashes on 32-bit iOS apps that use `double`.
//
// Apple's allocation reference: http://bit.ly/malloc-small
struct alignas(max_align_v) max_align_t {};

} // namespace folly

// compiler specific attribute translation
// msvc should come first, so if clang is in msvc mode it gets the right defines

#if defined(__clang__) || defined(__GNUC__)
# define FOLLY_ALIGNED(size) __attribute__((__aligned__(size)))
#elif defined(_MSC_VER)
# define FOLLY_ALIGNED(size) __declspec(align(size))
#else
# error Cannot define FOLLY_ALIGNED on this platform
#endif
#define FOLLY_ALIGNED_MAX FOLLY_ALIGNED(::folly::max_align_v)

// NOTE: this will only do checking in msvc with versions that support /analyze
#if _MSC_VER
# ifdef _USE_ATTRIBUTES_FOR_SAL
#    undef _USE_ATTRIBUTES_FOR_SAL
# endif
/* nolint */
# define _USE_ATTRIBUTES_FOR_SAL 1
# include <sal.h> // @manual
# define FOLLY_PRINTF_FORMAT _Printf_format_string_
# define FOLLY_PRINTF_FORMAT_ATTR(format_param, dots_param) /**/
#else
# define FOLLY_PRINTF_FORMAT /**/
# define FOLLY_PRINTF_FORMAT_ATTR(format_param, dots_param) \
  __attribute__((__format__(__printf__, format_param, dots_param)))
#endif

// deprecated
#if defined(__clang__) || defined(__GNUC__)
# define FOLLY_DEPRECATED(msg) __attribute__((__deprecated__(msg)))
#elif defined(_MSC_VER)
# define FOLLY_DEPRECATED(msg) __declspec(deprecated(msg))
#else
# define FOLLY_DEPRECATED(msg)
#endif

// warn unused result
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(nodiscard)
#define FOLLY_NODISCARD [[nodiscard]]
#endif
#endif
#if !defined FOLLY_NODISCARD
#if defined(_MSC_VER) && (_MSC_VER >= 1700)
#define FOLLY_NODISCARD _Check_return_
#elif defined(__clang__) || defined(__GNUC__)
#define FOLLY_NODISCARD __attribute__((__warn_unused_result__))
#else
#define FOLLY_NODISCARD
#endif
#endif

// target
#ifdef _MSC_VER
# define FOLLY_TARGET_ATTRIBUTE(target)
#else
# define FOLLY_TARGET_ATTRIBUTE(target) __attribute__((__target__(target)))
#endif

// detection for 64 bit
#if defined(__x86_64__) || defined(_M_X64)
# define FOLLY_X64 1
#else
# define FOLLY_X64 0
#endif

#if defined(__aarch64__)
# define FOLLY_A64 1
#else
# define FOLLY_A64 0
#endif

#if defined (__powerpc64__)
# define FOLLY_PPC64 1
#else
# define FOLLY_PPC64 0
#endif

namespace folly {
constexpr bool kIsArchAmd64 = FOLLY_X64 == 1;
constexpr bool kIsArchAArch64 = FOLLY_A64 == 1;
constexpr bool kIsArchPPC64 = FOLLY_PPC64 == 1;
}

namespace folly {

#if FOLLY_SANITIZE_ADDRESS
constexpr bool kIsSanitizeAddress = true;
#else
constexpr bool kIsSanitizeAddress = false;
#endif

#if FOLLY_SANITIZE_THREAD
constexpr bool kIsSanitizeThread = true;
#else
constexpr bool kIsSanitizeThread = false;
#endif
}

// packing is very ugly in msvc
#ifdef _MSC_VER
# define FOLLY_PACK_ATTR /**/
# define FOLLY_PACK_PUSH __pragma(pack(push, 1))
# define FOLLY_PACK_POP __pragma(pack(pop))
#elif defined(__clang__) || defined(__GNUC__)
# define FOLLY_PACK_ATTR __attribute__((__packed__))
# define FOLLY_PACK_PUSH /**/
# define FOLLY_PACK_POP /**/
#else
# define FOLLY_PACK_ATTR /**/
# define FOLLY_PACK_PUSH /**/
# define FOLLY_PACK_POP /**/
#endif

// Generalize warning push/pop.
#if defined(_MSC_VER)
# define FOLLY_PUSH_WARNING __pragma(warning(push))
# define FOLLY_POP_WARNING __pragma(warning(pop))
// Disable the GCC warnings.
# define FOLLY_GCC_DISABLE_WARNING(warningName)
# define FOLLY_MSVC_DISABLE_WARNING(warningNumber) __pragma(warning(disable: warningNumber))
#elif defined(__clang__) || defined(__GNUC__)
# define FOLLY_PUSH_WARNING _Pragma("GCC diagnostic push")
# define FOLLY_POP_WARNING _Pragma("GCC diagnostic pop")
# define FOLLY_GCC_DISABLE_WARNING_INTERNAL2(warningName) #warningName
# define FOLLY_GCC_DISABLE_WARNING(warningName) \
  _Pragma(                                      \
  FOLLY_GCC_DISABLE_WARNING_INTERNAL2(GCC diagnostic ignored warningName))
// Disable the MSVC warnings.
# define FOLLY_MSVC_DISABLE_WARNING(warningNumber)
#else
# define FOLLY_PUSH_WARNING
# define FOLLY_POP_WARNING
# define FOLLY_GCC_DISABLE_WARNING(warningName)
# define FOLLY_MSVC_DISABLE_WARNING(warningNumber)
#endif

#ifdef FOLLY_HAVE_SHADOW_LOCAL_WARNINGS
#define FOLLY_GCC_DISABLE_NEW_SHADOW_WARNINGS        \
  FOLLY_GCC_DISABLE_WARNING("-Wshadow-compatible-local") \
  FOLLY_GCC_DISABLE_WARNING("-Wshadow-local")
#else
#define FOLLY_GCC_DISABLE_NEW_SHADOW_WARNINGS /* empty */
#endif

/* Platform specific TLS support
 * gcc implements __thread
 * msvc implements __declspec(thread)
 * the semantics are the same
 * (but remember __thread has different semantics when using emutls (ex. apple))
 */
#if defined(_MSC_VER)
# define FOLLY_TLS __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
# define FOLLY_TLS __thread
#else
# error cannot define platform specific thread local storage
#endif

#if FOLLY_MOBILE
#undef FOLLY_TLS
#endif

// It turns out that GNU libstdc++ and LLVM libc++ differ on how they implement
// the 'std' namespace; the latter uses inline namespaces. Wrap this decision
// up in a macro to make forward-declarations easier.
#if FOLLY_USE_LIBCPP
#include <__config> // @manual
#define FOLLY_NAMESPACE_STD_BEGIN     _LIBCPP_BEGIN_NAMESPACE_STD
#define FOLLY_NAMESPACE_STD_END       _LIBCPP_END_NAMESPACE_STD
#else
#define FOLLY_NAMESPACE_STD_BEGIN     namespace std {
#define FOLLY_NAMESPACE_STD_END       }
#endif

// If the new c++ ABI is used, __cxx11 inline namespace needs to be added to
// some types, e.g. std::list.
#if _GLIBCXX_USE_CXX11_ABI
#define FOLLY_GLIBCXX_NAMESPACE_CXX11_BEGIN \
  inline _GLIBCXX_BEGIN_NAMESPACE_CXX11
# define FOLLY_GLIBCXX_NAMESPACE_CXX11_END   _GLIBCXX_END_NAMESPACE_CXX11
#else
# define FOLLY_GLIBCXX_NAMESPACE_CXX11_BEGIN
# define FOLLY_GLIBCXX_NAMESPACE_CXX11_END
#endif

// MSVC specific defines
// mainly for posix compat
#ifdef _MSC_VER
#include <folly/portability/SysTypes.h>

// compiler specific to compiler specific
// nolint
# define __PRETTY_FUNCTION__ __FUNCSIG__

// Hide a GCC specific thing that breaks MSVC if left alone.
# define __extension__

// We have compiler support for the newest of the new, but
// MSVC doesn't tell us that.
#define __SSE4_2__ 1

#endif

// Debug
namespace folly {
#ifdef NDEBUG
constexpr auto kIsDebug = false;
#else
constexpr auto kIsDebug = true;
#endif
}

// Endianness
namespace folly {
#ifdef _MSC_VER
// It's MSVC, so we just have to guess ... and allow an override
#ifdef FOLLY_ENDIAN_BE
constexpr auto kIsLittleEndian = false;
#else
constexpr auto kIsLittleEndian = true;
#endif
#else
constexpr auto kIsLittleEndian = __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
#endif
constexpr auto kIsBigEndian = !kIsLittleEndian;
}

#ifndef FOLLY_SSE
# if defined(__SSE4_2__)
#  define FOLLY_SSE 4
#  define FOLLY_SSE_MINOR 2
# elif defined(__SSE4_1__)
#  define FOLLY_SSE 4
#  define FOLLY_SSE_MINOR 1
# elif defined(__SSE4__)
#  define FOLLY_SSE 4
#  define FOLLY_SSE_MINOR 0
# elif defined(__SSE3__)
#  define FOLLY_SSE 3
#  define FOLLY_SSE_MINOR 0
# elif defined(__SSE2__)
#  define FOLLY_SSE 2
#  define FOLLY_SSE_MINOR 0
# elif defined(__SSE__)
#  define FOLLY_SSE 1
#  define FOLLY_SSE_MINOR 0
# else
#  define FOLLY_SSE 0
#  define FOLLY_SSE_MINOR 0
# endif
#endif

#define FOLLY_SSE_PREREQ(major, minor) \
  (FOLLY_SSE > major || FOLLY_SSE == major && FOLLY_SSE_MINOR >= minor)

#if FOLLY_UNUSUAL_GFLAGS_NAMESPACE
namespace FOLLY_GFLAGS_NAMESPACE { }
namespace gflags {
using namespace FOLLY_GFLAGS_NAMESPACE;
}  // namespace gflags
#endif

// for TARGET_OS_IPHONE
#ifdef __APPLE__
#include <TargetConditionals.h> // @manual
#endif

// RTTI may not be enabled for this compilation unit.
#if defined(__GXX_RTTI) || defined(__cpp_rtti) || \
    (defined(_MSC_VER) && defined(_CPPRTTI))
# define FOLLY_HAS_RTTI 1
#endif

#if defined(__APPLE__) || defined(_MSC_VER)
#define FOLLY_STATIC_CTOR_PRIORITY_MAX
#else
// 101 is the highest priority allowed by the init_priority attribute.
// This priority is already used by JEMalloc and other memory allocators so
// we will take the next one.
#define FOLLY_STATIC_CTOR_PRIORITY_MAX __attribute__((__init_priority__(102)))
#endif

namespace folly {

#if __OBJC__
constexpr auto kIsObjC = true;
#else
constexpr auto kIsObjC = false;
#endif

#if defined(__linux__) && !FOLLY_MOBILE
constexpr auto kIsLinux = true;
#else
constexpr auto kIsLinux = false;
#endif

#if defined(_WIN32)
constexpr auto kIsWindows = true;
constexpr auto kMscVer = _MSC_VER;
#else
constexpr auto kIsWindows = false;
constexpr auto kMscVer = 0;
#endif
}
