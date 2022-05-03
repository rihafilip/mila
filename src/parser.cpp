#include "parser.hpp"
#include "ast.hpp"
#include "tokens.hpp"

#include <functional>
#include <string>

using namespace ast;
using namespace token;

/*********************************************************************/
// Helper functions

template <typename T1, typename T2>
void append( std::vector<T1>& dest, const std::vector<T2>& from )
{
    dest.reserve( from.size() );
    dest.insert( dest.end(), from.begin(), from.end() );
}

/// Transform all idetifiers to variables with given type
Many<Variable> identifiers_to_variables( const Many<ast::Identifier>& ids, Type t )
{
    Many<Variable> out{};
    out.reserve( ids.size() );
    std::ranges::transform( ids, std::back_inserter( out ),
        [&t]( const ast::Identifier& i ) -> Variable
        {
            return { i, t };
        }
    );

    return out;
}

/*********************************************************************/

const cont::Bimap<token::OPERATOR, ast::BinaryOperator::OPERATOR> OPERATORS_MAP
{
    {OPERATOR::EQUAL,       ast::BinaryOperator::OPERATOR::EQ},
    {OPERATOR::NOT_EQUAL,   ast::BinaryOperator::OPERATOR::NOT_EQ},
    {OPERATOR::LESS_EQUAL,  ast::BinaryOperator::OPERATOR::LESS_EQ},
    {OPERATOR::LESS,        ast::BinaryOperator::OPERATOR::LESS},
    {OPERATOR::MORE_EQUAL,  ast::BinaryOperator::OPERATOR::MORE_EQ},
    {OPERATOR::MORE,        ast::BinaryOperator::OPERATOR::MORE},
    {OPERATOR::PLUS,        ast::BinaryOperator::OPERATOR::PLUS},
    {OPERATOR::MINUS,       ast::BinaryOperator::OPERATOR::MINUS},
    {OPERATOR::STAR,        ast::BinaryOperator::OPERATOR::TIMES},
    {OPERATOR::SLASH,       ast::BinaryOperator::OPERATOR::DIVISION},
};

ast::BinaryOperator::OPERATOR token_to_ast_operator ( token::OPERATOR op )
{
    return OPERATORS_MAP.byKey(op);
}

/*********************************************************************/

namespace parser
{
    Program Parser::parse( std::istream& str )
    {
        return Parser(str).program();
    }

    std::optional<token::Token> stack_wrap_adaptor ( lexer::Lexer& lex )
    {
        return lex.next();
    }

    std::optional<WrappedToken> Parser::lookup()
    {
        auto el = m_Data.top();

        if ( el.has_value() ){
            return wrap( el.value() );
        } else {
            return std::nullopt;
        }
    }

    /*********************************************************************/

    WrappedToken Parser::next_token()
    {
        auto el = m_Data.pop();

        if ( el.has_value() ){
            return wrap( el.value() );
        } else {
            throw std::runtime_error( "Parser error: Unexpected EOF" );
        }
    }

    void Parser::match( token::OPERATOR op )
    {
        auto tk = next_token();
        if ( tk.is_eq( op ) )
            return;

        fail( token::to_string( op ), tk.variant );
    }

    void Parser::match( token::CONTROL_SYMBOL cs )
    {
        auto tk = next_token();
        if ( tk.is_eq( cs ) )
            return;

        fail( token::to_string( cs ), tk.variant );
    }

    void Parser::match( token::KEYWORD kw )
    {
        auto tk = next_token();
        if ( tk.is_eq( kw ) )
            return;

        fail( token::to_string( kw ), tk.variant );
    }

    /*********************************************************************/

    [[noreturn]] void Parser::fail( const std::string& expected, const Token& got )
    {
        const auto&[ column, line ] = m_Data.get_data().position();

        throw std::runtime_error(
            "Parser error: Expected "
            + expected
            + ", but instead got "
            + token::to_string( got )
            + " (line "
            + std::to_string( line )
            + ", column "
            + std::to_string( column )
            + ")"
        );
    }

    /*********************************************************************/

