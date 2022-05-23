#include "compiler.hpp"
#include "ast.hpp"
#include "variant_helpers.hpp"
#include <bits/ranges_algo.h>
#include <llvm/IR/BasicBlock.h>
#include <math.h>
#include <optional>
#include <stdexcept>

namespace compiler
{
    std::optional<llvm::Value*> DeclarationMap::find ( const std::string& ident ) const
    {
        auto i = m_Data.find( ident );
        if ( i != m_Data.end() ){
            return i->second;
        }

        return std::nullopt;
    }

    void DeclarationMap::add ( const std::string& ident, llvm::Value* val )
    {
        auto i = m_Data.find( ident );
        if ( i == m_Data.end() ){
            m_Data.insert({ ident, val });
        }

        throw std::runtime_error( "Redefinition of " + ident );
    }
    llvm::Value* find_or_throw ( const Declarations& decls, const std::string& ident )
    {
        return wrap( decls ).visit(
            [&ident] ( const DeclarationMap& map ){
                auto val = map.find(ident);
                if ( val.has_value() )
                {
                    return val.value();
                }

                throw std::runtime_error( "Identifier " + ident + " not found" );
            },
            [&ident] ( const SubprogramDeclarations& subMaps ){
                auto val = subMaps.m_Local.find(ident);
                if ( val.has_value() )
                {
                    return val.value();
                }

                val = subMaps.m_Globals.find( ident );
                if ( val.has_value() )
                {
                    return val.value();
                }

                 throw std::runtime_error( "Identifier " + ident + " not found" );
            }
        );
    }

    /******************************************************************/

    llvm::ConstantInt* AstVisitor::operator() ( const BooleanConstant& b )
    {
        if ( b.value ){
            return m_Builder.getTrue();
        }
        else{
            return m_Builder.getFalse();
        }
    }

    llvm::ConstantInt* AstVisitor::operator() ( const IntegerConstant& c )
    {
        return m_Builder.getInt32( c.value );
    }

/******************************************************************/

    llvm::Value* AstVisitor::operator() ( const VariableAccess& va )
    {
        auto val = find_or_throw(m_Declarations, va.identifier);
        return m_Builder.CreateLoad( m_Builder.getInt32Ty(), val, va.identifier );
    }

    llvm::Value* AstVisitor::operator() ( const ConstantExpression& c )
    {
        return compile ( c.value );
    }

    llvm::Value* AstVisitor::operator() ( const ptr<ArrayAccess>& )
    {
        throw std::runtime_error( "TODO" );
    }

    llvm::Value* AstVisitor::operator() ( const ptr<SubprogramCall>& sub )
    {
        std::vector<llvm::Value*> args {};
        args.reserve(sub->arguments.size());
        for ( const auto& p : sub->arguments ){
            args.push_back( compile( p ) );
        }

        return m_Builder.CreateCall(m_Module.getFunction(sub->functionName), args, sub->functionName);
    }

    llvm::Value* AstVisitor::operator() ( const ptr<UnaryOperator>& un )
    {
        auto val = compile( un->expression );
        switch ( un->op ){
        case UnaryOperator::OPERATOR::PLUS:
            return val;

        // '0 - val'
        case UnaryOperator::OPERATOR::MINUS:
            return m_Builder.CreateSub( m_Builder.getInt32(0), val );

        case UnaryOperator::OPERATOR::NOT:
            return m_Builder.CreateNot(val);
        }

    }

