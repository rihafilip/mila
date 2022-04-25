#include "lexer.hpp"
#include "lexer_table.hpp"
#include "tokens.hpp"
#include "variant_helpers.hpp"
#include <cctype>
#include <optional>
#include <stdexcept>

using namespace token;


namespace lexer
{
    /// Bool wrapper struct that signifies if this is a start state
    template <typename St>
    struct is_start_state {
        constexpr static bool value = false;
    };

    template <>
    struct is_start_state<S> {
        constexpr static bool value = true;
    };

    /***********************************************/

    /// Get a state, its transition table and a character and make a transition
    template <typename St>
    std::optional<token::Token> transition ( const Table<St>& table, std::istream& stream, const St& state )
    {
        // Bad stream
        if ( stream.fail() ){
            throw std::runtime_error( "Error while reading input." );
        }

        // Handle EOF
        if ( stream.eof() || std::isspace( stream.peek() ) ){
            // On start state just return nothing
            if ( is_start_state<St>::value ){
                return std::nullopt;
            }

            // Try to extract a token from state, fail if there is none
            auto tk = table.m_ExtractToken( state );
            if ( tk.has_value() ){
                return tk.value();
            }
            else {
                throw std::runtime_error( "Unexpected end of input." );
            }
        }

        // Get next char
        char ch = stream.peek();

        // Try to find in map
        if ( auto i = table.m_Map.find( ch ); i != table.m_Map.end() ){
            stream.get();
            auto token_or_state = i->second( state, ch );

            // Transition more on state, return token on token
            return wrap(token_or_state).visit(
                []( const Token& tk ) -> std::optional<token::Token>
                {
                    return tk;
                },
                [&stream] ( const State& new_state )
                {
                    return transition_all( new_state, stream );
                }
            );
        }

        // Try to extract token on character that's not in table
        else if ( auto tok = table.m_ExtractToken( state ); tok.has_value() ) {
            return tok.value();
        }

        // Error
        else {
            throw std::runtime_error( std::string {"Unexpected character "} + ch );
        }
    }

    std::optional<token::Token> transition_all ( const State& state, std::istream& stream )
    {
        auto lam = [&stream] <typename St> ( const Table<St>& table ){
            return [&stream, &table] ( const St& state ){
                return transition( table, stream, state );
            };
        };

        return wrap( state ).visit(
            lam(S_TABLE),
            lam(DECIMAL_TABLE),
            lam(OCTAL_START_TABLE),
            lam(OCTAL_TABLE),
            lam(HEX_START_TABLE),
            lam(HEX_TABLE),
            lam(WORD_TABLE),
            lam(GREATER_THAN_TABLE),
            lam(LOWER_THAN_TABLE),
            lam(COLON_TABLE)
        );
    }

    /***********************************************/

    std::optional<token::Token> Lexer::next()
    {
        // Skip whitespaces
        while ( m_Data.good() && std::isspace( m_Data.peek() ) ){
            m_Data.get();
        }

        return transition_all(start_state(), m_Data);
    }
}
