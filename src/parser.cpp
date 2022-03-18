#include "parser.hpp"

#include <functional>

using namespace ast;
using namespace token;

/*********************************************************************/
// Fail functions

[[noreturn]] void fail( const std::string& expected, const std::string& got )
{
    throw std::runtime_error(
        "Parser error: Expected "
        + expected
        + ", but instead got "
        + got
    );
}

[[noreturn]] void tokenFail( const Token& expected, const Token& got )
{
    fail( token::to_string( expected ), token::to_string( got ) );
}

/*********************************************************************/
// Helper functions

Subprogram declToFun( Many<Variable> vs, Block code, const FunctionDecl& d )
{
    return Function{ d.name, d.parameters, d.returnType, vs, code };
}

Subprogram declToProc( Many<Variable> vs, Block code, const ProcedureDecl& d )
{
    return Procedure{ d.name, d.parameters, vs, code };
}

template <typename T>
auto retype()
{
    return []( const auto& a ) -> T
    {
        return a;
    };
}

// constexpr auto doNothing = []( const auto& ) -> void {};

namespace parser
{
    WrappedToken Parser::pop()
    {
        if ( m_Data.empty() )
            throw std::runtime_error( "Parser error: Unexpected EOF" );

        auto x = m_Data.top();
        m_Data.pop();
        return wrap( x );
    }

    std::optional<WrappedToken> Parser::top()
    {
        if ( m_Data.empty() )
            return std::nullopt;

        return wrap( m_Data.top() );
    }

    /*********************************************************************/

    void Parser::popOperator( token::OPERATOR op )
    {
        auto tk = pop();
        if ( tk.isEq( op ) )
            return;

        tokenFail( op, tk.variant );
    }

    void Parser::popControlSymbol( token::CONTROL_SYMBOL cs )
    {
        auto tk = pop();
        if ( tk.isEq( cs ) )
            return;

        tokenFail( cs, tk.variant );
    }

    void Parser::popKeyword( token::KEYWORD kw )
    {
        auto tk = pop();
        if ( tk.isEq( kw ) )
            return;

        tokenFail( kw, tk.variant );
    }

    /*********************************************************************/

    ast::Identifier Parser::identifier()
    {
        auto t = pop();
        auto v = t.get<token::Identifier>();
        if ( v.has_value() )
            return v->value;

        fail( "identifier", token::to_string( t.variant ) );
    }

    Constant Parser::constant_literal()
    {
        return pop().visit(
            []( token::Integer i ) -> Constant
            {
                return IntegerConstant{ i.value };
            },

            []( token::Boolean b ) -> Constant
            {
                return BooleanConstant{ b.value };
            },

                []( auto t ) -> Constant
            {
                fail( "constant", token::to_string( t ) );
            }
            );
    }

    /*********************************************************************/

    Program Parser::program()
    {
        popKeyword( KEYWORD::PROGRAM );
        auto name = identifier();
        popControlSymbol( CONTROL_SYMBOL::SEMICOLON );
        auto subp = subprogs();
        auto [consts, varias] = globals();
        auto code = block();
        popControlSymbol( CONTROL_SYMBOL::DOT );

        auto t = top();
        if ( t.has_value() )
            fail ( "EOF", token::to_string( t->variant ) );

        return Program{ name, subp, consts, varias, code };
    }

    Many<Subprogram> Parser::subprogs( Many<Subprogram> acc )
    {
        if ( lookupEq( { KEYWORD::FUNCTION, KEYWORD::PROCEDURE } ) )
        {
            acc.push_back( subprog() );
            popControlSymbol( CONTROL_SYMBOL::SEMICOLON );
            return subprogs( acc );
        }

        return acc;
    }

    Subprogram Parser::subprog()
    {
        auto decl = wrap( subprog_decl() );

        if ( lookupEq( KEYWORD::FORWARD ) )
        {
            pop();
            return decl.visit( retype<Subprogram>() );
        }

        else if ( lookupEq( { KEYWORD::VAR, KEYWORD::BEGIN } ) )
        {
            auto vs = vars();
            auto code = block();
            return decl.visit(
                std::bind_front( declToFun, vs, code ),
                std::bind_front( declToProc, vs, code )
            );
        }

        fail( "function declaration or definition", token::to_string( top()->variant ) );
    }

    SubprogramDecl Parser::subprog_decl()
    {
        auto v = pop();
        if ( v.isEq( KEYWORD::FUNCTION ) )
        {
            auto id = identifier();
            popControlSymbol( CONTROL_SYMBOL::BRACKET_OPEN );
            auto ps = params1();
            popControlSymbol( CONTROL_SYMBOL::BRACKET_CLOSE );
            popControlSymbol( CONTROL_SYMBOL::COLON );
            auto t = type();
            popControlSymbol( CONTROL_SYMBOL::SEMICOLON );
            return FunctionDecl{ id, ps, t };
        }
        else if ( v.isEq( KEYWORD::PROCEDURE ) )
        {
            auto id = identifier();
            popControlSymbol( CONTROL_SYMBOL::BRACKET_OPEN );
            auto ps = params1();
            popControlSymbol( CONTROL_SYMBOL::BRACKET_CLOSE );
            popControlSymbol( CONTROL_SYMBOL::SEMICOLON );
            return ProcedureDecl{ id, ps };
        }

        fail( "Procedure or function declaration", token::to_string( v.variant ) );
    }

