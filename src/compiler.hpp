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
#include <optional>
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

        /// Try to find a value to an identifier in the map
        std::optional<llvm::Value*> find ( const std::string& ident ) const;

        /// Add an identifier and it's value to the map, throwing if it is a redefinition
        void add ( const std::string& ident, llvm::Value* val );
    };

    struct SubprogramDeclarations
    {
        DeclarationMap m_Local;
        DeclarationMap m_Globals;
    };

    using Declarations = std::variant<DeclarationMap, SubprogramDeclarations>;

    llvm::Value* find_or_throw ( const Declarations& decls, const std::string& ident );

    /******************************************************************/

    /**
     * \defgroup AstVisitors AST visitors and code generator
     *  @{
     */

    /// Visitor generating constants, expressions and types
    struct AstVisitor
    {
    public:
        /// LLVM context
        llvm::LLVMContext& m_Context;

        /// LLVM builder
        llvm::IRBuilder<>& m_Builder;

        /// LLVM module
        llvm::Module& m_Module;

        /// Current visible declarations
        Declarations& m_Declarations;

        /// Compile constant, expression ot type
        auto compile ( const auto& variant )
        {
            return std::visit( *this, variant );
        }

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

        // Types
        llvm::Type* operator() ( SimpleType );
        llvm::Type* operator() ( const ptr<Array>& );
    };

    /// Visitor generating statements in function and procedure
    class SubprogramVisitor : public AstVisitor
    {
    public:
        /// Current subprogram name
        const std::string m_Name;

        /// Last block in the subprogram
        llvm::BasicBlock* m_ReturnBlock;

        /// In function address of return value, in procedure nullopt
        std::optional<llvm::Value*> m_ReturnAddress;

        /// In loop the basic block that continues the looping, otherwise nullopt
        std::optional<llvm::BasicBlock*> m_LoopContinuation;

        /// Compile a statement
        void compile_stm ( const Statement& variant )
        {
            return std::visit( *this, variant );
        }

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

        /// Compile the subprogram
        void compile_subprogram ( Many<Variable> params, Many<Variable> variables, Block code );

        /// Helper function to handle compiling of 'while' and 'for' loops
        void compile_loop ( Expression condition, Statement block );
    };

    /// Visitor generating top level declarations and definitions
    struct ProgramVisitor : public AstVisitor
    {
    public:
        /// Compile a global definition
        void compile_glob ( const Global& variant )
        {
            return std::visit( *this, variant );
        }

        // Globals
        void operator() ( const ProcedureDecl& );
        void operator() ( const Procedure& );
        void operator() ( const FunctionDecl& );
        void operator() ( const Function& );
        void operator() ( const NamedConstant& );
        void operator() ( const Variable& );

        /// Add a linkage to external functions (writeln, write, readln)
        void add_external_funcs();

        void compile_program ( const Program& program );
    };
    /// @}

    /**
     * @brief Code compiler
     *
     * It is defined this way so even after the construction of code
     * the context, builder and module are persistent, otherwise parts of the
     * code become undefined and LLVM throws an error
     */
    class Compiler
    {
    private:
        /// LLVM context
        llvm::LLVMContext m_Context;

        /// LLVM builder
        llvm::IRBuilder<> m_Builder;

        /// LLVM module
        llvm::Module m_Module;

        Compiler ( const std::string& name )
        : m_Context {}
        , m_Builder { m_Context }
        , m_Module{ name, m_Context }
        {}

    public:
        /**
         * Compile the given program, returning a pointer to a struct that
         * persist the inner LLVM structure
         */
        static std::unique_ptr<Compiler> compile ( const Program& program );

        /// Get the generated module
        const llvm::Module& get_module () const;
    };
}

#endif // COMPILER_HPP
