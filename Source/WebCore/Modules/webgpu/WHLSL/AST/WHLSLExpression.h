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

#if ENABLE(WEBGPU)

#include "WHLSLAddressSpace.h"
#include "WHLSLLexer.h"
#include "WHLSLUnnamedType.h"
#include "WHLSLValue.h"
#include <wtf/Optional.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class Expression : public Value {
    using Base = Value;
public:
    Expression(Lexer::Token&& origin)
        : Base(WTFMove(origin))
    {
    }

    virtual ~Expression() = default;

    Expression(const Expression&) = delete;
    Expression(Expression&&) = default;

    Expression& operator=(const Expression&) = delete;
    Expression& operator=(Expression&&) = default;

    UnnamedType* maybeResolvedType() { return m_type ? &*m_type : nullptr; }

    UnnamedType& resolvedType()
    {
        ASSERT(m_type);
        return *m_type;
    }

    void setType(UniqueRef<UnnamedType>&& type)
    {
        ASSERT(!m_type);
        m_type = WTFMove(type);
    }

    const TypeAnnotation* maybeTypeAnnotation() const { return m_typeAnnotation ? &*m_typeAnnotation : nullptr; }

    const TypeAnnotation& typeAnnotation() const
    {
        ASSERT(m_typeAnnotation);
        return *m_typeAnnotation;
    }

    void setTypeAnnotation(TypeAnnotation&& typeAnnotation)
    {
        ASSERT(!m_typeAnnotation);
        m_typeAnnotation = WTFMove(typeAnnotation);
    }

    void copyTypeTo(Expression& other) const
    {
        if (auto* resolvedType = const_cast<Expression*>(this)->maybeResolvedType())
            other.setType(resolvedType->clone());
        if (auto* typeAnnotation = maybeTypeAnnotation())
            other.setTypeAnnotation(TypeAnnotation(*typeAnnotation));
    }

    virtual bool isAssignmentExpression() const { return false; }
    virtual bool isBooleanLiteral() const { return false; }
    virtual bool isCallExpression() const { return false; }
    virtual bool isCommaExpression() const { return false; }
    virtual bool isDereferenceExpression() const { return false; }
    virtual bool isDotExpression() const { return false; }
    virtual bool isGlobalVariableReference() const { return false; }
    virtual bool isFloatLiteral() const { return false; }
    virtual bool isIndexExpression() const { return false; }
    virtual bool isIntegerLiteral() const { return false; }
    virtual bool isLogicalExpression() const { return false; }
    virtual bool isLogicalNotExpression() const { return false; }
    virtual bool isMakeArrayReferenceExpression() const { return false; }
    virtual bool isMakePointerExpression() const { return false; }
    virtual bool isNullLiteral() const { return false; }
    virtual bool isPropertyAccessExpression() const { return false; }
    virtual bool isReadModifyWriteExpression() const { return false; }
    virtual bool isTernaryExpression() const { return false; }
    virtual bool isUnsignedIntegerLiteral() const { return false; }
    virtual bool isVariableReference() const { return false; }
    virtual bool isEnumerationMemberLiteral() const { return false; }

private:
    Optional<UniqueRef<UnnamedType>> m_type;
    Optional<TypeAnnotation> m_typeAnnotation;
};

} // namespace AST

}

}

#define SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WHLSL::AST::ToValueTypeName) \
    static bool isType(const WebCore::WHLSL::AST::Expression& expression) { return expression.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