    // /*********************************************************************/

    Many<Variable> Parser::params1()
    {
        Many<Variable> acc{};

        // top is empty or not identifier -> no params
        if ( ! lookup<token::Identifier>() )
        {
            return acc;
        }

        acc.push_back( param() );

        if ( lookupEq( CONTROL_SYMBOL::SEMICOLON ) )
        {
            return params( acc );
        }

        return acc;
    }

    Many<Variable> Parser::params( Many<Variable> acc )
    {
        popControlSymbol( CONTROL_SYMBOL::SEMICOLON );
        acc.push_back( param() );

        if ( lookupEq( CONTROL_SYMBOL::SEMICOLON ) )
        {
            return params( acc );
        }

        return acc;
    }

    Variable Parser::param()
    {
        auto id = identifier();
        popControlSymbol( CONTROL_SYMBOL::COLON );
        auto t = type();
        return Variable{ id, t };
    }

    /*********************************************************************/

    std::pair<Many<NamedConstant>, Many<Variable>> Parser::globals( Many<NamedConstant> consts, Many<Variable> variables )
    {
        if ( lookupEq( KEYWORD::CONST ) )
        {
            auto toAdd = constants();
            consts.insert( consts.end(), toAdd.begin(), toAdd.end() );
            return globals( consts, variables );
        }
        else if ( lookupEq( KEYWORD::VAR ) )
        {
            auto toAdd = vars();
            variables.insert( variables.end(), toAdd.begin(), toAdd.end() );
            return globals( consts, variables );
        }

        return { consts, variables };
    }

    /*********************************************************************/

    Many<NamedConstant> Parser::constants()
    {
        Many<NamedConstant> acc{};

        popKeyword( KEYWORD::CONST );
        acc.push_back( named_constant() );
        return constant_defs( acc );
    }

    Many<NamedConstant> Parser::constant_defs( Many<NamedConstant> acc )
    {
        if ( !lookup<token::Identifier>() )
        {
            return acc;
        }

        acc.push_back( named_constant() );
        return constant_defs( acc );
    }

    NamedConstant Parser::named_constant()
    {
        auto id = identifier();
        popControlSymbol( CONTROL_SYMBOL::COLON );
        auto c = constant_literal();
        popControlSymbol( CONTROL_SYMBOL::SEMICOLON );
        return NamedConstant{ id, c };
    }

    /*********************************************************************/

    Many<Variable> Parser::vars()
    {
        Many<Variable> acc{};

        popKeyword( KEYWORD::VAR );
        auto toAdd = variable();
        acc.insert( acc.end(), toAdd.begin(), toAdd.end() );
        return var_defs( acc );
    }
    Many<Variable> Parser::var_defs( Many<Variable> acc )
    {
        if ( !lookup<token::Identifier>() )
            return acc;

        auto toAdd = variable();
        acc.insert( acc.end(), toAdd.begin(), toAdd.end() );
        return var_defs( acc );
    }

    Many<Variable> Parser::variable()
    {
        auto ids = var_identifiers( { identifier() } );
        popControlSymbol( CONTROL_SYMBOL::COLON );
        auto t = type();
        popControlSymbol( CONTROL_SYMBOL::SEMICOLON );

        Many<Variable> out {};
        out.reserve( ids.size() );
        for ( const auto& i : ids )
        {
            out.emplace_back( i, t );
        }

        return out;
    }

    Many<ast::Identifier> Parser::var_identifiers( Many<ast::Identifier> acc )
    {
        if ( ! lookupEq( CONTROL_SYMBOL::COMMA ) )
            return acc;

        popControlSymbol( CONTROL_SYMBOL::COMMA );
        acc.push_back( identifier() );
        return var_identifiers( acc );
    }

    /*********************************************************************/


    Block Parser::block()
    {
        popKeyword( KEYWORD::BEGIN );
        auto sts = stats();
        popKeyword( KEYWORD::END );

        return Block { sts };
    }

    Many<Statement> Parser::stats()
    {

    }

    Statement Parser::stat()
    {

    }

    /*********************************************************************/

    Expression Parser::expr()
    {

    }
    Many<Expression> Parser::args1()
    {

    }
    Many<Expression> Parser::args()
    {

    }

    /*********************************************************************/

    Expression Parser::op1()
    {

    }
    Expression Parser::op2()
    {

    }
    Expression Parser::op3()
    {

    }
    Expression Parser::op4()
    {

    }

    /*********************************************************************/

    Type Parser::type()
    {

    }
}
