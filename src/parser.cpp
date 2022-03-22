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

template <typename T>
void append( std::vector<T>& dest, const std::vector<T>& from )
{
    dest.reserve( from.size() );
    dest.insert( dest.end(), from.begin(), from.end() );
}

Many<Variable> identifierToVariable( const Many<ast::Identifier>& ids, Type t )
{
    Many<Variable> out{};
    out.reserve( ids.size() );
    std::ranges::transform( ids, std::back_inserter( out ),
        [&t]( const ast::Identifier& i ) -> Variable
        {
            return { i, t };
        }
    );

    return out;
}

namespace parser
{
    std::optional<WrappedToken> Parser::top()
    {
        if ( m_Data.empty() )
            return std::nullopt;

        return wrap( m_Data.top() );
    }

    WrappedToken Parser::pop()
    {
        if ( m_Data.empty() )
            throw std::runtime_error( "Parser error: Unexpected EOF" );

        auto x = m_Data.top();
        m_Data.pop();
        return wrap( x );
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
        auto [consts, varias, subp] = globals();
        auto code = block();
        popControlSymbol( CONTROL_SYMBOL::DOT );

        auto t = top();
        if ( t.has_value() )
            fail( "EOF", token::to_string( t->variant ) );

        return Program{ name, subp, consts, varias, code };
    }

    /*********************************************************************/

    Parser::Globals Parser::globals()
    {
        auto [consts, vars, subps] = Globals{};

        for ( auto v = top(); v.has_value(); v = top() )
        {
            if ( v->isEq( KEYWORD::CONST ) )
            {
                append( consts, constants() );
            }
            else if ( v->isEq( KEYWORD::VAR ) )
            {
                append( vars, variables() );
            }
            else if ( v->isEq( KEYWORD::FUNCTION ) )
            {
                subps.push_back( function() );
            }
            else if ( v->isEq( KEYWORD::PROCEDURE ) )
            {
                subps.push_back( procedure() );
            }
            else
            {
                break;
            }
        }

        return { consts, vars, subps };
    }

    /*********************************************************************/

    Many<NamedConstant> Parser::constants()
    {
        popKeyword( KEYWORD::CONST );

        Many<NamedConstant> acc{ single_constant() };

        // MoreConstants
        while ( lookup<token::Identifier>() )
        {
            acc.push_back( single_constant() );
        }
        return acc;
    }

    NamedConstant Parser::single_constant()
    {
        auto id = identifier();
        popOperator( OPERATOR::EQUAL );
        auto c = constant_literal();
        popControlSymbol( CONTROL_SYMBOL::SEMICOLON );
        return NamedConstant{ id, c };
    }

    /*********************************************************************/

    Many<Variable> Parser::variables()
    {
        popKeyword( KEYWORD::VAR );

        Many<Variable> acc{ single_variable() };

        /// MoreVariables
        while ( lookup<token::Identifier>() )
        {
            append( acc, single_variable() );
        }
        return acc;
    }


    Many<Variable> Parser::single_variable()
    {
        auto ids = identifier_list();
        popControlSymbol( CONTROL_SYMBOL::COLON );
        auto t = type();
        popControlSymbol( CONTROL_SYMBOL::SEMICOLON );

        return identifierToVariable( ids, t );
    }

    /*********************************************************************/

    Many<ast::Identifier> Parser::identifier_list()
    {
        Many<ast::Identifier> acc{ identifier() };

        // MoreIdentifierList
        while ( lookupEq( CONTROL_SYMBOL::COMMA ) )
        {
            popControlSymbol( CONTROL_SYMBOL::COMMA );
            acc.push_back( identifier() );
        }

        return acc;
    }

    /*********************************************************************/

    Subprogram Parser::procedure()
    {
        popKeyword( KEYWORD::PROCEDURE );
        auto n = identifier();
        auto ps = parameters();
        popControlSymbol( CONTROL_SYMBOL::SEMICOLON );

        auto b = body();

        if ( b.has_value() )
        {
            return Procedure{ n, ps, b->first, b->second };
        }
        else
        {
            return ProcedureDecl{ n, ps };
        }
    }

    Subprogram Parser::function()
    {
        popKeyword( KEYWORD::FUNCTION );
        auto n = identifier();
        auto ps = parameters();

        popControlSymbol( CONTROL_SYMBOL::COLON );
        auto t = type();
        popControlSymbol( CONTROL_SYMBOL::SEMICOLON );

        auto b = body();

        if ( b.has_value() )
        {
            return Function{ n, ps, t, b->first, b->second };
        }
        else
        {
            return FunctionDecl{ n, ps, t };
        }
    }

    /*********************************************************************/