    llvm::Value* AstVisitor::operator() ( const ptr<BinaryOperator>& bin )
    {
        auto lhs = compile( bin->left );
        auto rhs = compile( bin->right );

        switch ( bin->op ){
            case BinaryOperator::OPERATOR::EQ:
                return m_Builder.CreateICmpEQ(lhs, rhs);

            case BinaryOperator::OPERATOR::NOT_EQ:
                return m_Builder.CreateICmpNE(lhs, rhs);

            case BinaryOperator::OPERATOR::LESS_EQ:
                return m_Builder.CreateICmpSLE(lhs, rhs);

            case BinaryOperator::OPERATOR::LESS:
                return m_Builder.CreateICmpSLT(lhs, rhs);

            case BinaryOperator::OPERATOR::MORE_EQ:
                return m_Builder.CreateICmpSGE(lhs, rhs);

            case BinaryOperator::OPERATOR::MORE:
                return m_Builder.CreateICmpSGT(lhs, rhs);

            case BinaryOperator::OPERATOR::PLUS:
                return m_Builder.CreateAdd(lhs, rhs);

            case BinaryOperator::OPERATOR::MINUS:
                return m_Builder.CreateSub(lhs, rhs);

            case BinaryOperator::OPERATOR::TIMES:
                return m_Builder.CreateMul(lhs, rhs);

            case BinaryOperator::OPERATOR::DIVISION:
            case BinaryOperator::OPERATOR::INTEGER_DIVISION:
                return m_Builder.CreateSDiv(lhs, rhs);

            case BinaryOperator::OPERATOR::MODULO:
                return m_Builder.CreateSRem(lhs, rhs);

            case BinaryOperator::OPERATOR::AND:
                return m_Builder.CreateAnd(lhs, rhs);

            case BinaryOperator::OPERATOR::OR:
                return m_Builder.CreateOr(lhs, rhs);

            case BinaryOperator::OPERATOR::XOR:
                return m_Builder.CreateXor(lhs, rhs);
        }
    }

/******************************************************************/

    llvm::Type* AstVisitor::operator() ( SimpleType t )
    {
        switch (t) {
        case SimpleType::BOOLEAN:
            return m_Builder.getInt32Ty();

        case ast::SimpleType::INTEGER:
            return m_Builder.getInt1Ty();
        }
    }

    llvm::Type* AstVisitor::operator() ( const ptr<Array>& )
    {
        throw std::runtime_error( "TODO" );
    }

/******************************************************************/

    void SubprogramVisitor::operator() ( const SubprogramCall& sub )
    {
        std::vector<llvm::Value*> args;

        for ( const auto& a : sub.arguments ){
            args.push_back( compile( a ) );
        }

        m_Builder.CreateCall(m_Module.getFunction(sub.functionName), args, sub.functionName);
    }

    void SubprogramVisitor::operator() ( const Assignment& assign )
    {
        auto val = compile( assign.value );

        // 'function_name := val' => assign return value
        if ( m_ReturnAddress.has_value() && m_Name == assign.variable )
        {
            m_Builder.CreateStore(val, m_ReturnAddress.value());
        }
        else
        {
            auto ptr = find_or_throw( m_Declarations, assign.variable );
            m_Builder.CreateStore(val, ptr);
        }
    }

    void SubprogramVisitor::operator() ( const ArrayAssignment& )
    {
        throw std::runtime_error( "TODO" );
    }

    void SubprogramVisitor::operator() ( const ExitStatement& )
    {
        if ( m_ReturnAddress.has_value() )
        {
            throw std::runtime_error( "Exit used outside of procedure" );
        }

        m_Builder.CreateBr( m_ReturnBlock );

        auto bb = llvm::BasicBlock::Create(m_Context, "afterExit", m_Builder.GetInsertBlock()->getParent());
        m_Builder.SetInsertPoint(bb);
    }

    void SubprogramVisitor::operator() ( const BreakStatement& )
    {
        if ( ! m_LoopContinuation.has_value() )
        {
            throw std::runtime_error( "Break used outside of loop" );
        }

        m_Builder.CreateBr( m_LoopContinuation.value() );

        auto bb = llvm::BasicBlock::Create(m_Context, "afterBreak", m_Builder.GetInsertBlock()->getParent());
        m_Builder.SetInsertPoint(bb);
    }

    void SubprogramVisitor::operator() ( const EmptyStatement& )
    {
        return;
    }

    void SubprogramVisitor::operator() ( const ptr<Block>& bl )
    {
        for ( const auto& st : bl->statements )
        {
            std::visit( *this, st );
        }
    }

    void SubprogramVisitor::operator() ( const ptr<If>& if_ )
    {
        auto parent = m_Builder.GetInsertBlock()->getParent();

        auto trueBB = llvm::BasicBlock::Create(m_Context, "trueBranch", parent);
        auto falseBB = llvm::BasicBlock::Create(m_Context, "falseBranch", parent);
        auto continueBB = llvm::BasicBlock::Create( m_Context, "afterIf", parent );

        // conditional jump
        auto cond = compile(if_->condition);
        m_Builder.CreateCondBr( cond, trueBB, falseBB );

        // compile true branch
        m_Builder.SetInsertPoint( trueBB );
        compile_stm( if_->trueCode );
        m_Builder.CreateBr(continueBB);

        // compile false branch
        m_Builder.SetInsertPoint( falseBB );
        if ( if_->elseCode.has_value() )
        {
            compile_stm( if_->elseCode.value());
        }
        m_Builder.CreateBr(continueBB);

        // switch to continuation
        m_Builder.SetInsertPoint( continueBB );
    }

