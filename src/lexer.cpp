#include "lexer.hpp"
#include "lexer_table.hpp"
#include "tokens.hpp"
#include "variant_helpers.hpp"
#include <cctype>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace token;


namespace lexer
{
    /***********************************************/

    void Lexer::skip_ws()
    {
        while ( m_Data.good() && std::isspace( m_Data.peek() ) ){
            m_Data.get();
        }
    }

    /***********************************************/

    /**
     * @brief Visit all states with table and function
     *
     * @param fun Function returning visiting function on passing table
     * @param st State to visit
     * @return auto Output of fun on visiting
     */
    auto visit_with_tables ( const auto& fun, const State& st ){
        return wrap( st ).visit(
            fun(S_TABLE),
            fun(DECIMAL_TABLE),
            fun(OCTAL_START_TABLE),
            fun(OCTAL_TABLE),
            fun(HEX_START_TABLE),
            fun(HEX_TABLE),
            fun(WORD_TABLE),
            fun(GREATER_THAN_TABLE),
            fun(LOWER_THAN_TABLE),
            fun(COLON_TABLE)
        );
    }

    std::optional<token::Token> Lexer::handleEof()
    {
        // On start state, don't try to extract anything
        if ( wrap(m_State).get<S>().has_value() ){
            return std::nullopt;
        }

        // Extracting function
        auto lam = []<typename St>( const Table<St> table ){
            return [table]( const St& state ){
                    return table.m_ExtractToken( state );
            };
        };

        std::optional<token::Token> tk = visit_with_tables(lam, m_State);
        if ( tk.has_value() ) {
            return tk.value();
        }
        else {
            throw std::runtime_error( "Unexpected end of input." );
        }
    }

    /***********************************************/

    std::optional<token::Token> Lexer::next()
    {
        skip_ws();

        if ( m_Data.fail() ) {
            throw std::runtime_error( "Error while reading input." );
        }

        if ( m_Data.eof() ) {
            return handleEof();
        }

        char ch = m_Data.peek();

        // visiting function
        const auto lam = [ch, this] <typename St> ( const Table<St>& table )
        {
            return [ch, this, table] ( const St& state ) -> TransitionReturn
            {
                auto i = table.m_Map.find(ch);
                // found in map
                if ( i != table.m_Map.end() ){
                    this -> m_Data.get();
                    return i->second(state, ch);
                }
                // move to otherwise function, token extracted
                else if ( auto tok = table.m_ExtractToken( state ); tok.has_value() ) {
                    return tok.value();
                }
                // error
                else{
                    throw std::runtime_error( std::string {"Unexpected character "} + ch );
                }
            };
        };

        TransitionReturn ret = visit_with_tables(lam, m_State);

        // either new state or token and reset to state to S
        return wrap(ret).visit(
            [this]( const Token& tk) -> std::optional<token::Token>
            {
                this->m_State = start_state();
                return tk;
            },
            [this] ( const State& st ){
                this->m_State = st;
                return this->next();
            }
        );
    }
}
