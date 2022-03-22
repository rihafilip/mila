#include "parser.hpp"

#include <functional>

using namespace ast;
using namespace token;

/*********************************************************************/
// Fail functions

[[noreturn]] void fail( const std::string& expected, const Token& got )
{
    throw std::runtime_error(
        "Parser error: Expected "
        + expected
        + ", but instead got "
        + token::to_string( got )
    );
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

    /*********************************************************************/

    WrappedToken Parser::pop()
    {
        if ( m_Data.empty() )
            throw std::runtime_error( "Parser error: Unexpected EOF" );

        auto x = m_Data.top();
        m_Data.pop();
        return wrap( x );
    }

    void Parser::pop( token::OPERATOR op )
    {
        auto tk = pop();
        if ( tk.isEq( op ) )
            return;

        fail( token::to_string( op ), tk.variant );
    }

    void Parser::pop( token::CONTROL_SYMBOL cs )
    {
        auto tk = pop();
        if ( tk.isEq( cs ) )
            return;

        fail( token::to_string( cs ), tk.variant );
    }

    void Parser::pop( token::KEYWORD kw )
    {
        auto tk = pop();
        if ( tk.isEq( kw ) )
            return;

        fail( token::to_string( kw ), tk.variant );
    }

    /*********************************************************************/

    ast::Identifier Parser::identifier()
    {
        auto t = pop();
        auto v = t.get<token::Identifier>();
        if ( v.has_value() )
            return v->value;

        fail( "identifier", t.variant );
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
                fail( "constant", t );
            }
            );
    }

    /*********************************************************************/

    Program Parser::program()
    {
        pop( KEYWORD::PROGRAM );
        auto name = identifier();
        pop( CONTROL_SYMBOL::SEMICOLON );
        auto [consts, varias, subp] = globals();
        auto code = block();
        pop( CONTROL_SYMBOL::DOT );

        auto t = top();
        if ( t.has_value() )
            fail( "EOF", t->variant );

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
        pop( KEYWORD::CONST );

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
        pop( OPERATOR::EQUAL );
        auto c = constant_literal();
        pop( CONTROL_SYMBOL::SEMICOLON );
        return NamedConstant{ id, c };
    }

    /*********************************************************************/

    Many<Variable> Parser::variables()
    {
        pop( KEYWORD::VAR );

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
        pop( CONTROL_SYMBOL::COLON );
        auto t = type();
        pop( CONTROL_SYMBOL::SEMICOLON );

        return identifierToVariable( ids, t );
    }

    /*********************************************************************/

    Many<ast::Identifier> Parser::identifier_list()
    {
        Many<ast::Identifier> acc{ identifier() };

        // MoreIdentifierList
        while ( lookupEq( CONTROL_SYMBOL::COMMA ) )
        {
            pop( CONTROL_SYMBOL::COMMA );
            acc.push_back( identifier() );
        }

        return acc;
    }

    /*********************************************************************/

    Subprogram Parser::procedure()
    {
        pop( KEYWORD::PROCEDURE );
        auto n = identifier();
        auto ps = parameters();
        pop( CONTROL_SYMBOL::SEMICOLON );

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
        pop( KEYWORD::FUNCTION );
        auto n = identifier();
        auto ps = parameters();

        pop( CONTROL_SYMBOL::COLON );
        auto t = type();
        pop( CONTROL_SYMBOL::SEMICOLON );

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

        pop( CONTROL_SYMBOL::BRACKET_OPEN );

        Many<Variable> acc{ single_parameter() };

        // MoreParameters
        while ( lookupEq( CONTROL_SYMBOL::COMMA ) )
        {
            pop( CONTROL_SYMBOL::COMMA );
            append( acc, single_parameter() );
        }

        pop( CONTROL_SYMBOL::BRACKET_CLOSE );
        return acc;
    }

    Many<Variable> Parser::single_parameter()
    {
        auto ids = identifier_list();
        pop( CONTROL_SYMBOL::COLON );
        auto t = type();

        return identifierToVariable( ids, t );
    }

    /*********************************************************************/

    std::optional<std::pair<Many<Variable>, Block>> Parser::body()
    {
        if ( lookupEq( KEYWORD::FORWARD ) )
        {
            pop( KEYWORD::FORWARD );
            pop( CONTROL_SYMBOL::SEMICOLON );
            return std::nullopt;
        }

        auto vars = variables();
        auto b = block();
        return { { vars, b } };
    }

    /*********************************************************************/

    Block Parser::block()
    {
        pop( KEYWORD::BEGIN );

        // Stats
        Many<Statement> acc{ stat() };
        while ( lookupEq( CONTROL_SYMBOL::SEMICOLON ) )
        {
            pop( CONTROL_SYMBOL::SEMICOLON );
            acc.push_back( stat() );
        }

        pop( KEYWORD::END );

        return Block{ acc };
    }

    Statement Parser::stat()
    {
        if ( lookup<token::Identifier>() )
            return statId();

        if ( lookupEq( KEYWORD::BEGIN ) )
            return make_ptr<Block>( block() );

        if ( lookupEq( KEYWORD::IF ) )
            return make_ptr<If>( if_p() );

        if ( lookupEq( KEYWORD::WHILE ) )
            return make_ptr<While>( while_p() );

        if ( lookupEq( KEYWORD::FOR ) )
            return make_ptr<For>( for_p() );

        return EmptyStatement{};
    }

    Statement Parser::statId()
    {
        auto id = identifier();
        if ( lookupEq( OPERATOR::ASSIGNEMENT ) )
        {
            pop( OPERATOR::ASSIGNEMENT );
            auto ex = expr();
            return Assignment{ id, ex };
        }

        else if ( lookupEq( CONTROL_SYMBOL::SQUARE_BRACKET_OPEN ) )
        {
            pop( CONTROL_SYMBOL::SQUARE_BRACKET_OPEN );
            auto pos = expr();
            pop( CONTROL_SYMBOL::SQUARE_BRACKET_CLOSE );
            pop( OPERATOR::ASSIGNEMENT );
            auto val = expr();
            return ArrayAssignment{ id, pos, val };
        }

        else if ( lookupEq( CONTROL_SYMBOL::BRACKET_OPEN ) )
        {
            pop( CONTROL_SYMBOL::BRACKET_OPEN );
            auto args = arguments();
            pop( CONTROL_SYMBOL::BRACKET_CLOSE );
            return SubprogramCall{ id, args };
        }

        else
        {
            fail( "assignment or subprogram call", top()->variant );
        }
    }

    If Parser::if_p()
    {
        pop( KEYWORD::IF );
        auto exp = expr();
        pop( KEYWORD::THEN );
        auto true_b = stat();
        if ( !lookupEq( KEYWORD::ELSE ) )
        {
            return { exp, true_b, std::nullopt };
        }
        pop( KEYWORD::ELSE );
        auto false_b = stat();
        return { exp, true_b, false_b };
    }

    While Parser::while_p()
    {
        pop( KEYWORD::WHILE );
        auto exp = expr();
        pop( KEYWORD::DO );
        auto st = stat();
        return { exp, st };
    }

    For Parser::for_p()
    {
        pop( KEYWORD::FOR );
        auto id = identifier();
        pop( OPERATOR::ASSIGNEMENT );
        auto init = expr();

        For::DIRECTION dir;
        if ( lookupEq( KEYWORD::TO ) )
        {
            pop( KEYWORD::TO );
            dir = For::DIRECTION::TO;
        }
        else if ( lookupEq( KEYWORD::DOWNTO ) )
        {
            pop( KEYWORD::DOWNTO );
            dir = For::DIRECTION::DOWNTO;
        }
        else
        {
            fail( "to or downto", top()->variant );
        }

        auto target = expr();
        pop( KEYWORD::DO );
        auto st = stat();
        return { id, init, dir, target, st };
    }
}
