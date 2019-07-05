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

#include "config.h"
#include "WHLSLPreserveVariableLifetimes.h"

#if ENABLE(WEBGPU)

#include "WHLSLAST.h"
#include "WHLSLASTDumper.h"
#include "WHLSLReplaceWith.h"
#include "WHLSLVisitor.h"

namespace WebCore {

namespace WHLSL {

// This pass works by ensuring proper variable lifetimes. In WHLSL, each variable
// has global lifetime. So returning a pointer to a local variable is a totally
// legitimate and well-specified thing to do.
//
// We implement this by:
// - We note every variable whose address we take.
// - Each such variable gets defined as a field in a struct.
// - Each function which is an entry point defines this struct.
// - Each non entry point takes a pointer to this struct as its final parameter.
// - Each call to a non-native function is rewritten to pass a pointer to the
//   struct as the last call argument.
// - Each variable reference to "x", where "x" ends up in the struct, is
//   modified to instead be "struct->x". We store to "struct->x" after declaring
//   "x". If "x" is a function parameter, we store to "struct->x" as the first
//   thing we do in the function body.

class EscapedVariableCollector : public Visitor {
    using Base = Visitor;
public:

    void escapeVariableUse(AST::Expression& expression)
    {
        if (!is<AST::VariableReference>(expression)) {
            // FIXME: Are we missing any interesting productions here?
            // https://bugs.webkit.org/show_bug.cgi?id=198311
            Base::visit(expression);
            return;
        }

        auto* variable = downcast<AST::VariableReference>(expression).variable();
        ASSERT(variable);
        // FIXME: We could skip this if we mark all internal variables with a bit, since we
        // never make any internal variable escape the current scope it is defined in:
        // https://bugs.webkit.org/show_bug.cgi?id=198383
        m_escapedVariables.add(variable, makeString("_", variable->name(), "_", m_count++));
    }

    void visit(AST::MakePointerExpression& makePointerExpression) override
    {
        escapeVariableUse(makePointerExpression.leftValue());
    }

    void visit(AST::MakeArrayReferenceExpression& makeArrayReferenceExpression) override
    {
        escapeVariableUse(makeArrayReferenceExpression.leftValue());
    }

    HashMap<AST::VariableDeclaration*, String> takeEscapedVariables() { return WTFMove(m_escapedVariables); }

private:
    size_t m_count { 1 };
    HashMap<AST::VariableDeclaration*, String> m_escapedVariables;
};

static ALWAYS_INLINE Lexer::Token anonymousToken(Lexer::Token::Type type)
{
    return Lexer::Token { { }, 0, type };
}

class PreserveLifetimes : public Visitor {
    using Base = Visitor;
public:
    PreserveLifetimes(UniqueRef<AST::TypeReference>& structType, const HashMap<AST::VariableDeclaration*, AST::StructureElement*>& variableMapping)
        : m_structType(structType)
        , m_pointerToStructType(makeUniqueRef<AST::PointerType>(anonymousToken(Lexer::Token::Type::Identifier), AST::AddressSpace::Thread, m_structType->clone()))
        , m_variableMapping(variableMapping)
    { }