    token::OPERATOR Parser::match_operator()
    {
        auto t = next_token();
        auto v = t.get<token::OPERATOR>();
        if ( v.has_value() )
            return v.value();

        fail( "operator", t.variant );
    }

    ast::Identifier Parser::match_identifier()
    {
        auto t = next_token();
        auto v = t.get<token::Identifier>();
        if ( v.has_value() )
            return v->value;

        fail( "identifier", t.variant );
    }

    Constant Parser::match_constant()
    {
        return next_token().visit(
            []( token::Integer i ) -> Constant
            {
                return IntegerConstant{ i.value };
            },

            []( token::Boolean b ) -> Constant
            {
                return BooleanConstant{ b.value };
            },

            [this]( auto t ) -> Constant
            {
                fail( "constant", t );
                return IntegerConstant{0}; // dummy compiler check
            }
        );
    }

    /*********************************************************************/

    Program Parser::program()
    {
        match( KEYWORD::PROGRAM );
        auto name = match_identifier();
        match( CONTROL_SYMBOL::SEMICOLON );
        auto globs = globals();
        auto code = block();
        match( CONTROL_SYMBOL::DOT );

        auto t = lookup();
        if ( t.has_value() )
            fail( "EOF", t->variant );

        return Program{ name, globs, code };
    }

    /*********************************************************************/

    Many<Global> Parser::globals()
    {
        Many<Global> globals {};

        while ( true )
        {
            if ( lookup_eq( KEYWORD::CONST ) )
            {
                append( globals, constants() );
            }
            else if ( lookup_eq( KEYWORD::VAR ) )
            {
                append( globals, variables() );
            }
            else if ( lookup_eq( KEYWORD::FUNCTION ) )
            {
                wrap(function()).visit(
                    [&globals]( const auto& f ){
                        globals.push_back(f);
                    }
                );
            }
            else if ( lookup_eq( KEYWORD::PROCEDURE ) )
            {
                wrap(function()).visit(
                    [&globals]( const auto& p ){
                        globals.push_back(p);
                    }
                );
            }
            else
            {
                break;
            }
        }

        return globals;
    }

    /*********************************************************************/

    Many<NamedConstant> Parser::constants()
    {
        match( KEYWORD::CONST );

        Many<NamedConstant> acc{ single_constant() };

        // MoreConstants
        while ( lookup_type<token::Identifier>() )
        {
            acc.push_back( single_constant() );
        }
        return acc;
    }

    NamedConstant Parser::single_constant()
    {
        auto id = match_identifier();
        match( OPERATOR::EQUAL );
        auto ex = expr();
        match( CONTROL_SYMBOL::SEMICOLON );
        return NamedConstant{ id, ex };
    }

    /*********************************************************************/

    Many<Variable> Parser::variables()
    {
        match( KEYWORD::VAR );

        Many<Variable> acc{ single_variable() };

        /// MoreVariables
        while ( lookup_type<token::Identifier>() )
        {
            append( acc, single_variable() );
        }
        return acc;
    }


    Many<Variable> Parser::single_variable()
    {
        auto ids = identifier_list();
        match( CONTROL_SYMBOL::COLON );
        auto t = type();
        match( CONTROL_SYMBOL::SEMICOLON );

        return identifiers_to_variables( ids, t );
    }

    /*********************************************************************/

    Many<ast::Identifier> Parser::identifier_list()
    {
        Many<ast::Identifier> acc{ match_identifier() };

        // MoreIdentifierList
        while ( lookup_eq( CONTROL_SYMBOL::COMMA ) )
        {
            match( CONTROL_SYMBOL::COMMA );
            acc.push_back( match_identifier() );
        }

        return acc;
    }

    /*********************************************************************/

    std::variant<ProcedureDecl, Procedure> Parser::procedure()
    {
        match( KEYWORD::PROCEDURE );
        auto n = match_identifier();
        auto ps = parameters();
        match( CONTROL_SYMBOL::SEMICOLON );

        auto b = body();

        if ( b.has_value() )
        {
            return Procedure{ n, ps, b->first, b->second };
        }
        else
        {
            return ProcedureDecl{ n, ps };
        }
    }