    void SubprogramVisitor::operator() ( const ptr<While>& wh )
    {
        compile_loop(wh->condition, wh->code);
    }

    void SubprogramVisitor::operator() ( const ptr<For>& fo )
    {
        // initialization
        // `for iterator ...`
        auto iterator = find_or_throw(m_Declarations, fo->loopVariable);
        auto init = compile( fo->initialization );
        m_Builder.CreateStore( init, iterator );

        // Transform a for loop to while loop
        // to     => while ( x <= target )
        // downto =>         x >=
        ast::Expression loopVar = ast::VariableAccess { fo->loopVariable };
        ast::Expression cond;
        switch (fo->direction) {
        case For::DIRECTION::TO:
            cond = make_ptr<BinaryOperator>(
                BinaryOperator::OPERATOR::LESS_EQ, loopVar, fo->target );
            break;

        case For::DIRECTION::DOWNTO:
            cond = make_ptr<BinaryOperator>(
                BinaryOperator::OPERATOR::LESS_EQ, loopVar, fo->target );
            break;
        }

        compile_loop(cond, fo->code);
    }

    void SubprogramVisitor::compile_block ( const Block& code )
    {
        throw std::runtime_error( "TODO" );
    }

    void SubprogramVisitor::compile_loop ( Expression condition, Statement block )
    {
        auto parent = m_Builder.GetInsertBlock()->getParent();

        auto condBB = llvm::BasicBlock::Create(m_Context, "loopCond", parent);
        auto bodyBB = llvm::BasicBlock::Create(m_Context, "loopBody", parent);
        auto continueBB = llvm::BasicBlock::Create(m_Context, "afterLoop", parent);

        m_Builder.CreateBr(condBB);

        // compile condition
        m_Builder.SetInsertPoint( condBB );
        auto cond = compile( condition );
        m_Builder.CreateCondBr( cond, bodyBB, continueBB );

        // this is done so nested loop don't break
        auto prevLoop = m_LoopContinuation;
        m_LoopContinuation = continueBB;

        // compile body
        m_Builder.SetInsertPoint( bodyBB );
        compile_stm( block );
        m_Builder.CreateBr( condBB );

        // switch to continuation
        m_Builder.SetInsertPoint( continueBB );
        m_LoopContinuation = prevLoop;
    }

/******************************************************************/


    void ProgramVisitor::operator() ( const ProcedureDecl& )
    {
       throw std::runtime_error( "TODO" );
    }

    void ProgramVisitor::operator() ( const Procedure& proc )
    {
        compile_subprogram(
            proc.name,
            proc.parameters,
            proc.variables,
            std::nullopt,
            proc.code
        );
    }

    void ProgramVisitor::operator() ( const FunctionDecl& )
    {
       throw std::runtime_error( "TODO" );
    }

    void ProgramVisitor::operator() ( const Function& fun )
    {
        compile_subprogram(
            fun.name,
            fun.parameters,
            fun.variables,
            fun.returnType,
            fun.code
        );
    }

    void ProgramVisitor::operator() ( const NamedConstant& c )
    {
        auto val = compile( c.value );
        new llvm::GlobalVariable(
            m_Module,
            val->getType(),
            true, // Is a constant
            llvm::GlobalVariable::ExternalLinkage,
            0, // TODO actual value
            c.name
        );
    }

    void ProgramVisitor::operator() ( const Variable& var )
    {
        auto type = compile( var.type );
        new llvm::GlobalVariable(
            m_Module,
            type,
            false, // Is a constant
            llvm::GlobalVariable::ExternalLinkage,
            0, // TODO actual value
            var.name
        );
    }


/******************************************************************/

