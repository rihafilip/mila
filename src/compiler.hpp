#ifndef COMPILER_HPP
#define COMPILER_HPP

#include "ast.hpp"
#include "declaration.hpp"
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <memory>
#include <variant>

namespace compiler
{
    using namespace ast;

    class AstVisitor
    {
    private:
        /// LLVM context
        llvm::LLVMContext m_Context;

        /// LLVM builder
        llvm::IRBuilder<> m_Builder;

        /// LLVM module
        llvm::Module m_Module;

        /// Declarations map
        DeclarationMap m_Decls;

        AstVisitor ( const std::string& name )
        : m_Context {}
        , m_Builder { m_Context }
        , m_Module{ name, m_Context }
        , m_Decls {}
        {}

    public:
        static std::unique_ptr<AstVisitor> compile ( const Program& program )
        {
            std::unique_ptr<AstVisitor> visitor ( new AstVisitor{ program.name } );
            visitor->compile_program( program );
            return visitor;
        }

        const llvm::Module& get_module () const
        {
            return m_Module;
        }

    private:
        llvm::ConstantInt* operator() ( const BooleanConstant& );
        llvm::ConstantInt* operator() ( const IntegerConstant& );

        llvm::Value* operator() ( const VariableAccess& );
        llvm::Value* operator() ( const ConstantExpression& );
        llvm::Value* operator() ( const ptr<ArrayAccess>& );
        llvm::Value* operator() ( const ptr<SubprogramCall>& );
        llvm::Value* operator() ( const ptr<UnaryOperator>& );
        llvm::Value* operator() ( const ptr<BinaryOperator>& );

        void operator() ( const SubprogramCall& );
        void operator() ( const Assignment& );
        void operator() ( const ArrayAssignment& );
        void operator() ( const ExitStatement& );
        void operator() ( const BreakStatement& );
        void operator() ( const EmptyStatement& );
        void operator() ( const ptr<Block>& );
        void operator() ( const ptr<If>& );
        void operator() ( const ptr<While>& );
        void operator() ( const ptr<For>& );

        llvm::Type operator() ( const SimpleType& );
        llvm::Type operator() ( const ptr<Array>& );

        void operator() ( const ProcedureDecl& );
        void operator() ( const Procedure& );
        void operator() ( const FunctionDecl& );
        void operator() ( const Function& );
        void operator() ( const NamedConstant& );
        void operator() ( const Variable& );

        void compile_program ( const Program& program );
    };
}

#endif // COMPILER_HPP
