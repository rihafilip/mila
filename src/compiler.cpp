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
    llvm::Constant* ConstantVisitor::operator() ( const VariableAccess& name )
    {
        auto glob = m_Module.getNamedGlobal(name.identifier);
        if ( glob == nullptr ){
            throw std::runtime_error( "Usage of undeclared " + name.identifier );
        }

        if ( ! glob->isConstant() ){
            throw std::runtime_error( "Usage of variable "
                + name.identifier
                + " as a constant"
            );
        }

        return glob;
    }

    llvm::Constant* ConstantVisitor::operator() ( const ConstantExpression& c)
    {
        return compile_const( c.value );
    }

    llvm::Constant* ConstantVisitor::operator() ( const ptr<ArrayAccess>& )
    {
        throw std::runtime_error( "Usage of array access as constant value." );
    }

    llvm::Constant* ConstantVisitor::operator() ( const ptr<SubprogramCall>& )
    {
        throw std::runtime_error( "Usage of subprogram call as constant value." );
    }

    llvm::Constant* ConstantVisitor::operator() ( const ptr<UnaryOperator>& un )
    {
        auto val = compile_cexpr( un->expression );
        switch (un->op) {
        case UnaryOperator::OPERATOR::PLUS:
            return val;

        case UnaryOperator::OPERATOR::MINUS:
            return llvm::ConstantExpr::getNeg(val);

        case UnaryOperator::OPERATOR::NOT:
            return llvm::ConstantExpr::getNot(val);
        }
    }

    llvm::Constant* ConstantVisitor::operator() ( const ptr<BinaryOperator>& bin )
    {
        auto lhs = compile_cexpr( bin->left );
        auto rhs = compile_cexpr( bin->right );

        switch ( bin->op ){
            case BinaryOperator::OPERATOR::EQ:
                return llvm::ConstantExpr::getCompare( llvm::CmpInst::ICMP_EQ, lhs, rhs);

            case BinaryOperator::OPERATOR::NOT_EQ:
                return llvm::ConstantExpr::getCompare( llvm::CmpInst::ICMP_NE, lhs, rhs);

            case BinaryOperator::OPERATOR::LESS_EQ:
                return llvm::ConstantExpr::getCompare( llvm::CmpInst::ICMP_SLE, lhs, rhs);

            case BinaryOperator::OPERATOR::LESS:
                return llvm::ConstantExpr::getCompare( llvm::CmpInst::ICMP_SLT, lhs, rhs);

            case BinaryOperator::OPERATOR::MORE_EQ:
                return llvm::ConstantExpr::getCompare( llvm::CmpInst::ICMP_SGE, lhs, rhs);

            case BinaryOperator::OPERATOR::MORE:
                return llvm::ConstantExpr::getCompare( llvm::CmpInst::ICMP_SGT, lhs, rhs);

            case BinaryOperator::OPERATOR::PLUS:
                return llvm::ConstantExpr::getAdd(lhs, rhs);

            case BinaryOperator::OPERATOR::MINUS:
                return llvm::ConstantExpr::getSub(lhs, rhs);

            case BinaryOperator::OPERATOR::TIMES:
                return llvm::ConstantExpr::getMul(lhs, rhs);

            case BinaryOperator::OPERATOR::DIVISION:
            case BinaryOperator::OPERATOR::INTEGER_DIVISION:
                return llvm::ConstantExpr::getSDiv(lhs, rhs);

            case BinaryOperator::OPERATOR::MODULO:
                return llvm::ConstantExpr::getSRem(lhs, rhs);

            case BinaryOperator::OPERATOR::AND:
                return llvm::ConstantExpr::getAnd(lhs, rhs);

            case BinaryOperator::OPERATOR::OR:
                return llvm::ConstantExpr::getOr(lhs, rhs);

            case BinaryOperator::OPERATOR::XOR:
                return llvm::ConstantExpr::getXor(lhs, rhs);
        }
    }

