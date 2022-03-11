#ifndef TOKENS_HPP
#define TOKENS_HPP

#include "bimap.hpp"

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

    const cont::Bimap<OPERATOR, std::string> OPERATOR_MAP
    {
        {OPERATOR::EQUAL,       "="},
        {OPERATOR::NOT_EQUAL,   "<>"},
        {OPERATOR::LESS_EQUAL,  "<="},
        {OPERATOR::LESS,        "<"},
        {OPERATOR::MORE_EQUAL,  ">="},
        {OPERATOR::MORE,        ">"},
        {OPERATOR::PLUS,        "+"},
        {OPERATOR::MINUS,       "-"},
        {OPERATOR::TIMES,       "*"},
        {OPERATOR::DIVIDE,      "/"},
        {OPERATOR::MODULO,      "%"},
        {OPERATOR::ASSIGNEMENT, ":="}
    };

    const std::map<char, OPERATOR> SIMPLE_OPERATOR_MAP
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

    const cont::Bimap<CONTROL_SYMBOL, char> CONTROL_SYMBOL_MAP
    {
        {CONTROL_SYMBOL::SEMICOLON,             ';'},
        {CONTROL_SYMBOL::COLON,                 ':'},
        {CONTROL_SYMBOL::COMMA,                 ','},
        {CONTROL_SYMBOL::DOT,                   '.'},
        {CONTROL_SYMBOL::BRACKET_OPEN,          '('},
        {CONTROL_SYMBOL::BRACKET_CLOSE,         ')'},
        {CONTROL_SYMBOL::SQUARE_BRACKET_OPEN,   '['},
        {CONTROL_SYMBOL::SQUARE_BRACKET_CLOSE,  ']'}
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

    const cont::Bimap<KEYWORD, std::string> KEYWORD_MAP
    {
        {KEYWORD::PROGRAM,      "program"},
        {KEYWORD::FORWARD,      "forward"},
        {KEYWORD::FUNCTION,     "function"},
        {KEYWORD::PROCEDURE,    "procedure"},
        {KEYWORD::CONST,        "const"},
        {KEYWORD::VAR,          "var"},
        {KEYWORD::BEGIN,        "begin"},
        {KEYWORD::END,          "end"},
        {KEYWORD::WHILE,        "while"},
        {KEYWORD::DO,           "do"},
        {KEYWORD::FOR,          "for"},
        {KEYWORD::TO,           "to"},
        {KEYWORD::DOWNTO,       "downto"},
        {KEYWORD::IF,           "if"},
        {KEYWORD::THEN,         "then"},
        {KEYWORD::ELSE,         "else"},
        {KEYWORD::ARRAY,        "array"},
        {KEYWORD::OF,           "of"},
        {KEYWORD::INTEGER,      "integer"},
        {KEYWORD::BOOLEAN,      "boolean"},
        {KEYWORD::EXIT,         "exit"},
        {KEYWORD::BREAK,        "break"},
        {KEYWORD::DIV,          "div"},
        {KEYWORD::MOD,          "mod"},
        {KEYWORD::NOT,          "not"},
        {KEYWORD::AND,          "and"},
        {KEYWORD::OR,           "or"},
        {KEYWORD::XOR,          "xor"}
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

    std::string to_string( const Token& tk );
}


#endif // TOKENS_HPP
