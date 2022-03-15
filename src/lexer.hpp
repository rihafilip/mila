#ifndef LEXER_HPP
#define LEXER_HPP

#include "tokens.hpp"
#include "stream_stack.hpp"
#include "concept.hpp"

#include <vector>

/**
 * @brief Class that parses input stream into tokens
 *
 */
class Lexer
{
private:
    /// Stack that we are processing
    StreamStack m_St;
    /// Line number we are currently on
    uint lineNum;

    Lexer( std::istream& str )
        : m_St( str )
        , lineNum( 1 )
    {}
    ~Lexer() = default;

    /**
     * @brief Skips whitespaces in input
     *
     */
    void skipWs();
    /**
     * @brief Throws a parse error, where the character on top is different
     * that the one expected
     *
     * @param expected
     * @param got
     */
    void parseError( const std::string& expected, char got );

    /// Parse decimal digit with first (highest) digit being the input
    inline token::Integer digit( int firstDigit );
    /// Parse a hexadecimal digit
    inline token::Integer hexDigit();
    /// Parse an octal digit
    inline token::Integer octalDigit();

    /**
     * @brief  Parse an alpha-numerical sequence of characters that can contain
     * underscores, detecting if it is a keyword in the process
     *
     * @param firstChar first character of the detected word
     * @return token::Token
     */
    inline token::Token word( char firstChar );

    /// Return a token that starts with the given character
    inline token::Token analyze( char ch );
    /// Scan the whole input stack
    std::vector<token::Token> scan();

    /**
     * @brief Extract a character from stack, tries to convert it using 'get' and
     * stores it in the accumulator. It does this until stack is empty or 'get'
     * returns an empty optional
     *
     * @tparam Acc accumulator type
     * @tparam Elem one element type
     * @param val initial value of accumulator
     * @param combine function that combines an element and the accumulator into
     * new value
     * @param get function that returns an Elem or empty optional on character
     * @return Ret
     */
    template <typename Acc, typename Elem>
    Acc loop( Acc val,
        const Returns<Acc, Acc, Elem> auto& combine,
        const Returns<std::optional<Elem>, char> auto& get )
    {
        std::optional<char> opt;
        while ( ( opt = m_St.top() ).has_value() )
        {
            std::optional<Elem> e = get( opt.value() );
            if ( !e.has_value() )
                return val;

            val = combine( val, e.value() );
            m_St.pop();
        }

        return val;
    }

public:
    /// Returns a tokenized representation of input stream
    static std::vector<token::Token> scan( std::istream& str );
};


#endif // LEXER_HPP