/******************************************************************/

    llvm::ConstantInt* ConstantVisitor::operator() ( const BooleanConstant& b )
    {
        if ( b.value ) {
            return m_Builder.getTrue();
        }
        else {
            return m_Builder.getFalse();
        }
    }

    llvm::ConstantInt* ConstantVisitor::operator() ( const IntegerConstant& c )
    {
        return m_Builder.getInt32( c.value );
    }

/******************************************************************/

    TypeRes ConstantVisitor::operator() ( SimpleType t )
    {
        switch (t) {
        case ast::SimpleType::INTEGER:
            return m_Builder.getInt32Ty();

        case SimpleType::BOOLEAN:
            return m_Builder.getInt1Ty();
        }
    }

    TypeRes ConstantVisitor::operator() ( const ptr<Array>& arr )
    {
        // array [] of `inner`
        auto inner = compile_t( arr->elementType );
        auto low = compile_cexpr( arr->lowBound );
        auto high = compile_cexpr( arr->highBound );

        // Get the exact integer of `high - low`
        uint64_t size =
            llvm::ConstantExpr::getSub(high, low)->getUniqueInteger().getZExtValue();

        return wrap(inner).visit(
            // on inner simple type (for example array [] oif integer)
            // just use this type as inner
            [&]( llvm::Type* simple ) -> ArrayType
            {
                auto arr_t = llvm::ArrayType::get(simple, size);
                return { arr_t, { low } };
            },
            // on multidimensional array get the previous low bounds
            // and add the current one
            [&] ( ArrayType arr ) -> ArrayType
            {
                auto [t, prev_lows] = arr;
                auto curr_arr = llvm::ArrayType::get(t, size);

                // prepend new low bound
                prev_lows.insert( prev_lows.begin(), low );
                return { curr_arr, prev_lows };
            }
        );
    }

/******************************************************************/

    llvm::Value* ExprVisitor::local_or_global ( const std::string& name )
    {
        auto loc = m_Locals.find(name);
        if ( loc.has_value() ){
            return loc.value();
        }

        auto glob = m_Module.getNamedGlobal(name);
        if ( glob == nullptr ){
            throw std::runtime_error( "Usage of undeclared " + name );
        }

        return glob;
    }

    llvm::Value* ExprVisitor::variable_address ( const Expression& expr )
    {
        // only variables are allowed to be accessed by address
        if ( auto va = wrap(expr).get<VariableAccess>(); va.has_value() )
        {
            return local_or_global(va->identifier);
        }

        throw std::runtime_error( "Trying to use an address of a non-variable" );
    }

    std::pair<llvm::Type*, llvm::Value*> ExprVisitor::array_on_idxs ( const std::string& name, const std::vector<llvm::Value*>& idx )
    {
        auto arrPtr = local_or_global( name );
        auto lows = m_ArrayLows.find(name);

        // `name` is not an array
        if ( ! lows.has_value() )
        {
            throw std::runtime_error( "Invalid usage of " + name + " as an array.");
        }

        // array is accessed with as if it has more dimensions that it actualy has
        if( idx.size() > lows->size() )
        {
            throw std::runtime_error(
                name
                + " is a "
                + std::to_string(lows->size())
                + "-dimensional array, used as a "
                + std::to_string(idx.size())
                + "-dimensional"
            );
        }


        // Array of indexes
        std::vector<llvm::Value*> idxArr {};
        idxArr.reserve(idx.size() + 1);

        // Add the initial nitial zero
        idxArr.push_back(llvm::ConstantInt::get( m_Builder.getInt32Ty(), 0));
        for ( int i = 0; i < idx.size(); ++i )
        {
            idxArr.push_back( m_Builder.CreateAdd((*lows)[i], idx[i]) );
        }

        // Get the element on specified indicies
        auto* elem = m_Builder.CreateInBoundsGEP(
            arrPtr->getType()->getPointerElementType(), arrPtr, idxArr
        );

        // Get the actual type
        auto* type = elem->getType()->getPointerElementType();

        return { type, elem };
    }

