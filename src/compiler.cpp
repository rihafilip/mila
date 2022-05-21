#include "compiler.hpp"
#include "ast.hpp"
#include "variant_helpers.hpp"
#include <bits/ranges_algo.h>
#include <llvm/IR/BasicBlock.h>
#include <math.h>
#include <stdexcept>

namespace compiler
{

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
        auto val = find_or_throw(va.identifier);
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

    void AstVisitor::operator() ( const SubprogramCall& sub )
    {
        std::vector<llvm::Value*> args;

        for ( const auto& a : sub.arguments ){
            args.push_back( compile( a ) );
        }

        m_Builder.CreateCall(m_Module.getFunction(sub.functionName), args, sub.functionName);
    }

    void AstVisitor::operator() ( const Assignment& assign )
    {
        auto val = compile( assign.value );

        // 'function_name := val' => assign return value
        if ( m_SubprogramData.has_value()
            && m_SubprogramData->m_IsFunction
            && m_SubprogramData->m_Name == assign.variable )
        {
            m_Builder.CreateStore(val, m_SubprogramData->m_ReturnAddress);
        }
        else
        {
            auto ptr = find_or_throw( assign.variable );
            m_Builder.CreateStore(val, ptr);
        }
    }

    void AstVisitor::operator() ( const ArrayAssignment& )
    {
        throw std::runtime_error( "TODO" );
    }

    void AstVisitor::operator() ( const ExitStatement& )
    {
        if ( ! m_SubprogramData.has_value() || m_SubprogramData->m_IsFunction )
        {
            throw std::runtime_error( "Exit used outside of procedure" );
        }

        m_Builder.CreateBr( m_SubprogramData->m_ReturnBlock );

        auto bb = llvm::BasicBlock::Create(m_Context, "afterExit", m_Builder.GetInsertBlock()->getParent());
        m_Builder.SetInsertPoint(bb);
    }

    void AstVisitor::operator() ( const BreakStatement& )
    {
        if ( ! m_SubprogramData.has_value() || ! m_SubprogramData->m_IsInLoop )
        {
            throw std::runtime_error( "Break used outside of loop" );
        }

        m_Builder.CreateBr( m_SubprogramData->m_LoopContinuation );

        auto bb = llvm::BasicBlock::Create(m_Context, "afterBreak", m_Builder.GetInsertBlock()->getParent());
        m_Builder.SetInsertPoint(bb);
    }

    void AstVisitor::operator() ( const EmptyStatement& )
    {
        return;
    }

    void AstVisitor::operator() ( const ptr<Block>& bl )
    {
        for ( const auto& st : bl->statements )
        {
            compile( st );
        }
    }

    void AstVisitor::operator() ( const ptr<If>& if_ )
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
        compile( if_->trueCode );
        m_Builder.CreateBr(continueBB);

        // compile false branch
        m_Builder.SetInsertPoint( falseBB );
        if ( if_->elseCode.has_value() )
        {
            compile( if_->elseCode.value());
        }
        m_Builder.CreateBr(continueBB);

        // switch to continuation
        m_Builder.SetInsertPoint( continueBB );
    }

    void AstVisitor::operator() ( const ptr<While>& wh )
    {
        auto parent = m_Builder.GetInsertBlock()->getParent();

        auto condBB = llvm::BasicBlock::Create(m_Context, "whileCond", parent);
        auto bodyBB = llvm::BasicBlock::Create(m_Context, "whileBody", parent);
        auto continueBB = llvm::BasicBlock::Create(m_Context, "afterWhile", parent);

        m_Builder.CreateBr(condBB);

        // compile condition
        m_Builder.SetInsertPoint( condBB );
        auto cond = compile( wh->condition );
        m_Builder.CreateCondBr( cond, bodyBB, continueBB );

        // compile body
        enter_loop(continueBB);
        m_Builder.SetInsertPoint( bodyBB );
        compile( wh->code );
        m_Builder.CreateBr( condBB );

        // switch to continuation
        exit_loop();
        m_Builder.SetInsertPoint( continueBB );
    }

    void AstVisitor::operator() ( const ptr<For>& fo )
    {
        // for iterator ...
        // initialization
        auto iterator = find_or_throw(fo->loopVariable);
        auto init = compile(fo->initialization);
        m_Builder.CreateStore(init, iterator);

        // Transform a for loop to while loop
        // to ..   while ( x <= target )
        // downto x >=
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

        // call the while compilation
        (*this)( make_ptr<While>(cond, fo->code) );
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


    void AstVisitor::operator() ( const ProcedureDecl& )
    {
       throw std::runtime_error( "TODO" );
    }

    void AstVisitor::operator() ( const Procedure& )
    {

    }

    void AstVisitor::operator() ( const FunctionDecl& )
    {
       throw std::runtime_error( "TODO" );
    }

    void AstVisitor::operator() ( const Function& )
    {

    }

    void AstVisitor::operator() ( const NamedConstant& )
    {

    }

    void AstVisitor::operator() ( const Variable& )
    {

    }


/******************************************************************/

    void AstVisitor::compile_program ( const Program& program )
    {
        // create writeln function
        {
        std::vector<llvm::Type*> Ints(1, m_Builder.getInt32Ty());
        llvm::FunctionType * FT = llvm::FunctionType::get(m_Builder.getInt32Ty(), Ints, false);
        llvm::Function * F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "writeln", m_Module);
        for (auto & Arg : F->args())
            Arg.setName("x");
        }



        // auto bb = llvm::BasicBlock::Create( m_Context, "entry", main );
        // auto bb2 = llvm::BasicBlock::Create( m_Context, "bb2", main );
        // auto bb3 = llvm::BasicBlock::Create( m_Context, "bb3", main );
        // auto bb4 = llvm::BasicBlock::Create( m_Context, "bb4", main );
        // m_Builder.SetInsertPoint( bb );

        // auto ret = m_Builder.CreateAdd(m_Builder.getInt32(50), m_Builder.getInt32(32));
        // m_Builder.CreateCall( m_Module.getFunction( "writeln" ), ret );
        // m_Builder.CreateBr( bb2 );

        // m_Builder.SetInsertPoint( bb2 );
        // auto val1 = m_Builder.CreateAdd( m_Builder.getInt32(50), m_Builder.getInt32(32) );
        // m_Builder.CreateCall( m_Module.getFunction( "writeln" ), val1 );

        // m_Builder.SetInsertPoint( bb3 );
        // auto val2 = m_Builder.CreateAdd( m_Builder.getInt32(50), m_Builder.getInt32(32) );
        // m_Builder.CreateCall( m_Module.getFunction( "writeln" ), val2 );
        // // m_Builder.CreateBr( bb4 );
        // m_Builder.CreateRet( ret );


        // m_Builder.SetInsertPoint(  bb4 );
        // m_Builder.CreateRet( ret );

        for ( const auto& g : program.globals )
        {
            compile ( g );
        }

        auto main_type = llvm::FunctionType::get( m_Builder.getInt32Ty(), false );
        auto main = llvm::Function::Create( main_type, llvm::Function::ExternalLinkage, "main", m_Module );
        auto mainBB = llvm::BasicBlock::Create( m_Context, "entry", main );
        m_Builder.SetInsertPoint (mainBB);
        (*this)( make_ptr<ast::Block>( program.code ));

        // ast::Function mainFun { "main", {}, SimpleType::INTEGER, {}, program.code };
        // (*this)( mainFun );
    }
}
