#include "lexer.hpp"
#include "lexer_table.hpp"
#include "tokens.hpp"
#include "variant_helpers.hpp"
#include <cctype>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <optional>
#include <stdexcept>

using namespace token;


namespace lexer
{
    /**
     * @brief Apply a given function with a transition table to the given state
     *
     * @param state State to be visited
     * @param apply Function to apply
     * @return auto Return of the given apply function
     */
    inline auto on_all ( const State& state, const auto& apply )
    {
        return wrap( state ).visit(
            std::bind_front(apply, S_TABLE),
            std::bind_front(apply, DECIMAL_TABLE),
            std::bind_front(apply, OCTAL_START_TABLE),
            std::bind_front(apply, OCTAL_TABLE),
            std::bind_front(apply, HEX_START_TABLE),
            std::bind_front(apply, HEX_TABLE),
            std::bind_front(apply, WORD_TABLE),
            std::bind_front(apply, GREATER_THAN_TABLE),
            std::bind_front(apply, LOWER_THAN_TABLE),
            std::bind_front(apply, COLON_TABLE),
            std::bind_front(apply, DOT_TABLE)
        );
    }

    /// On all states apply a transition function from their state
    inline std::pair<TransitionReturn, bool> transition_all ( const State& state, char ch, Position pos )
    {
        return on_all( state,
            [&ch, &pos] <typename St> ( const Table<St>& table, const St& state ) -> std::pair<TransitionReturn, bool>
            {
                // Transition succesful
                if ( auto i = table.m_Map.find( ch ); i != table.m_Map.end() ){
                    return {i->second( state, ch ), true};
                }

                // Extraction succesful
                else if ( auto tok = table.m_ExtractToken( state ); tok.has_value() ) {
                    return {tok.value(), false};
                }

                // Error
                else {
                    throw std::runtime_error(
                        std::string {"Unexpected character "}
                        + ch
                        + " (line "
                        + std::to_string( pos.line )
                        + ", column "
                        + std::to_string( pos.column )
                        + ")"
                    );
                }
            }
        );
    }

    /// On all states, try to extract a token
    inline std::optional<token::Token> extract_all ( const State& state )
    {
        return on_all( state,
            [] <typename St> ( const Table<St>& table, const St& state)
            {
                return table.m_ExtractToken( state );
            }
        );
    }

    /**
     * @brief The actual driver making the transitions of the state machine
     *
     * @param current_state The actuall state
     * @param pos Current position in stream
     * @param stream Stream to pull characters from
     * @return std::optional<token::Token> Token on succesful lexing, nullopt on EOF
     */
    std::optional<token::Token> lexer_driver ( const State& current_state, Position& pos, std::istream& stream )
    {
        // Bad stream
        if ( stream.bad() ) {
            throw std::runtime_error(
                "Error while reading input at line "
                + std::to_string(pos.line)
                + ", column "
                + std::to_string(pos.column)
            );
        }

        if ( stream.eof() ){
            // On start state just return nothing
            if ( wrap(current_state).get<S>().has_value() ){
                return std::nullopt;
            }

            auto tk = extract_all( current_state );
            if ( tk.has_value() ){
                return tk.value();
            }
            else {
                throw std::runtime_error( "Unexpected end of input." );
            }
        }

        // Handle EOF
        if ( std::isspace( stream.peek() ) ){

            // On start state just return nothing
            if ( wrap(current_state).get<S>().has_value() ){
                return std::nullopt;
            }

            auto tk = extract_all( current_state );
            if ( tk.has_value() ){
                return tk.value();
            }
            else {
                throw std::runtime_error( "Unexpected ws." );
            }
        }

        // Try to transition
        char ch = stream.peek();
        auto [ret, consumed] = transition_all( current_state, ch, pos );

        if ( consumed )
        {
            stream.get();
            pos.column++;
        }

        // On token return
        // On state transition continue
        return wrap( ret ).visit(
            [] ( const Token& tok ) -> std::optional<token::Token>
            {
                return tok;
            },
            [&]( const State& st ) -> std::optional<token::Token>
            {
                return lexer_driver( st, pos, stream);
            }
        );
    }


    /***********************************************/

    std::optional<token::Token> Lexer::next()
    {
        // Skip whitespaces
        while ( m_Data.good() && std::isspace( m_Data.peek() ) ){
            char ch = m_Data.get();
            if ( ch == '\n' ){
                m_Position.line++;
                m_Position.column = 0;
            }
            else {
                m_Position.column++;
            }
        }

        return lexer_driver(start_state(), m_Position, m_Data);
    }

    Position Lexer::position() const
    {
        return m_Position;
    }
}