/******************************************************************/

    llvm::Value* ExprVisitor::operator() ( const VariableAccess& va )
    {
        auto val = local_or_global(va.identifier);
        return m_Builder.CreateLoad( m_Builder.getInt32Ty(), val, va.identifier );
    }

    llvm::Value* ExprVisitor::operator() ( const ConstantExpression& c )
    {
        return compile_const ( c.value );
    }

    llvm::Value* ExprVisitor::operator() ( const ptr<ArrayAccess>& arr )
    {
        auto idx = compile_expr( arr->value );
        auto [type, elem] = array_on_idxs(arr->array, { idx });
        return m_Builder.CreateLoad( type, elem );
    }

    llvm::Value* ExprVisitor::operator() ( const ptr<SubprogramCall>& sub )
    {
        std::vector<llvm::Value*> args {};
        args.reserve(sub->arguments.size());

        // If function expects pointer, get a pointer to a variable instead of
        // an expression value
        if ( external::POINTER_FUNS.contains( sub->functionName ))
        {
            for ( const auto& p : sub->arguments ){
                args.push_back( variable_address( p ) );
            }
        }
        else {
            for ( const auto& p : sub->arguments ){
                args.push_back( compile_expr( p ) );
            }
        }

        return m_Builder.CreateCall(m_Module.getFunction(sub->functionName), args);
    }

    llvm::Value* ExprVisitor::operator() ( const ptr<UnaryOperator>& un )
    {
        auto val = compile_expr( un->expression );
        switch ( un->op ){
        case UnaryOperator::OPERATOR::PLUS:
            return val;

        case UnaryOperator::OPERATOR::MINUS:
            return m_Builder.CreateNeg( val );

        case UnaryOperator::OPERATOR::NOT:
            return m_Builder.CreateNot(val);
        }
    }

    llvm::Value* ExprVisitor::operator() ( const ptr<BinaryOperator>& bin )
    {
        auto lhs = compile_expr( bin->left );
        auto rhs = compile_expr( bin->right );

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

    llvm::BasicBlock* SubprogramVisitor::get_basic_block ( const std::string& name )
    {
        return llvm::BasicBlock::Create(
            m_Context,
            name,
            m_Builder.GetInsertBlock()->getParent(),
            m_ReturnBlock
        );
    }

/******************************************************************/


    void SubprogramVisitor::operator() ( const SubprogramCall& sub )
    {
        // Same as expression subprogram call, but the result is thrown away
        compile_expr( make_ptr<SubprogramCall>(sub) );
    }

    void SubprogramVisitor::operator() ( const Assignment& assign )
    {
        auto val = compile_expr( assign.value );
        auto ptr = local_or_global( assign.variable );
        m_Builder.CreateStore(val, ptr);
    }

    void SubprogramVisitor::operator() ( const ArrayAssignment& arr )
    {
        auto idx = compile_expr( arr.position );
        auto val = compile_expr( arr.value );
        auto [type, elem] = array_on_idxs(arr.array, { idx });

        m_Builder.CreateStore(val, elem);
    }

    void SubprogramVisitor::operator() ( const ExitStatement& )
    {
        m_Builder.CreateBr( m_ReturnBlock );

        auto bb = get_basic_block( "afterExit" );
        m_Builder.SetInsertPoint(bb);
    }

    void SubprogramVisitor::operator() ( const BreakStatement& )
    {
        if ( ! m_LoopContinuation.has_value() )
        {
            throw std::runtime_error( "Break used outside of loop" );
        }

        m_Builder.CreateBr( m_LoopContinuation.value() );

        auto bb = get_basic_block("afterBreak");
        m_Builder.SetInsertPoint(bb);
    }

    void SubprogramVisitor::operator() ( const EmptyStatement& )
    {
        return;
    }

    void SubprogramVisitor::operator() ( const ptr<Block>& bl )
    {
        compile_block(*bl);
    }

    void SubprogramVisitor::operator() ( const ptr<If>& if_ )
    {
        auto trueBB = get_basic_block("trueBranch");
        auto falseBB = get_basic_block("falseBranch");
        auto continueBB = get_basic_block( "afterIf");

        // conditional jump
        auto cond = compile_expr(if_->condition);
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
        compile_loop(wh->condition, wh->code, EmptyStatement{});
    }

    void SubprogramVisitor::operator() ( const ptr<For>& fo )
    {
        // initialization
        // `for iterator := init
        auto iterator = local_or_global( fo->loopVariable );
        auto init = compile_expr( fo->initialization );
        m_Builder.CreateStore( init, iterator );

        // Transform a for loop to while loop
        BinaryOperator::OPERATOR condOp;
        BinaryOperator::OPERATOR incrOp;
        switch (fo->direction) {
        case For::DIRECTION::TO:
            condOp = BinaryOperator::OPERATOR::LESS_EQ;
            incrOp = BinaryOperator::OPERATOR::PLUS;
            break;

        case For::DIRECTION::DOWNTO:
            condOp = BinaryOperator::OPERATOR::MORE_EQ;
            incrOp = BinaryOperator::OPERATOR::MINUS;
            break;
        }

        ast::Expression loopVar = ast::VariableAccess { fo->loopVariable };

        // i <= target or i >= targer
        ast::Expression cond =
            make_ptr<BinaryOperator>(condOp, loopVar, fo->target );

        // i = i + 1 or i = i - 1
        ast::Statement incr = Assignment{
                fo->loopVariable,
                make_ptr<BinaryOperator>(
                    incrOp,
                    loopVar,
                    ConstantExpression{ IntegerConstant{ 1 } }
                )
            };


        compile_loop(cond, fo->code, incr);
    }

/******************************************************************/

    void SubprogramVisitor::compile_block ( const Block& code )
    {
        for ( const auto& st : code.statements ){
            compile_stm( st );
        }
    }

    void SubprogramVisitor::compile_loop ( Expression condition, Statement block, Statement increment )
    {
        auto condBB = get_basic_block("loopCond");
        auto bodyBB = get_basic_block("loopBody");
        auto continueBB = get_basic_block("afterLoop");

        m_Builder.CreateBr(condBB);

        // compile condition
        m_Builder.SetInsertPoint( condBB );
        auto cond = compile_expr( condition );
        m_Builder.CreateCondBr( cond, bodyBB, continueBB );

        // this is done so nested loops don't break
        auto prevLoop = m_LoopContinuation;
        m_LoopContinuation = continueBB;

        // compile body
        m_Builder.SetInsertPoint( bodyBB );
        compile_stm( block );
        compile_stm( increment );
        m_Builder.CreateBr( condBB );

        // switch to continuation
        m_Builder.SetInsertPoint( continueBB );
        m_LoopContinuation = prevLoop;
    }

/******************************************************************/


    void ProgramVisitor::operator() ( const ProcedureDecl& proc )
    {
        compile_subprogram_decl(proc.name, proc.parameters, std::nullopt);
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

    void ProgramVisitor::operator() ( const FunctionDecl& fun )
    {
        compile_subprogram_decl(fun.name, fun.parameters, fun.returnType);
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
        auto val = compile_cexpr( c.value );
        new llvm::GlobalVariable(
            m_Module,
            val->getType(),
            true, // Is a constant
            llvm::GlobalVariable::ExternalLinkage,
            val,
            c.name
        );
    }

    void ProgramVisitor::operator() ( const Variable& var )
    {
        auto type = compile_t( var.type );

        llvm::Type* actual_type;

        wrap(type).visit(
            // On simple type just assign it
            [&]( llvm::Type* simple )
            {
                actual_type = simple;
            },
            // On array type also add it to array lows
            [&]( const ArrayType& arr )
            {
                actual_type = arr.first;
                m_ArrayLows.add(var.name, arr.second);
            }
        );

        // Initialized by zero
        auto zero = llvm::Constant::getNullValue(actual_type);

        new llvm::GlobalVariable(
            m_Module,
            actual_type,
            false, // Is a constant
            llvm::GlobalVariable::ExternalLinkage,
            zero,
            var.name
        );
    }

/******************************************************************/

    void ProgramVisitor::add_external_funcs()
    {
        for ( const auto& f : external::EXTERNAL_FUNCS ){
            wrap(f).visit(
                [&]( const ProcedureDecl& decl ){
                    compile_subprogram_decl(
                        decl.name,
                        decl.parameters,
                        std::nullopt,
                        external::POINTER_FUNS.contains(decl.name)
                    );
                },
                [&] ( const FunctionDecl& decl ){
                    compile_subprogram_decl(
                        decl.name,
                        decl.parameters,
                        decl.returnType,
                        external::POINTER_FUNS.contains(decl.name)
                    );
                }
            );
        }
    }

    void ProgramVisitor::compile_program ( const Program& program )
    {
        for ( const auto& g : program.globals )
        {
            compile_glob ( g );
        }

        compile_subprogram("main", {}, {}, SimpleType::INTEGER, program.code);
    }

/******************************************************************/

    llvm::Function* ProgramVisitor::compile_subprogram_decl(
        const Identifier& name,
        const Many<Variable>& parameters,
        const std::optional<Type>& retType,
        bool ptrParams)
    {
        // Return type
        llvm::Type * llvmReturnType;
        if ( retType.has_value() ) {
            llvmReturnType = compile_simple_t( retType.value() );
        }
        else {
            llvmReturnType = m_Builder.getVoidTy();
        }

        // Args
        std::vector<llvm::Type*> llvmParams;
        llvmParams.reserve( parameters.size() );
        for ( const auto& p : parameters )
        {
            auto t = compile_simple_t( p.type );
            if ( ptrParams ){
                t = t->getPointerTo();
            }
            llvmParams.push_back( t );
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

        return llvmFun;
    }

    void ProgramVisitor::compile_subprogram (
        const Identifier& name,
        const Many<Variable>& parameters,
        const Many<Variable>& variables,
        const std::optional<Type>& retType,
        const Block& code
    )
    {
        llvm::Function* llvmFun = m_Module.getFunction( name );

        if ( llvmFun == nullptr ){
            llvmFun = compile_subprogram_decl(name, parameters, retType);
        }
        else {
            // TODO validate correct structure
        }

        // Entry and return basic blocks
        auto entryBB = llvm::BasicBlock::Create(m_Context, "entry", llvmFun);
        auto returnBB = llvm::BasicBlock::Create(m_Context, "return", llvmFun);

        m_Builder.SetInsertPoint( entryBB );

        // Map of local variables, including parameters and return value
        DeclarationMap locals {};

        // Parameters
        for ( auto& a : llvmFun->args() ) {
            auto pAddr = m_Builder.CreateAlloca( a.getType(), nullptr, a.getName() + "_arg" );
            m_Builder.CreateStore( &a, pAddr );
            locals.add( a.getName().data(), pAddr );
        }

        // Local variables
        for ( const auto& v : variables )
        {
            auto vAddr = m_Builder.CreateAlloca( compile_simple_t(v.type), nullptr, v.name + "_var");
            locals.add(v.name, vAddr);
        }

        // Return address
        std::optional<llvm::Value*> returnAddress;
        if ( retType.has_value() ) {
            returnAddress = m_Builder.CreateAlloca( llvmFun->getReturnType(), nullptr, "return_addr" );
            locals.add(name, returnAddress.value());
        }
        else {
            returnAddress = std::nullopt;
        }

        // Code
        SubprogramVisitor visitor {
            { { m_Context, m_Builder, m_Module, m_ArrayLows }, locals },
            returnBB,
            std::nullopt // Not starting in a loop
        };
        visitor.compile_block( code );

        // Return
        m_Builder.CreateBr( returnBB );
        m_Builder.SetInsertPoint( returnBB );
        if ( returnAddress.has_value() ){
            auto retVal = m_Builder.CreateLoad( llvmFun->getReturnType(), returnAddress.value() );
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

        ProgramVisitor pr {
            {
                {compiler->m_Context, compiler->m_Builder, compiler->m_Module},
                {}
            }
        };
        pr.add_external_funcs();
        pr.compile_program( program );

        return compiler;
    }

    const llvm::Module& Compiler::get_module () const
    {
        return m_Module;
    }
}
