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
#include <set>
#include <variant>

namespace compiler
{
    using namespace ast;

    namespace external
    {
        /// Functions that require pointer as an argument
        const std::set<std::string> POINTER_FUNS = {
            "readln", "dec"
        };

        const std::vector<std::variant<ast::ProcedureDecl, ast::FunctionDecl>> EXTERNAL_FUNCS=
        {
            FunctionDecl { "writeln", { {"x", ast::SimpleType::INTEGER}}, ast::SimpleType::INTEGER },
            FunctionDecl { "write",   { {"x", ast::SimpleType::INTEGER}}, ast::SimpleType::INTEGER },
            FunctionDecl { "readln",  { {"x", ast::SimpleType::INTEGER}}, ast::SimpleType::INTEGER },
            ProcedureDecl{ "dec", { {"x", ast::SimpleType::INTEGER}}}
        };
    }

    /// Simple wrapper around map for holding variables
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

    /******************************************************************/

    /// Visitor generating constants, constant expressions and types
    struct ConstantVisitor
    {
        /// LLVM context
        llvm::LLVMContext& m_Context;

        /// LLVM builder
        llvm::IRBuilder<>& m_Builder;

        /// LLVM module
        llvm::Module& m_Module;

        /// Compile constant
        llvm::ConstantInt* compile_const ( const Constant& variant )
        {
            return std::visit( *this, variant );
        }

        /// Compile constant expression
        llvm::Constant* compile_cexpr ( const Expression& expr )
        {
            return std::visit( *this, expr );
        }

        /// Compile type
        llvm::Type* compile_t ( const Type& variant )
        {
            return std::visit( *this, variant );
        }

        // TODO groups like this in doc
        /// @name Constant generators
        /// @{
        llvm::ConstantInt* operator() ( const BooleanConstant& );
        llvm::ConstantInt* operator() ( const IntegerConstant& );
        /// @}

        // Constant expressions
        llvm::Constant* operator() ( const VariableAccess& );
        llvm::Constant* operator() ( const ConstantExpression& );
        llvm::Constant* operator() ( const ptr<ArrayAccess>& );
        llvm::Constant* operator() ( const ptr<SubprogramCall>& );
        llvm::Constant* operator() ( const ptr<UnaryOperator>& );
        llvm::Constant* operator() ( const ptr<BinaryOperator>& );

        // Types
        llvm::Type* operator() ( SimpleType );
        llvm::Type* operator() ( const ptr<Array>& );
    };

    /**
     * \defgroup AstVisitors AST visitors and code generator
     *  @{
     */

    /// Visitor generating expressions and holding local variables
    struct ExprVisitor : public ConstantVisitor
    {
    public:
        /// Local declarations
        const DeclarationMap m_Locals;

        /// Compile expression
        llvm::Value* compile_expr ( const Expression& variant )
        {
            return std::visit( *this, variant );
        }

        llvm::Value* local_or_global ( const std::string& name );

        llvm::Value* variable_address ( const Expression& expr );

        // Expressions
        llvm::Value* operator() ( const VariableAccess& );
        llvm::Value* operator() ( const ConstantExpression& );
        llvm::Value* operator() ( const ptr<ArrayAccess>& );
        llvm::Value* operator() ( const ptr<SubprogramCall>& );
        llvm::Value* operator() ( const ptr<UnaryOperator>& );
        llvm::Value* operator() ( const ptr<BinaryOperator>& );
    };

    /// Visitor generating statements in function and procedure
    class SubprogramVisitor : public ExprVisitor
    {
    public:
        /// Last block in the subprogram
        llvm::BasicBlock* m_ReturnBlock;

        /// In loop the basic block that continues the looping, otherwise nullopt
        std::optional<llvm::BasicBlock*> m_LoopContinuation;

        /// Compile a statement
        void compile_stm ( const Statement& variant )
        {
            return std::visit( *this, variant );
        }

        llvm::BasicBlock* get_basic_block ( const std::string& name );

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

        /// Compile the subprogram code
        void compile_block ( const Block& code );

        /// Helper function to handle compiling of 'while' and 'for' loops
        void compile_loop ( Expression condition, Statement block, Statement increment );
    };

    /// Visitor generating top level declarations and definitions
    struct ProgramVisitor : public ExprVisitor
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

        /// Compile the whole AST
        void compile_program ( const Program& program );

        /// Compile a subprogram declaration
        llvm::Function* compile_subprogram_decl (
            const Identifier& name,
            const Many<Variable>& parameters,
            const std::optional<Type>& retType,
            bool ptrParams = false
        );

        /// Compile a subprogram
        void compile_subprogram (
            const Identifier& name,
            const Many<Variable>& parameters,
            const Many<Variable>& variables,
            const std::optional<Type>& retType,
            const Block& code
        );
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
