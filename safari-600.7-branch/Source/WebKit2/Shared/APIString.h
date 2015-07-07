/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef APIString_h
#define APIString_h

#include "APIObject.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/UTF8.h>

namespace API {

class String final : public ObjectImpl<Object::Type::String> {
public:
    static PassRefPtr<String> createNull()
    {
        return adoptRef(new String);
    }

    static PassRefPtr<String> create(const WTF::String& string)
    {
        return adoptRef(new String(string));
    }

    static PassRefPtr<String> create(JSStringRef jsStringRef)
    {
        return adoptRef(new String(jsStringRef->string()));
    }

    static PassRefPtr<String> createFromUTF8String(const char* string)
    {
        return adoptRef(new String(WTF::String::fromUTF8(string)));
    }

    virtual ~String()
    {
    }

    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }
    
    size_t length() const { return m_string.length(); }
    size_t getCharacters(UChar* buffer, size_t bufferLength) const
    {
        unsigned unsignedBufferLength = std::min<size_t>(bufferLength, std::numeric_limits<unsigned>::max());
        StringView substring = StringView(m_string).substring(0, unsignedBufferLength);
        substring.getCharactersWithUpconvert(buffer);
        return substring.length();
    }

    size_t maximumUTF8CStringSize() const { return m_string.length() * 3 + 1; }
    size_t getUTF8CString(char* buffer, size_t bufferSize)
    {
        if (!bufferSize)
            return 0;
        char* p = buffer;
        WTF::Unicode::ConversionResult result;
        if (m_string.is8Bit()) {
            const LChar* characters = m_string.characters8();
            result = WTF::Unicode::convertLatin1ToUTF8(&characters, characters + m_string.length(), &p, p + bufferSize - 1);
        } else {
            const UChar* characters = m_string.characters16();
            result = WTF::Unicode::convertUTF16ToUTF8(&characters, characters + m_string.length(), &p, p + bufferSize - 1, /* strict */ true);
        }
        if (result != WTF::Unicode::conversionOK && result != WTF::Unicode::targetExhausted)
            return 0;
        *p++ = '\0';
        return p - buffer;
    }

    bool equal(String* other) { return m_string == other->m_string; }
    bool equalToUTF8String(const char* other) { return m_string == WTF::String::fromUTF8(other); }
    bool equalToUTF8StringIgnoringCase(const char* other) { return equalIgnoringCase(m_string, other); }

    const WTF::String& string() const { return m_string; }

    JSStringRef createJSString() const
    {
        JSC::initializeThreading();
        return OpaqueJSString::create(m_string).leakRef();
    }

private:
    String()
        : m_string()
    {
    }

    String(const WTF::String& string)
        : m_string(!string.impl() ? WTF::String(StringImpl::empty()) : string)
    {
    }

    WTF::String m_string;
};

} // namespace WebKit

#endif // APIString_h
