#ifndef TOKENS_HPP
#define TOKENS_HPP

#include <variant>
#include <string>
#include <map>

namespace token
{
    enum class OPERATOR
    {
        EQUAL, NOT_EQUAL,
        LESS_EQUAL, LESS,
        MORE_EQUAL, MORE,
        PLUS, MINUS, TIMES, DIVIDE, MODULO,
        ASSIGNEMENT
    };

    const std::map<char, OPERATOR> SIMPLE_OPERATORS_MAP
    {
        {'=', OPERATOR::EQUAL},
        {'+', OPERATOR::PLUS},
        {'-', OPERATOR::MINUS},
        {'*', OPERATOR::TIMES},
        {'/', OPERATOR::DIVIDE},
        {'%', OPERATOR::MODULO}
    };

    enum class CONTROL_SYMBOL
    {
        SEMICOLON, COLON, COMMA, DOT,
        BRACKET_OPEN, BRACKET_CLOSE,
        SQUARE_BRACKET_OPEN, SQUARE_BRACKET_CLOSE
    };

    const std::map<char, CONTROL_SYMBOL> CONTROL_SYMBOLS_MAP
    {
        {';', CONTROL_SYMBOL::SEMICOLON},
        {':', CONTROL_SYMBOL::COLON},
        {',', CONTROL_SYMBOL::COMMA},
        {'.', CONTROL_SYMBOL::DOT},
        {'(', CONTROL_SYMBOL::BRACKET_OPEN},
        {')', CONTROL_SYMBOL::BRACKET_CLOSE},
        {'[', CONTROL_SYMBOL::SQUARE_BRACKET_OPEN},
        {']', CONTROL_SYMBOL::SQUARE_BRACKET_CLOSE}
    };

    enum class KEYWORD
    {
        PROGRAM, FORWARD,
        FUNCTION, PROCEDURE,
        CONST, VAR,
        BEGIN, END,
        WHILE, DO,
        FOR, TO, DOWNTO,
        IF, THEN, ELSE,
        ARRAY, OF,
        INTEGER, BOOLEAN,
        EXIT, BREAK,
        DIV, MOD,
        NOT, AND, OR, XOR
    };

    const std::map<std::string, KEYWORD> KEYWORDS_MAP =
    {
        {"program",     KEYWORD::PROGRAM},
        {"forward",     KEYWORD::FORWARD},
        {"function",    KEYWORD::FUNCTION},
        {"procedure",   KEYWORD::PROCEDURE},
        {"const",       KEYWORD::CONST},
        {"var",         KEYWORD::VAR},
        {"begin",       KEYWORD::BEGIN},
        {"end",         KEYWORD::END},
        {"while",       KEYWORD::WHILE},
        {"do",          KEYWORD::DO},
        {"for",         KEYWORD::FOR},
        {"to",          KEYWORD::TO},
        {"downto",      KEYWORD::DOWNTO},
        {"if",          KEYWORD::IF},
        {"then",        KEYWORD::THEN},
        {"else",        KEYWORD::ELSE},
        {"array",       KEYWORD::ARRAY},
        {"of",          KEYWORD::OF},
        {"integer",     KEYWORD::INTEGER},
        {"boolean",     KEYWORD::BOOLEAN},
        {"exit",        KEYWORD::EXIT},
        {"break",       KEYWORD::BREAK},
        {"div",         KEYWORD::DIV},
        {"mod",         KEYWORD::MOD},
        {"not",         KEYWORD::NOT},
        {"and",         KEYWORD::AND},
        {"or",          KEYWORD::OR},
        {"xor",         KEYWORD::XOR}
    };

    struct Identifier
    {
        std::string value;
    };

    struct Integer
    {
        long long value;
    };

    struct Boolean
    {
        bool value;
    };

    using Token = std::variant<OPERATOR, CONTROL_SYMBOL, KEYWORD, Identifier, Integer, Boolean>;

    std::string to_string ( const Token & tk );
    std::string to_string ( OPERATOR op );
    std::string to_string ( CONTROL_SYMBOL sym );
    std::string to_string ( KEYWORD kw );
}


#endif // TOKENS_HPP
