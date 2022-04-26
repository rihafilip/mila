#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast.hpp"
#include "lexer.hpp"
#include "stack_wrap.hpp"
#include "tokens.hpp"
#include "variant_helpers.hpp"

#include <stack>
#include <algorithm>

namespace parser
{
    using LexStack = cont::StackWrapper<token::Token, lexer::Lexer>;

    std::optional<token::Token> stack_wrap_adaptor ( lexer::Lexer& lex );

    /// Alias for Token inside VariantWrapper
    using WrappedToken = VariantWrapper<
        token::OPERATOR, token::CONTROL_SYMBOL,
        token::KEYWORD, token::Identifier,
        token::Integer, token::Boolean>;

    using namespace ast;

    /// Parser that parses incomming vector of tokens
    class Parser
    {
    public:
        /// Run the parsing, return an AST
        static Program parse( std::istream& str );

    private:
        /// Lexer
        LexStack m_Data;

        Parser( std::istream& str )
            : m_Data( str, stack_wrap_adaptor )
        {}

        /// Look at the top token
        std::optional<WrappedToken> top();

        /// Pop one token from stack, throwing error on empty
        WrappedToken pop();

        // Pop a specific token, failing if it is not present
        // TODO refactor
        void pop( token::OPERATOR op );
        void pop( token::CONTROL_SYMBOL cs );
        void pop( token::KEYWORD kw );

        /// Pop a single operator
        token::OPERATOR popOperator();
        /// Identifier parser, practically a pop wrapper
        ast::Identifier identifier();
        /// Constant parser, practically a pop wrapper
        Constant constant_literal();
        /// Pop a single integer literal
        long long integer_literal();

        /// Look if top token is of given type
        template <typename T>
        bool lookup()
        {
            auto x = top();
            if ( x.has_value() && x->get<T>().has_value() )
                return true;

            return false;
        }

        /// Look if top token is of given type and value
        template <typename T>
        bool lookupEq( const T& v )
        {
            return lookupEq( { v } );
        }

        /// Look if top token is of given type and one of given values
        template <typename T>
        bool lookupEq( std::initializer_list<T> vs )
        {
            auto x = top();
            if ( x.has_value() )
            {
                return std::ranges::any_of( vs,
                    [&x]( const auto& a )
                    {
                        return x->isEq( a );
                    } );
            }

            return false;
        }

        /// Failure in parsing
        [[noreturn]] void fail( const std::string& expected, const token::Token& got );

        // Individual token parsers

        Program program();

        using Globals =
            std::tuple<Many<NamedConstant>, Many<Variable>, Many<Subprogram>>;

        Globals globals();

        Many<NamedConstant> constants();
        NamedConstant single_constant();

        Many<Variable> variables();
        Many<Variable> single_variable();

        Many<ast::Identifier> identifier_list();

        Subprogram procedure();
        Subprogram function();

        Many<Variable> parameters();
        Many<Variable> single_parameter();

        /// Returns Block if body is present, nullopt on forwarding
        std::optional<std::pair<Many<Variable>, Block>> body();

        Many<Variable> many_variables ();

        Block block();
        Statement stat();
        Statement statId();

        // _p is used because they are keywords
        If if_p();
        While while_p();
        For for_p();

        Expression expr();
        Expression simple_expr();
        Expression more_simple_expr( const Expression & lhs );
        Expression term();
        Expression more_term( const Expression & lhs );
        Expression factor();

        Many<Expression> arguments();

        Type type();
    };
}

#endif // PARSER_HPP
