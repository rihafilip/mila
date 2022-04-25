#ifndef LEXER_HPP
#define LEXER_HPP

#include "tokens.hpp"
#include "lexer_table.hpp"
#include <istream>
#include <optional>

namespace lexer
{
    class Lexer
    {
    private:
        std::istream& m_Data;
        State m_State = start_state(); // start symbol

        void skip_ws();
        std::optional<token::Token> handleEof();

    public:
        Lexer( std::istream& str )
        : m_Data( str )
        {}

        ~Lexer() = default;

        /// Return the next scanned token, or nothing on EOF
        std::optional<token::Token> next();
    };

}


#endif // LEXER_HPP