    std::variant<FunctionDecl, Function> Parser::function()
    {
        match( KEYWORD::FUNCTION );
        auto n = match_identifier();
        auto ps = parameters();

        match( CONTROL_SYMBOL::COLON );
        auto t = type();
        match( CONTROL_SYMBOL::SEMICOLON );

        auto b = body();

        if ( b.has_value() )
        {
            return Function{ n, ps, t, b->first, b->second };
        }
        else
        {
            return FunctionDecl{ n, ps, t };
        }
    }

    /*********************************************************************/

    Many<Variable> Parser::parameters()
    {
        if ( !lookup_eq( CONTROL_SYMBOL::BRACKET_OPEN ) )
        {
            return {};
        }

        match( CONTROL_SYMBOL::BRACKET_OPEN );

        // ()
        if ( lookup_eq( CONTROL_SYMBOL::BRACKET_CLOSE ) )
        {
            match (CONTROL_SYMBOL::BRACKET_CLOSE );
            return {};
        }

        // At least one parameter
        Many<Variable> acc{ single_parameter() };

        // MoreParameters
        while ( lookup_eq( CONTROL_SYMBOL::SEMICOLON ) )
        {
            match( CONTROL_SYMBOL::SEMICOLON );
            append( acc, single_parameter() );
        }

        match( CONTROL_SYMBOL::BRACKET_CLOSE );
        return acc;
    }

    Many<Variable> Parser::single_parameter()
    {
        auto ids = identifier_list();
        match( CONTROL_SYMBOL::COLON );
        auto t = type();

        return identifiers_to_variables( ids, t );
    }

    /*********************************************************************/

    std::optional<std::pair<Many<Variable>, Block>> Parser::body()
    {
        if ( lookup_eq( KEYWORD::FORWARD ) )
        {
            match( KEYWORD::FORWARD );
            match( CONTROL_SYMBOL::SEMICOLON );
            return std::nullopt;
        }

        auto vars = many_variables();
        auto b = block();
        match( CONTROL_SYMBOL::SEMICOLON );
        return { { vars, b } };
    }

    Many<Variable> Parser::many_variables ()
    {
        Many<Variable> vars {};

        while ( lookup_eq( KEYWORD::VAR ) ){
            append( vars, variables() );
        }

        return vars;
    }


    /*********************************************************************/

    Block Parser::block()
    {
        match( KEYWORD::BEGIN );

        // Stats
        Many<Statement> acc{ stat() };
        while ( lookup_eq( CONTROL_SYMBOL::SEMICOLON ) )
        {
            match( CONTROL_SYMBOL::SEMICOLON );
            acc.push_back( stat() );
        }

        match( KEYWORD::END );

        return Block{ acc };
    }

    Statement Parser::stat()
    {
        if ( lookup_type<token::Identifier>() )
            return stat_id();

        if ( lookup_eq( KEYWORD::BEGIN ) )
            return make_ptr<Block>( block() );

        if ( lookup_eq( KEYWORD::IF ) )
            return make_ptr<If>( if_p() );

        if ( lookup_eq( KEYWORD::WHILE ) )
            return make_ptr<While>( while_p() );

        if ( lookup_eq( KEYWORD::FOR ) )
            return make_ptr<For>( for_p() );

        if ( lookup_eq( KEYWORD::EXIT ) )
        {
            match( KEYWORD::EXIT );
            return ExitStatement{};
        }

        return EmptyStatement{};
    }

    Statement Parser::stat_id()
    {
        auto id = match_identifier();
        if ( lookup_eq( OPERATOR::ASSIGNEMENT ) )
        {
            match( OPERATOR::ASSIGNEMENT );
            auto ex = expr();
            return Assignment{ id, ex };
        }

        else if ( lookup_eq( CONTROL_SYMBOL::SQUARE_BRACKET_OPEN ) )
        {
            match( CONTROL_SYMBOL::SQUARE_BRACKET_OPEN );
            auto pos = expr();
            match( CONTROL_SYMBOL::SQUARE_BRACKET_CLOSE );
            match( OPERATOR::ASSIGNEMENT );
            auto val = expr();
            return ArrayAssignment{ id, pos, val };
        }

        else if ( lookup_eq( CONTROL_SYMBOL::BRACKET_OPEN ) )
        {
            match( CONTROL_SYMBOL::BRACKET_OPEN );
            auto args = arguments();
            match( CONTROL_SYMBOL::BRACKET_CLOSE );
            return SubprogramCall{ id, args };
        }

        else
        {
            fail( "assignment or subprogram call", lookup()->variant );
        }
    }

