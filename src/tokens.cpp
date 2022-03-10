#include "tokens.hpp"
#include "overloaded.hpp"

#include <functional>

namespace token
{
    std::string to_string( const Token& tk )
    {
        return std::visit( overloaded{
            []( OPERATOR op )
            {
                return to_string( op );
            },
            []( CONTROL_SYMBOL sym )
            {
                return to_string( sym );
            },
            []( KEYWORD kw )
            {
                return to_string( kw );
            },
            []( Identifier id )
            {
                return "<identifier> (" + id.value + ")";
            },
            []( Integer i )
            {
                return "<integer> (" + std::to_string( i.value ) + ")";
            },
            []( Boolean b )
            {
                return "<boolean> (" + std::to_string( b.value ) + ")";
            },
            }, tk );
    }

    std::string to_string( OPERATOR op )
    {
        const std::map<OPERATOR, std::string> map =
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

        return "<" + map.at( op ) + ">";
    }

    std::string to_string( CONTROL_SYMBOL sym )
    {
        const std::map<CONTROL_SYMBOL, std::string> map =
        {
            {CONTROL_SYMBOL::SEMICOLON,             ";"},
            {CONTROL_SYMBOL::COLON,                 ":"},
            {CONTROL_SYMBOL::COMMA,                 ","},
            {CONTROL_SYMBOL::DOT,                   "."},
            {CONTROL_SYMBOL::BRACKET_OPEN,          "("},
            {CONTROL_SYMBOL::BRACKET_CLOSE,         ")"},
            {CONTROL_SYMBOL::SQUARE_BRACKET_OPEN,   "["},
            {CONTROL_SYMBOL::SQUARE_BRACKET_CLOSE,  "]"}
        };

        return "<" + map.at( sym ) + ">";
    }

    std::string to_string( KEYWORD kw )
    {
        const std::map<KEYWORD, std::string> map =
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

        return "<" + map.at(kw) + ">";
    }
}
