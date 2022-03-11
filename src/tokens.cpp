#include "tokens.hpp"
#include "overloaded.hpp"

namespace token
{
    std::string to_string( const Token& tk )
    {
        return std::visit( overloaded{
            []( OPERATOR op ) -> std::string
            {
                return "<'" + OPERATOR_MAP.byKey( op ) + "'>";
            },
            []( CONTROL_SYMBOL sym ) -> std::string
            {
                return "<'" + std::string {CONTROL_SYMBOL_MAP.byKey( sym )} + "'>";
            },
            []( KEYWORD kw ) -> std::string
            {
                return "<" + KEYWORD_MAP.byKey( kw ) + ">";
            },
            []( Identifier id ) -> std::string
            {
                return "<identifier> (" + id.value + ")";
            },
            []( Integer i ) -> std::string
            {
                return "<integer> (" + std::to_string( i.value ) + ")";
            },
            []( Boolean b ) -> std::string
            {
                return "<boolean> (" + std::to_string( b.value ) + ")";
            },
            }, tk );
    }
}