    If Parser::if_p()
    {
        match( KEYWORD::IF );
        auto exp = expr();
        match( KEYWORD::THEN );
        auto true_b = stat();
        if ( !lookup_eq( KEYWORD::ELSE ) )
        {
            return { exp, true_b, std::nullopt };
        }
        match( KEYWORD::ELSE );
        auto false_b = stat();
        return { exp, true_b, false_b };
    }

    While Parser::while_p()
    {
        match( KEYWORD::WHILE );
        auto exp = expr();
        match( KEYWORD::DO );
        auto st = stat();
        return { exp, st };
    }

    For Parser::for_p()
    {
        match( KEYWORD::FOR );
        auto id = match_identifier();
        match( OPERATOR::ASSIGNEMENT );
        auto init = expr();

        For::DIRECTION dir;
        if ( lookup_eq( KEYWORD::TO ) )
        {
            match( KEYWORD::TO );
            dir = For::DIRECTION::TO;
        }
        else if ( lookup_eq( KEYWORD::DOWNTO ) )
        {
            match( KEYWORD::DOWNTO );
            dir = For::DIRECTION::DOWNTO;
        }
        else
        {
            fail( "to or downto", lookup()->variant );
        }

        auto target = expr();
        match( KEYWORD::DO );
        auto st = stat();
        return { id, init, dir, target, st };
    }

    /*********************************************************************/

    Expression Parser::expr()
    {
        auto lhs = simple_expr();
        if ( lookup_eq( {
            OPERATOR::EQUAL, OPERATOR::NOT_EQUAL,
            OPERATOR::LESS, OPERATOR::LESS_EQUAL,
            OPERATOR::MORE, OPERATOR::MORE_EQUAL
            } ) )
        {
            auto op = token_to_ast_operator(match_operator());
            auto rhs = simple_expr();
            return make_ptr<BinaryOperator>( op, lhs, rhs );
        }

        return lhs;
    }

    Expression Parser::simple_expr()
    {
        auto lhs = term();
        return more_simple_expr( lhs );
    }

    Expression Parser::more_simple_expr( const Expression & lhs )
    {
        BinaryOperator::OPERATOR op;
        if ( lookup_eq( { token::OPERATOR::PLUS, token::OPERATOR::MINUS } ) )
        {
            op = token_to_ast_operator( match_operator() );
        }
        else if ( lookup_eq( token::KEYWORD::OR ))
        {
            match( token::KEYWORD::OR );
            op = BinaryOperator::OPERATOR::OR;
        }
        else if ( lookup_eq(token::KEYWORD::XOR ))
        {
            match( token::KEYWORD::XOR );
            op = BinaryOperator::OPERATOR::XOR;
        }
        else // rule: this -> epsilon
        {
            return lhs;
        }

        auto ter = term();
        auto rhs = more_simple_expr( ter );
        return make_ptr<BinaryOperator>( op, lhs, rhs );
    }

    Expression Parser::term()
    {
        auto lhs = factor();
        return more_term( lhs );
    }

    Expression Parser::more_term( const Expression & lhs )
    {
        BinaryOperator::OPERATOR op;
        if ( lookup_eq( {token::OPERATOR::STAR, token::OPERATOR::SLASH}) )
        {
            op = token_to_ast_operator( match_operator() );
        }
        else if ( lookup_eq( token::KEYWORD::DIV ) )
        {
            match( token::KEYWORD::DIV );
            op = BinaryOperator::OPERATOR::INTEGER_DIVISION;
        }
        else if ( lookup_eq( token::KEYWORD::MOD ) )
        {
            match( token::KEYWORD::MOD );
            op = BinaryOperator::OPERATOR::MODULO;
        }
        else if ( lookup_eq( token::KEYWORD::AND ) )
        {
            match( token::KEYWORD::AND );
            op = BinaryOperator::OPERATOR::AND;
        }
        else // rule: this -> epsilon
        {
            return lhs;
        }

        auto fac = factor();
        auto rhs = more_term( fac );
        return make_ptr<BinaryOperator>( op, lhs, rhs );
    }

