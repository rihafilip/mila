#include "lexer.hpp"

#include "tokens.hpp"
#include "concept.hpp"
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

using namespace token;

/***********************************************/
// Error function
void parseError( const std::string& expected, char got )
{
    int lineNum = 10;
    throw std::runtime_error(
        "Parsing error on line " + std::to_string( lineNum )
        + ": Expected " + expected
        + ", got " + got );
}

/***********************************************/
// States definition

// Integer helper superclass
template<int base, typename Subclass>
struct IntegerState {
    int value = 0;

    Subclass add ( int val ) const
    {
        return Subclass { (value * base) + val};
    }
};

struct S {};
struct Decimal : public IntegerState<10, Decimal> {};
struct Octal : public IntegerState<8, Octal> {};
struct Hex : public IntegerState<16, Hex> {};

struct Word {
    std::string value {};
};

struct GreaterThan {};
struct LowerThan {};
struct Colon {};

using State = std::variant<S, Decimal, Octal, Hex, Word, GreaterThan, LowerThan, Colon>;

/***********************************************/
// Table definiton
using EffectReturn = std::variant<token::Token, State>;

template <typename St>
using Effect = std::function<EffectReturn(St, char)>;

// On non-found character or whitespace, activate this
template <typename St>
using EffectOnEnd = std::function<EffectReturn(St)>;

template <typename St>
struct Table
{
    std::map<char, Effect<St>> m_Map {};
    EffectOnEnd<St> m_Otherwise;
};

/***********************************************/
// Scanning functions

EffectReturn make_word ( const S&, char ch){
    return Word{ { ch } };
}

EffectReturn add_letter ( const Word& state, char ch ){
    return Word{ state.value + ch };
}

template <typename OutSt, typename InSt>
EffectReturn state_factory ( const InSt &, char ){
    return OutSt{};
}

template <token::OPERATOR op, typename St>
EffectReturn operator_factory ( const St&, char ){
    return op;
}

// template <token::CONTROL_SYMBOL cs, typename St>
// EffectReturn control_symbol_factory ( const St&, char ){
//     return cs;
// }

template <typename St>
EffectReturn get_integer ( const St& i ){
    return token::Integer { i.value };
}

/***********************************************/
// Table creation helper function

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

template <typename St>
Table<St> make_table(std::initializer_list<std::pair<TableRange, Effect<St>>> init, EffectOnEnd<St>) {
    Table<St> table{};
    for (const auto &[range, eff] : init) {
        const auto &[low, high] = range;
        for (char i = low; low <= high; ++i) {
            table.m_Map.insert({i, eff});
        }
    }

    return table;
}

/***********************************************/
// Tables declaration

const Table<S> S_TABLE = make_table<S>({
    { {'&'}, state_factory<Octal, S> },
    { {'$'}, state_factory<Hex, S> },
    { {'<'}, state_factory<LowerThan, S>},
    { {'>'}, state_factory<GreaterThan, S> },
    { {':'}, state_factory<Colon, S>},
    { {'0', '9'}, make_word },
    { {'a', 'z'}, make_word },
    { {'A', 'Z'}, make_word },
    { {'_'}, make_word },
    { {'='}, operator_factory<OPERATOR::EQUAL, S> },
    { {'-'}, operator_factory<OPERATOR::MINUS, S> },
    { {'*'}, operator_factory<OPERATOR::TIMES, S> },
    { {'/'}, operator_factory<OPERATOR::DIVIDE, S> },
    { {'%'}, operator_factory<OPERATOR::MODULO, S> }
},
[] ( const S& st ) {
    return st;
});

const Table<Decimal> DECIMAL_TABLE = make_table<Decimal>({
    { {'0', '9'}, []( const Decimal & st, char c ){
        return st.add( c - '0');
    }}
}, get_integer<Decimal>);

const Table<Octal> OCTAL_TABLE = make_table<Octal>({
    {{'0', '7'}, []( const Octal & st, char c){
        return st.add( c - '0' );
    }}
}, get_integer<Octal>);

const Table<Hex> HEX_TABLE = make_table<Hex>({
    { {'0', '9'}, []( const Hex & st, char c){
        return st.add( c - '0' );
    }},
    { {'a', 'f'}, []( const Hex & st, char c){
        return st.add(c - 'a' + 10);
    }},
    { {'A', 'F'}, []( const Hex & st, char c){
        return st.add(c - 'A' + 10);
    }}
}, get_integer<Hex>);

const Table<Word> WORD_TABLE = make_table<Word>({
    { {'0', '9'}, add_letter },
    { {'a', 'z'}, add_letter },
    { {'A', 'Z'}, add_letter },
    { {'_'}, add_letter }
}, [] ( const Word& state ) -> EffectReturn {
    auto kw = KEYWORD_MAP.byValueSafe( state.value );
    if ( kw.has_value() )
        return kw.value();

    return Identifier{ state.value };
});

const Table<LowerThan> LOWER_THAN_TABLE = make_table<LowerThan>({
    { {'='}, operator_factory<OPERATOR::LESS_EQUAL, LowerThan> },
    { {'>'}, operator_factory<OPERATOR::NOT_EQUAL, LowerThan> },
}, [] ( const LowerThan& ){
    return OPERATOR::LESS;
});

const Table<GreaterThan> GREATER_THAN_TABLE = make_table<GreaterThan>({
    { {'='}, operator_factory<OPERATOR::MORE_EQUAL, GreaterThan> }
},[] ( const GreaterThan& ){
    return OPERATOR::MORE;
});

const Table<Colon> COLON_TABLE = make_table<Colon>({
    { {'='}, operator_factory<OPERATOR::ASSIGNEMENT, Colon>}
},[] ( const Colon& ){
    return CONTROL_SYMBOL::COLON;
});
