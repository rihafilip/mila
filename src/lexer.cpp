#include "lexer.hpp"

#include <optional>
#include <stdexcept>
#include <cctype>

using namespace token;

namespace
{
    std::optional<int> digitValue( char ch )
    {
        if ( ch >= '0' && ch <= '9' )
            return ch - '0';

        return std::nullopt;
    }

    std::optional<int> hexDigitValue( char ch )
    {
        if ( ch >= '0' && ch <= '9' )
            return ch - '0';

        else if ( ch >= 'a' && ch <= 'f' )
            return ( ch - 'a' ) + 10;

        else if ( ch >= 'A' && ch <= 'F' )
            return ( ch - 'A' ) + 10;

        return std::nullopt;
    }

    std::optional<int> octDigitValue( char ch )
    {
        if ( ch >= '0' && ch <= '7' )
            return ch - '0';

        return std::nullopt;
    }

    constexpr auto combineInteger( int base )
    {
        return [base]( Integer i, int e )
        {
            i.value *= base;
            i.value += e;
            return i;
        };
    }
} // anonymous namespace

void Lexer::skipWs()
{
    std::optional<char> ch;
    while ( ( ch = m_St.top() ).has_value()
        && std::isspace( ch.value() ) )
    {
        m_St.pop();
        if ( ch.value() == '\n' )
            lineNum++;
    }
}

void Lexer::parseError( const std::string& expected, char got )
{
    throw std::runtime_error(
        "Parsing error on line " + std::to_string( lineNum )
        + ": Expected " + expected
        + ", got " + got );
}

inline Integer Lexer::digit( int firstDigit )
{
    return loop<Integer, int>(
        Integer{ firstDigit },
        combineInteger( 10 ),
        digitValue );
}

inline Integer Lexer::hexDigit()
{
    char ch = m_St.topForce();
    auto dig = hexDigitValue( ch );
    m_St.pop();

    if ( !dig.has_value() )
        parseError( "hexadecimal digit", ch );

    return loop<Integer, int>(
        Integer{ dig.value() },
        combineInteger( 16 ),
        hexDigitValue );
}

inline Integer Lexer::octalDigit()
{
    char ch = m_St.topForce();
    auto dig = octDigitValue( ch );
    m_St.pop();

    if ( !dig.has_value() )
        parseError( "octal digit", ch );

    return loop<Integer, int>(
        Integer{ dig.value() },
        combineInteger( 8 ),
        octDigitValue );
}

inline Token Lexer::word( char firstChar )
{
    constexpr auto comb = []( const std::string& str, char ch )
    {
        return str + ch;
    };

    constexpr auto get = []( char ch ) -> std::optional<char>
    {
        if ( ( ch >= 'a' && ch <= 'z' )
            || ( ch >= 'A' && ch <= 'Z' )
            || ( ch >= '0' && ch <= '9' )
            || ( ch == '_' ) )
            return ch;

        return std::nullopt;
    };

    std::string word = loop<std::string, char>(
        std::string{ firstChar }, comb, get );

    auto kw = KEYWORD_MAP.byValueSafe( word );
    if ( kw.has_value() )
        return kw.value();

    return Identifier{ word };
}

inline Token Lexer::analyze( char ch )
{
    auto dig = digitValue( ch );
    if ( dig.has_value() )
        return digit( dig.value() );

    if ( ( ch >= 'a' && ch <= 'z' ) || ( ch >= 'A' && ch <= 'Z' ) )
        return word( ch );

    switch ( ch )
    {
    case '&':
        return octalDigit();

    case '$':
        return hexDigit();

    case '<':
        if ( m_St.popIf( '>' ) )
            return OPERATOR::NOT_EQUAL;
        else if ( m_St.popIf( '=' ) )
            return OPERATOR::LESS_EQUAL;
        else
            return OPERATOR::LESS;

    case '>':
        if ( m_St.popIf( '=' ) )
            return OPERATOR::MORE_EQUAL;
        else
            return OPERATOR::MORE;

    case ':':
        if ( m_St.popIf( '=' ) )
            return OPERATOR::ASSIGNEMENT;
        else
            return CONTROL_SYMBOL::COLON;
    }

    auto oper = SIMPLE_OPERATOR_MAP.find( ch );
    if ( oper != SIMPLE_OPERATOR_MAP.end() )
        return oper->second;

    auto sym = CONTROL_SYMBOL_MAP.byValueSafe( ch );
    if ( sym.has_value() )
        return sym.value();

    throw std::runtime_error( std::string( "Unrecognized character " ) + ch );
}

std::vector<Token> Lexer::scan()
{
    std::vector<Token> acc{};
    std::optional<char> optCh;

    skipWs();

    while ( ( optCh = m_St.top() ).has_value() )
    {
        m_St.pop();
        acc.push_back( analyze( optCh.value() ) );
        skipWs();
    }

    return acc;
}

std::vector<Token> Lexer::scan( std::istream& str )
{
    return Lexer( str ).scan();
}