    Expression Parser::factor()
    {
        if ( lookup_type<token::Identifier>() )
        {
            auto id = match_identifier();
            if ( lookup_eq( CONTROL_SYMBOL::SQUARE_BRACKET_OPEN ) ){
                match(CONTROL_SYMBOL::SQUARE_BRACKET_OPEN);
                auto exp = expr();
                match(CONTROL_SYMBOL::SQUARE_BRACKET_CLOSE);
                return make_ptr<ArrayAccess>( id, exp );
            }
            else if ( lookup_eq( CONTROL_SYMBOL::BRACKET_OPEN ) ){
                match( CONTROL_SYMBOL::BRACKET_OPEN );
                auto exp = arguments();
                match ( CONTROL_SYMBOL::BRACKET_CLOSE );
                return make_ptr<SubprogramCall>( id, exp );
            }
            else{
                return VariableAccess{id};
            }
        }

        if ( lookup_type<token::Integer>() || lookup_type<token::Boolean>() )
        {
            auto c = match_constant();
            return ConstantExpression{ c };
        }

        if ( lookup_eq( CONTROL_SYMBOL::BRACKET_OPEN ) ){
            match( CONTROL_SYMBOL::BRACKET_OPEN );
            auto exp = expr();
            match( CONTROL_SYMBOL::BRACKET_CLOSE );
            return exp;
        }

        if ( lookup_eq( KEYWORD::NOT ) ){
            match(KEYWORD::NOT);
            auto fac = factor();
            return make_ptr<UnaryOperator>( UnaryOperator::OPERATOR::NOT, fac );
        }

        if ( lookup_eq( OPERATOR::MINUS ) ){
            match(OPERATOR::MINUS);
            auto fac = factor();
            return make_ptr<UnaryOperator>( UnaryOperator::OPERATOR::MINUS, fac );
        }

        if ( lookup_eq( OPERATOR::PLUS ) ){
            match(OPERATOR::PLUS);
            auto fac = factor();
            return make_ptr<UnaryOperator>( UnaryOperator::OPERATOR::PLUS, fac );
        }

        fail("factor", lookup()->variant );
    }

    /*********************************************************************/

    Many<Expression> Parser::arguments()
    {
        if ( lookup_eq( CONTROL_SYMBOL::BRACKET_CLOSE ) ){
            return Many<Expression>{};
        }

        Many<Expression> exs { expr() };

        while ( lookup_eq( CONTROL_SYMBOL::COMMA ) ){
            match( CONTROL_SYMBOL::COMMA );
            exs.push_back( expr() );
        }

        return exs;
    }

    /*********************************************************************/

    Type Parser::type()
    {
        if ( lookup_eq( KEYWORD::ARRAY) )
        {
            match( KEYWORD::ARRAY );
            match( CONTROL_SYMBOL::SQUARE_BRACKET_OPEN );

            auto low = expr();
            match( CONTROL_SYMBOL::TWO_DOTS );
            auto high = expr();

            match( CONTROL_SYMBOL::SQUARE_BRACKET_CLOSE );

            match( KEYWORD::OF );
            auto t = type();

            return make_ptr<Array>( low, high, t );
        }
        else if (lookup_eq( KEYWORD::INTEGER ) )
        {
            match( KEYWORD::INTEGER );
            return SimpleType::INTEGER;
        }
        else if ( lookup_eq(KEYWORD::BOOLEAN) )
        {
            match( KEYWORD::BOOLEAN );
            return SimpleType::BOOLEAN;
        }
        else
        {
            fail( "type", lookup()->variant );
        }
    }
}
