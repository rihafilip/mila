#ifndef LEXER_HPP
#define LEXER_HPP

#include "tokens.hpp"
#include "lexer_table.hpp"
#include <istream>
#include <optional>

namespace lexer
{
    /// \defgroup Lex Lexer
    /// @{

    /// Simple struct saying where in file we are
    struct Position
    {
        size_t column, line;
    };

    /// Lexer on given input stream
    class Lexer
    {
    private:
        std::istream& m_Data;
        Position m_Position {0, 1};

    public:
        Lexer( std::istream& str )
        : m_Data( str )
        {}

        ~Lexer() = default;

        /// Return the next scanned token, or nothing on EOF
        std::optional<token::Token> next();

        /// Return current line and column number number
        Position position() const;
    };
    /// @}
}

#endif // LEXER_HPP
