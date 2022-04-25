#ifndef LEXER_TABLE_HPP
#define LEXER_TABLE_HPP

#include "tokens.hpp"
#include <functional>
#include <optional>
#include <variant>

namespace lexer
{
    using namespace token;

    /***********************************************/
    /**
    * \defgroup LexSt States definition
    * @{
    */

    /// Integer helper superclass
    template<int base, typename Subclass>
    struct IntegerState {
        int value = 0;

        Subclass add ( int val ) const
        {
            return Subclass { (value * base) + val};
        }
    };

    struct S {};

    struct OctalStart {};
    struct HexStart {};

    struct Decimal : public IntegerState<10, Decimal> {};
    struct Octal : public IntegerState<8, Octal> {};
    struct Hex : public IntegerState<16, Hex> {};

    struct Word  {
        std::string value {};
    };

    struct GreaterThan {};
    struct LowerThan {};
    struct Colon {};
    // TODO dot

    using State = std::variant<S, Decimal, OctalStart, Octal, HexStart, Hex, Word, GreaterThan, LowerThan, Colon>;

    /// Create a start state
    constexpr S start_state()
    {
        return S{};
    }

    /// @}

    /***********************************************/
    /// Return on state transition
    using TransitionReturn = std::variant<Token, State>;

    /// What to do on state
    template <typename St>
    using Transition = std::function<TransitionReturn(St, char)>;

    /**
     * Return nothing if extraction fails, otherwise returns token
     */
    template <typename St>
    using ExtractToken = std::function<std::optional<Token>(St)>;

    /// Single table definition
    template <typename St>
    struct Table
    {
        std::map<char, Transition<St>> m_Map {};
        ExtractToken<St> m_ExtractToken;
    };

    /***********************************************/

    /// Return a new state on transition
    template <typename OutSt, typename InSt>
    TransitionReturn state_factory ( const InSt &, char ){
        return OutSt{};
    }

    /// Return a new operator token on state transition
    template <token::OPERATOR op, typename St>
    TransitionReturn operator_factory ( const St&, char ){
        return op;
    }

    template <token::CONTROL_SYMBOL cs, typename St>
    TransitionReturn control_symbol_factory( const St&, char){
        return cs;
    }

    /***********************************************/

    /// Fail on extracting token
    template <typename St>
    std::optional<Token> fail_factory( const St& )
    {
        return std::nullopt;
    }

    /// Return computed integer from IntegerState
    template <typename St>
    Token extract_integer ( const St& i ){
        return token::Integer { i.value };
    }

    /***********************************************/

    /// Range of characters, if one character is given, the range is that single char
    struct TableRange {
        char low;
        char high;

        TableRange ( char l, char h )
        : low (l)
        , high (h)
        {}

        TableRange ( char x )
        : low (x)
        , high (x)
        {}
    };

    /// Table creation helper function
    template <typename St>
    Table<St> make_table(
        std::initializer_list<std::pair<TableRange, Transition<St>>> init,
        const ExtractToken<St>& extr
    ) {
        Table<St> table{ {}, extr };

        for (const auto &[range, eff] : init) {
            const auto &[low, high] = range;

            for (char i = low; i <= high; ++i) {
                table.m_Map.insert({i, eff});
            }
        }

        return table;
    }

    /***********************************************/

    /// Make a word state with the initial character
    inline TransitionReturn make_word ( const S&, char ch ){
        return Word { { ch } };
    }

    /**
     * \defgroup LexTables Tables declaration
     * @{
     */