    UniqueRef<AST::VariableReference> makeStructVariableReference()
    {
        auto structVariableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(*m_structVariable));
        structVariableReference->setType(m_structVariable->type()->clone());
        structVariableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });
        return structVariableReference;
    }

    UniqueRef<AST::AssignmentExpression> assignVariableIntoStruct(AST::VariableDeclaration& variable, AST::StructureElement* element)
    {
        auto lhs = makeUniqueRef<AST::GlobalVariableReference>(variable.origin(), makeStructVariableReference(), element);
        lhs->setType(variable.type()->clone());
        lhs->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        auto rhs = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variable));
        rhs->setType(variable.type()->clone());
        rhs->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        auto assignment = makeUniqueRef<AST::AssignmentExpression>(variable.origin(), WTFMove(lhs), WTFMove(rhs));
        assignment->setType(variable.type()->clone());
        assignment->setTypeAnnotation(AST::RightValue());

        return assignment;
    }

    void visit(AST::FunctionDefinition& functionDefinition) override
    {
        bool isEntryPoint = !!functionDefinition.entryPointType();
        if (isEntryPoint) {
            auto structVariableDeclaration = makeUniqueRef<AST::VariableDeclaration>(functionDefinition.origin(), AST::Qualifiers(),
                m_structType->clone(), String(), nullptr, nullptr);

            auto structVariableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(structVariableDeclaration));
            structVariableReference->setType(m_structType->clone());
            structVariableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

            AST::VariableDeclarations structVariableDeclarations;
            structVariableDeclarations.append(WTFMove(structVariableDeclaration));
            auto structDeclarationStatement = makeUniqueRef<AST::VariableDeclarationsStatement>(functionDefinition.origin(), WTFMove(structVariableDeclarations));

            auto makePointerExpression = std::make_unique<AST::MakePointerExpression>(functionDefinition.origin(), WTFMove(structVariableReference));
            makePointerExpression->setType(m_pointerToStructType->clone());
            makePointerExpression->setTypeAnnotation(AST::RightValue());

            auto pointerDeclaration = makeUniqueRef<AST::VariableDeclaration>(functionDefinition.origin(), AST::Qualifiers(),
                m_pointerToStructType->clone(), "wrapper"_s, nullptr, WTFMove(makePointerExpression));
            m_structVariable = &pointerDeclaration;

            AST::VariableDeclarations pointerVariableDeclarations;
            pointerVariableDeclarations.append(WTFMove(pointerDeclaration));
            auto pointerDeclarationStatement = makeUniqueRef<AST::VariableDeclarationsStatement>(functionDefinition.origin(), WTFMove(pointerVariableDeclarations));

            functionDefinition.block().statements().insert(0, WTFMove(structDeclarationStatement));
            functionDefinition.block().statements().insert(1, WTFMove(pointerDeclarationStatement));
        } else {
            auto pointerDeclaration = makeUniqueRef<AST::VariableDeclaration>(functionDefinition.origin(), AST::Qualifiers(),
                m_pointerToStructType->clone(), "wrapper"_s, nullptr, nullptr);
            m_structVariable = &pointerDeclaration;
            functionDefinition.parameters().append(WTFMove(pointerDeclaration));
        }

        Base::visit(functionDefinition);

        for (auto& parameter : functionDefinition.parameters()) {
            auto iter = m_variableMapping.find(&parameter);
            if (iter == m_variableMapping.end())
                continue;

            functionDefinition.block().statements().insert(isEntryPoint ? 2 : 0,
                makeUniqueRef<AST::EffectfulExpressionStatement>(assignVariableIntoStruct(parameter, iter->value)));
        }

        // Inner functions are not allowed in WHLSL. So this is fine.
        m_structVariable = nullptr;
    }

    void visit(AST::CallExpression& callExpression) override
    {
        RELEASE_ASSERT(m_structVariable);

        Base::visit(callExpression);

        // This works because it's illegal to call an entrypoint. Therefore, we can only
        // call functions where we've already appended this struct as its final parameter.
        if (!callExpression.function()->isNativeFunctionDeclaration())
            callExpression.arguments().append(makeStructVariableReference());
    }

    void visit(AST::VariableReference& variableReference) override
    {
        RELEASE_ASSERT(m_structVariable);

        auto iter = m_variableMapping.find(variableReference.variable());
        if (iter == m_variableMapping.end())
            return;

        auto type = variableReference.variable()->type()->clone();
        AST::TypeAnnotation typeAnnotation = variableReference.typeAnnotation();
        auto* internalField = AST::replaceWith<AST::GlobalVariableReference>(variableReference, variableReference.origin(), makeStructVariableReference(), iter->value);
        internalField->setType(WTFMove(type));
        internalField->setTypeAnnotation(WTFMove(typeAnnotation));
    }

    void visit(AST::VariableDeclarationsStatement& variableDeclarationsStatement) override
    {
        RELEASE_ASSERT(m_structVariable);

        Base::visit(variableDeclarationsStatement);

        AST::Statements statements;
        for (UniqueRef<AST::VariableDeclaration>& variableDeclaration : variableDeclarationsStatement.variableDeclarations()) {
            AST::VariableDeclaration& variable = variableDeclaration.get();

            {
                AST::VariableDeclarations declarations;
                declarations.append(WTFMove(variableDeclaration));
                statements.append(makeUniqueRef<AST::VariableDeclarationsStatement>(variable.origin(), WTFMove(declarations)));
            }

            auto iter = m_variableMapping.find(&variable);
            if (iter != m_variableMapping.end())
                statements.append(makeUniqueRef<AST::EffectfulExpressionStatement>(assignVariableIntoStruct(variable, iter->value)));
        }

        auto origin = Lexer::Token(variableDeclarationsStatement.origin());
        AST::replaceWith<AST::StatementList>(variableDeclarationsStatement, WTFMove(origin), WTFMove(statements));
    }

private:
    AST::VariableDeclaration* m_structVariable { nullptr };

    UniqueRef<AST::TypeReference>& m_structType;
    UniqueRef<AST::PointerType> m_pointerToStructType;
    // If this mapping contains the variable, it means that the variable's canonical location
    // is in the struct we use to preserve variable lifetimes.
    const HashMap<AST::VariableDeclaration*, AST::StructureElement*>& m_variableMapping;
};

void preserveVariableLifetimes(Program& program)
{
    HashMap<AST::VariableDeclaration*, String> escapedVariables;
    {
        EscapedVariableCollector collector;
        collector.Visitor::visit(program);
        escapedVariables = collector.takeEscapedVariables();
    }

    AST::StructureElements elements;
    for (auto& pair : escapedVariables) {
        auto* variable = pair.key;
        String name = pair.value;
        elements.append(AST::StructureElement { Lexer::Token(variable->origin()), { }, variable->type()->clone(), WTFMove(name), nullptr });
    }

    // Name of this doesn't matter, since we don't use struct names when
    // generating Metal type names. We just pick something here to make it
    // easy to read in AST dumps.
    auto wrapperStructDefinition = makeUniqueRef<AST::StructureDefinition>(anonymousToken(Lexer::Token::Type::Struct), "__WrapperStruct__"_s, WTFMove(elements));

    HashMap<AST::VariableDeclaration*, AST::StructureElement*> variableMapping;
    unsigned index = 0;
    for (auto& pair : escapedVariables)
        variableMapping.add(pair.key, &wrapperStructDefinition->structureElements()[index++]);

    {
        auto wrapperStructType = AST::TypeReference::wrap(anonymousToken(Lexer::Token::Type::Identifier), wrapperStructDefinition);
        PreserveLifetimes preserveLifetimes(wrapperStructType, variableMapping);
        preserveLifetimes.Visitor::visit(program);
    }

    program.structureDefinitions().append(WTFMove(wrapperStructDefinition));
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
