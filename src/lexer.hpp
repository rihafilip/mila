#ifndef LEXER_HPP
#define LEXER_HPP

#include "tokens.hpp"
#include "lexer_table.hpp"
#include <istream>
#include <optional>

namespace lexer
{
    /// Simple struct saying where in file we are
    struct Position
    {
        size_t column, line;
    };

    /**
     * @brief Get a character from stream and transition with it on the given state
     *
     * @param state State to transition from
     * @param stream Stream to get characters from
     * @return std::optional<token::Token> Token or nothing on EOF
     */
    std::optional<token::Token> transition_all ( const State& state, Position& pos, std::istream& stream );

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

}

#endif // LEXER_HPP