    const Table<S> S_TABLE = make_table<S>({
        { {'&'}, state_factory<OctalStart, S> },
        { {'$'}, state_factory<HexStart, S> },
        { {'<'}, state_factory<LowerThan, S>},
        { {'>'}, state_factory<GreaterThan, S> },
        { {':'}, state_factory<Colon, S>},
        { {'0', '9'}, [] ( const S&, char ch ){
            return Decimal { ch - '0' };
        }},
        { {'a', 'z'}, make_word },
        { {'A', 'Z'}, make_word },
        { {'_'}, make_word },
        { {'='}, operator_factory<OPERATOR::EQUAL, S> },
        { {'-'}, operator_factory<OPERATOR::MINUS, S> },
        { {'*'}, operator_factory<OPERATOR::TIMES, S> },
        { {'/'}, operator_factory<OPERATOR::DIVIDE, S> },
        { {'%'}, operator_factory<OPERATOR::MODULO, S> },
        { {';'}, control_symbol_factory<CONTROL_SYMBOL::SEMICOLON, S>},
        { {':'}, control_symbol_factory<CONTROL_SYMBOL::COLON, S>},
        { {','}, control_symbol_factory<CONTROL_SYMBOL::COMMA, S>},
        { {'.'}, control_symbol_factory<CONTROL_SYMBOL::DOT, S>},
        { {'('}, control_symbol_factory<CONTROL_SYMBOL::BRACKET_OPEN, S>},
        { {')'}, control_symbol_factory<CONTROL_SYMBOL::BRACKET_CLOSE, S>},
        { {'['}, control_symbol_factory<CONTROL_SYMBOL::SQUARE_BRACKET_OPEN, S>},
        { {']'}, control_symbol_factory<CONTROL_SYMBOL::SQUARE_BRACKET_CLOSE, S>}
    }, fail_factory<S>);

    /***********************************************/

    const Table<Decimal> DECIMAL_TABLE = make_table<Decimal>({
        { {'0', '9'}, []( const Decimal & st, char c ){
            return st.add( c - '0');
        }}
    }, extract_integer<Decimal>);

    /***********************************************/

    const Table<OctalStart> OCTAL_START_TABLE = make_table<OctalStart>({
        {{'0', '7'}, []( const OctalStart & st, char c ){
            return Octal { c - '0' };
        }}
    }, fail_factory<OctalStart>);

    const Table<Octal> OCTAL_TABLE = make_table<Octal>({
        {{'0', '7'}, []( const Octal & st, char c){
            return st.add( c - '0' );
        }}
    }, extract_integer<Octal>);

    /***********************************************/

    const Table<HexStart> HEX_START_TABLE = make_table<HexStart>({
        { {'0', '9'}, []( const HexStart & st, char c){
            return Hex { c - '0' };
        }},
        { {'a', 'f'}, []( const HexStart & st, char c){
            return Hex { c - 'a' + 10 };
        }},
        { {'A', 'F'}, []( const HexStart & st, char c){
            return Hex { c - 'A' + 10 };
        }}
    }, fail_factory<HexStart>);

    const Table<Hex> HEX_TABLE = make_table<Hex>({
        { {'0', '9'}, []( const Hex & st, char c){
            return st.add( c - '0' );
        }},
        { {'a', 'f'}, []( const Hex & st, char c){
            return st.add( c - 'a' + 10 );
        }},
        { {'A', 'F'}, []( const Hex & st, char c){
            return st.add( c - 'A' + 10 );
        }}
    }, extract_integer<Hex>);

    /***********************************************/

    /// @}

    /// Add a letter to a word state
    inline TransitionReturn add_letter ( const Word& state, char ch ){
        return Word{ state.value + ch };
    }

    /**
     * \addtogroup LexTables
     * @{
     */
    const Table<Word> WORD_TABLE = make_table<Word>({
        { {'0', '9'}, add_letter },
        { {'a', 'z'}, add_letter },
        { {'A', 'Z'}, add_letter },
        { {'_'}, add_letter }
    }, [] ( const Word& state ) -> std::optional<Token>
    {
        auto kw = KEYWORD_MAP.byValueSafe( state.value );
        if ( kw.has_value() ){
            return kw.value();
        }
        else{
            return Identifier{ state.value };
        }
    });

    /***********************************************/

    const Table<LowerThan> LOWER_THAN_TABLE = make_table<LowerThan>({
        { {'='}, operator_factory<OPERATOR::LESS_EQUAL, LowerThan> },
        { {'>'}, operator_factory<OPERATOR::NOT_EQUAL, LowerThan> },
    }, [] ( const LowerThan& ){
        return OPERATOR::LESS;
    });

    /***********************************************/

    const Table<GreaterThan> GREATER_THAN_TABLE = make_table<GreaterThan>({
        { {'='}, operator_factory<OPERATOR::MORE_EQUAL, GreaterThan> }
    },[] ( const GreaterThan& ){
        return OPERATOR::MORE;
    });

    /***********************************************/

    const Table<Colon> COLON_TABLE = make_table<Colon>({
        { {'='}, operator_factory<OPERATOR::ASSIGNEMENT, Colon>}
    },[] ( const Colon& ){
        return CONTROL_SYMBOL::COLON;
    });

    /// @}
}

#endif // LEXER_TABLE_HPP
