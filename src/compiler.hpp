#ifndef COMPILER_HPP
#define COMPILER_HPP

#include "ast.hpp"
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
#include <optional>
#include <stdexcept>
#include <variant>

namespace compiler
{
    using namespace ast;

    class DeclarationMap
    {
    private:
        std::map<std::string, llvm::Value*> m_Data;

    public:
        DeclarationMap()
        : m_Data{}
        {}

        std::optional<llvm::Value*> find ( const std::string& ident )
        {
            auto i = m_Data.find( ident );
            if ( i != m_Data.end() ){
                return i->second;
            }

            return std::nullopt;
        }

        void add ( const std::string& ident, llvm::Value* val )
        {
            auto i = m_Data.find( ident );
            if ( i == m_Data.end() ){
                m_Data.insert({ ident, val });
            }

            throw std::runtime_error( "Redefinition of " + ident );
        }
    };

    struct SubprogramData
    {
        DeclarationMap m_Local;
        const std::string m_Name;

        llvm::Value* m_ReturnAddress;
        llvm::BasicBlock* m_ReturnBlock;
        llvm::BasicBlock* m_LoopContinuation;
        bool m_IsFunction;
        bool m_IsInLoop;
    };

    // enum class StatementContinuation
    // {
    //     CONTINUE, BREAK, EXIT
    // };

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

        /// Subprogram data in subprogram
        std::optional<SubprogramData> m_SubprogramData;

        AstVisitor ( const std::string& name )
        : m_Context {}
        , m_Builder { m_Context }
        , m_Module{ name, m_Context }
        , m_Decls {}
        , m_SubprogramData { std::nullopt }
        {}

        auto compile ( const auto& variant )
        {
            return std::visit( *this, variant );
        }

        // Search in subprogram declarations, then in global declarations
        llvm::Value* find_or_throw ( const std::string& ident )
        {
            if ( m_SubprogramData.has_value() )
            {
                auto val = m_SubprogramData->m_Local.find(ident);
                if ( val.has_value() )
                {
                    return val.value();
                }
            }

            auto val = m_Decls.find( ident );
            if ( val.has_value() )
            {
                return val.value();
            }

            throw std::runtime_error( "Identifier " + ident + " not found" );
        }

        void enter_loop(llvm::BasicBlock* val)
        {
            m_SubprogramData->m_IsInLoop = true;
            m_SubprogramData->m_LoopContinuation = val;
        }

        void exit_loop()
        {
            m_SubprogramData->m_IsInLoop = false;
            m_SubprogramData->m_LoopContinuation = nullptr;
        }

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

        // TODO split the visitors

        // Constants
        llvm::ConstantInt* operator() ( const BooleanConstant& );
        llvm::ConstantInt* operator() ( const IntegerConstant& );

        // Expressions
        llvm::Value* operator() ( const VariableAccess& );
        llvm::Value* operator() ( const ConstantExpression& );
        llvm::Value* operator() ( const ptr<ArrayAccess>& );
        llvm::Value* operator() ( const ptr<SubprogramCall>& );
        llvm::Value* operator() ( const ptr<UnaryOperator>& );
        llvm::Value* operator() ( const ptr<BinaryOperator>& );

        // Statements
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

        // Types
        llvm::Type* operator() ( SimpleType );
        llvm::Type* operator() ( const ptr<Array>& );

        // Globals
        void operator() ( const ProcedureDecl& );
        void operator() ( const Procedure& );
        void operator() ( const FunctionDecl& );
        void operator() ( const Function& );
        void operator() ( const NamedConstant& );
        void operator() ( const Variable& );

private:
        /// Compile the whole program
        void compile_program ( const Program& program );
    };

}

#endif // COMPILER_HPP
