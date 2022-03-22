#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast.hpp"
#include "tokens.hpp"
#include "variant_helpers.hpp"

#include <stack>
#include <algorithm>

namespace parser
{
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
        Program parse( const std::vector<token::Token>& tks );

    private:
        /// Used stack
        std::stack<token::Token, std::vector<token::Token>> m_Data;

        Parser( const std::vector<token::Token>& tks )
            : m_Data( tks )
        {}

        /// Look at the top token
        std::optional<WrappedToken> top();

        /// Pop one token from stack, throwing error on empty
        WrappedToken pop();

        /**
         * \defgroup Pop Pop a specific token, failing if it is not present
         * @{
         */
        void popOperator( token::OPERATOR op );
        void popControlSymbol( token::CONTROL_SYMBOL cs );
        void popKeyword( token::KEYWORD kw );

        /// Identifier parser, practically a pop wrapper
        ast::Identifier identifier();
        /// Constant parser, practically a pop wrapper
        Constant constant_literal();
        /// @}

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

        /**
         * \defgroup Ind_parsers Individual token parsers
         * @{
         */
        Program program();

        using Globals =
            std::tuple<Many<NamedConstant>, Many<Variable>, Many<Subprogram>>;

        Globals globals();

        Many<NamedConstant> constants();
        NamedConstant single_constant();

        Many<Variable> variables();
        Many<Variable> single_variable();

        Many<ast::Identifier> identifier_list ();

        Subprogram procedure();
        Subprogram function();

        Many<Variable> parameters ();
        Many<Variable> single_parameter ();

        /// Returns Block if body is present, nullopt on forwarding
        std::optional<std::pair<Many<Variable>, Block>> body ();

        Block block();
        Statement stat();

        Expression expr();
        Many<Expression> arguments();

        Type type();
        /// @}
    };
}

#endif // PARSER_HPP
