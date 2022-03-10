#ifndef LEXER_HPP
#define LEXER_HPP

#include "tokens.hpp"
#include "stream_stack.hpp"
#include "concept.hpp"

#include <vector>

class Lexer
{
private:
    StreamStack m_St;
    uint lineNum;

    Lexer( std::istream& str )
        : m_St( str )
        , lineNum( 0 )
    {}
    ~Lexer() = default;

    void skipWs();
    void parseError( const std::string& expected, char got );

    inline token::Integer digit( int firstDigit );
    inline token::Integer hexDigit();
    inline token::Integer octalDigit();

    inline token::Token word( char firstChar );

    inline token::Token analyze( char ch );
    std::vector<token::Token> scan();

    template <typename Ret, typename Elem>
    Ret loop( Ret initial,
        const Returns<Ret, Ret, Elem> auto& combine,
        const Returns<std::optional<Elem>, char> auto& get )
    {
        Ret output = initial;
        std::optional<char> opt;
        while ( ( opt = m_St.top() ).has_value() )
        {
            std::optional<Elem> e = get( opt.value() );
            if ( !e.has_value() )
                return output;

            output = combine( output, e.value() );
            m_St.pop();
        }

        return output;
    }

public:
    static std::vector<token::Token> scan( std::istream& str );
};


#endif // LEXER_HPP