    Many<Variable> Parser::parameters()
    {
        if ( !lookupEq( CONTROL_SYMBOL::BRACKET_OPEN ) )
        {
            return {};
        }

        popControlSymbol( CONTROL_SYMBOL::BRACKET_OPEN );

        Many<Variable> acc{ single_parameter() };

        // MoreParameters
        while ( lookupEq( CONTROL_SYMBOL::COMMA ) )
        {
            popControlSymbol( CONTROL_SYMBOL::COMMA );
            append( acc, single_parameter() );
        }

        popControlSymbol( CONTROL_SYMBOL::BRACKET_CLOSE );
        return acc;
    }

    Many<Variable> Parser::single_parameter()
    {
        auto ids = identifier_list();
        popControlSymbol( CONTROL_SYMBOL::COLON );
        auto t = type();

        return identifierToVariable( ids, t );
    }

    /*********************************************************************/

    std::optional<std::pair<Many<Variable>, Block>> Parser::body()
    {
        if ( lookupEq( KEYWORD::FORWARD ) )
        {
            popKeyword( KEYWORD::FORWARD );
            popControlSymbol( CONTROL_SYMBOL::SEMICOLON );
            return std::nullopt;
        }

        auto vars = variables();
        auto b = block();
        return {{ vars, b }};
    }

    /*********************************************************************/

    Block Parser::block()
    {
        popKeyword( KEYWORD::BEGIN );

        // Stats
        Many<Statement> acc { stat() };
        while ( lookupEq( CONTROL_SYMBOL::SEMICOLON ) )
        {
            popControlSymbol( CONTROL_SYMBOL::SEMICOLON );
            acc.push_back( stat() );
        }

        popKeyword( KEYWORD::END );

        return Block { acc };
    }

    Statement Parser::stat()
    {
        // StatId
        if ( lookup<token::Identifier>())
        {
            auto id = identifier();
            if ( lookupEq( OPERATOR::ASSIGNEMENT ))
            {
                popOperator( OPERATOR::ASSIGNEMENT );
                auto ex = expr();
                return Assignment { id, ex };
            }
            else if ( lookupEq( CONTROL_SYMBOL::SQUARE_BRACKET_OPEN ) )
            {
                popControlSymbol( CONTROL_SYMBOL::SQUARE_BRACKET_OPEN );
                auto pos = expr();
                popControlSymbol( CONTROL_SYMBOL::SQUARE_BRACKET_CLOSE );
                popOperator( OPERATOR::ASSIGNEMENT );
                auto val = expr();
                return ArrayAssignment { id, pos, val };
            }
            else if ( lookupEq( CONTROL_SYMBOL::BRACKET_OPEN ) )
            {
                popControlSymbol( CONTROL_SYMBOL::BRACKET_OPEN );
                auto args = arguments();
                popControlSymbol( CONTROL_SYMBOL::BRACKET_CLOSE );
                return SubprogramCall { id, args };
            }
            else
            {
                fail( "assignment or subprogram call",
                    token::to_string( top()->variant ) );
            }
        }

        if ( lookupEq( KEYWORD::BEGIN) )
        {
            return make_ptr<Block>( block() );
        }

        if ( lookupEq( KEYWORD::IF ) )
        {
            popKeyword(KEYWORD::IF);
            auto exp = expr();
            popKeyword( KEYWORD::THEN );
            auto true_b = stat();
            if ( ! lookupEq ( KEYWORD::ELSE ) )
            {
                return make_ptr<If>( exp, true_b, std::nullopt );
            }
            popKeyword( KEYWORD::ELSE );
            auto false_b = stat();
            return make_ptr<If>( exp, true_b, false_b );
        }

        if ( lookupEq( KEYWORD::WHILE) )
        {
            popKeyword( KEYWORD::WHILE );
            auto exp = expr();
            popKeyword( KEYWORD::DO );
            auto st = stat();
            return make_ptr<While>( exp, st );
        }

        if ( lookupEq( KEYWORD::FOR ) )
        {
            popKeyword( KEYWORD::FOR );
            auto id = identifier();
            popOperator( OPERATOR::ASSIGNEMENT );
            auto init = expr();

            For::DIRECTION dir;
            if ( lookupEq( KEYWORD::TO ) )
            {
                popKeyword( KEYWORD::TO );
                dir = For::DIRECTION::TO;
            }
            else if ( lookupEq( KEYWORD::DOWNTO) )
            {
                popKeyword( KEYWORD::DOWNTO );
                dir = For::DIRECTION::DOWNTO;
            }
            else
            {
                fail( "to or downto", token::to_string( top()->variant ));
            }

            auto target = expr();
            popKeyword( KEYWORD::DO );
            auto st = stat();

            return make_ptr<For>( id, init, dir, target, st );
        }

        fail( "statement", token::to_string( top()->variant ) );
    }

}