    void ProgramVisitor::add_external_funcs()
    {
        std::vector<llvm::Type*> int_arg (1, m_Builder.getInt32Ty());
        llvm::FunctionType * fun_type = llvm::FunctionType::get(m_Builder.getInt32Ty(), int_arg , false);

        // create writeln function
        {
            llvm::Function * fun = llvm::Function::Create(fun_type, llvm::Function::ExternalLinkage, "writeln", m_Module);
            for (auto & a : fun->args())
            {
                a.setName("x");
            }
        }

        // create write function
        {
            llvm::Function * fun = llvm::Function::Create(fun_type, llvm::Function::ExternalLinkage, "write", m_Module);
            for (auto & a : fun->args())
            {
                a.setName("x");
            }
        }

        // create readln
        {
            std::vector<llvm::Type*> read_arg (1, m_Builder.getInt32Ty()->getPointerTo());
            llvm::FunctionType * read_type = llvm::FunctionType::get(m_Builder.getInt32Ty(), read_arg, false);
            llvm::Function * fun = llvm::Function::Create( read_type, llvm::Function::ExternalLinkage, "readln", m_Module );
            for (auto & a : fun->args())
            {
                a.setName("x");
            }
        }
        }

    void ProgramVisitor::compile_program ( const Program& program )
    {
        for ( const auto& g : program.globals )
        {
            compile_glob ( g );
        }

        auto * mainFun = llvm::Function::Create(
            llvm::FunctionType::get(
                m_Builder.getInt32Ty(),
                false
            ),
            llvm::Function::ExternalLinkage,
            "main",
            m_Module
        );

        auto BB = llvm::BasicBlock::Create(m_Context, "entry", mainFun);
        m_Builder.SetInsertPoint(BB);
        compile_subprogram("main", {}, {}, SimpleType::INTEGER, program.code);
    }

        void ProgramVisitor::compile_subprogram (
            const Identifier& name,
            const Many<Variable>& parameters,
            const Many<Variable>& variables,
            const std::optional<Type>& retType,
            const Block& code
        )
        {
            // Return type
            llvm::Type * llvmReturnType;
            if ( retType.has_value() ) {
                llvmReturnType = compile( retType.value() );
            }
            else {
                llvmReturnType = m_Builder.getVoidTy();
            }

            // Args
            std::vector<llvm::Type*> llvmParams;
            llvmParams.reserve( parameters.size() );
            for ( const auto& p : parameters )
            {
                llvmParams.push_back( compile( p.type ) );
            }

            // The actual function
            auto llvmFun = llvm::Function::Create(
                llvm::FunctionType::get( llvmReturnType, llvmParams, false),
                llvm::Function::ExternalLinkage,
                name,
                m_Module
            );

            // Name the arguments
            size_t i = 0;
            for ( auto& a : llvmFun->args() )
            {
                a.setName( parameters[i].name );
                ++i;
            }

            // Entry and return basic blocks
            auto entryBB = llvm::BasicBlock::Create(m_Context, "entry", llvmFun);
            auto returnBB = llvm::BasicBlock::Create(m_Context, "return", llvmFun);

            m_Builder.SetInsertPoint( entryBB );

            // Local variables
            DeclarationMap locals {};
            for ( const auto& v : variables )
            {
                auto vAddr = m_Builder.CreateAlloca( compile(v.type) );
                locals.add(v.name, vAddr);
            }

            // Parameters
            // TODO

            // All declarations
            Declarations decls = addLocal(m_Declarations, locals);

            // Return address
            std::optional<llvm::Value*> returnAddress;
            if ( retType.has_value() ) {
                returnAddress = m_Builder.CreateAlloca( llvmReturnType );
            }
            else {
                returnAddress = std::nullopt;
            }

            // Code
            SubprogramVisitor visitor {
                { m_Context, m_Builder, m_Module, decls },
                name,
                returnBB,
                returnAddress,
                std::nullopt // Not starting in a loop
            };
            visitor.compile_block( code );

            // Return
            m_Builder.SetInsertPoint( returnBB );
            if ( returnAddress.has_value() ){
                auto retVal = m_Builder.CreateLoad( llvmReturnType, returnAddress.value() );
                m_Builder.CreateRet( retVal );
            }
            else {
                m_Builder.CreateRetVoid();
            }
    }

/******************************************************************/

    std::unique_ptr<Compiler> Compiler::compile ( const Program& program )
    {
        std::unique_ptr<Compiler> compiler ( new Compiler{ program.name } );

        Declarations decls = DeclarationMap{};
        ProgramVisitor pr { compiler->m_Context, compiler->m_Builder, compiler->m_Module, decls };
        pr.add_external_funcs();
        pr.compile_program( program );

        return compiler;
    }

    const llvm::Module& Compiler::get_module () const
    {
        return m_Module;
    }
}
