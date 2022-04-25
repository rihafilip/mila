#ifndef AST_HPP
#define AST_HPP

#include <variant>
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace ast
{
    // Simple aliases
    using Identifier = std::string;

    template <typename T>
    using Many = std::vector<T>;

    template <typename T>
    using ptr = std::shared_ptr<T>;

    template< class T, class... Args >
    ptr<T> make_ptr( Args&&... args )
    {
        return std::make_shared<T>( args... );
    }

    /***********************************/
    // Types
    enum class SimpleType
    {
        INTEGER, BOOLEAN
    };

    struct Array;
    using Type = std::variant<SimpleType, ptr<Array>>;

    struct Array
    {
    public:
        int lowBound;
        int highBound;
        Type elementType;
    };

    /***********************************/
    // Variables

    struct Variable
    {
        Identifier name;
        Type type;
    };

    /***********************************/
    // Constants

    struct BooleanConstant
    {
        bool value;
    };

    struct IntegerConstant
    {
        long long value;
    };

    using Constant = std::variant<BooleanConstant, IntegerConstant>;

    struct NamedConstant
    {
        Identifier name;
        Constant value;
    };

    /***********************************/
    // Expressions

    struct VariableAccess
    {
        Identifier identifier;
    };

    struct ConstantExpression
    {
        Constant value;
    };

    struct ArrayAccess;
    struct SubprogramCall;

    struct UnaryOperator;
    struct BinaryOperator;

    using Expression = std::variant<
        VariableAccess, ConstantExpression,
        ptr<ArrayAccess>, ptr<SubprogramCall>,
        ptr<UnaryOperator>, ptr<BinaryOperator>>;

    struct ArrayAccess
    {
        Identifier array;
        Expression value;
    };

    struct SubprogramCall
    {
        Identifier functionName;
        Many<Expression> arguments;
    };

    struct UnaryOperator
    {
        enum class OPERATOR
        {
            PLUS, MINUS, NOT
        };

        OPERATOR op;
        Expression expression;
    };

    struct BinaryOperator
    {
        enum class OPERATOR
        {
            EQ, NOT_EQ,
            LESS_EQ, LESS, MORE_EQ, MORE,
            PLUS, MINUS, TIMES, DIVISION,
            INTEGER_DIVISION, MODULO,
            AND, OR, XOR
        };

        OPERATOR op;
        Expression left;
        Expression right;
    };

    /***********************************/
    // Statements

    struct Assignment
    {
        Identifier variable;
        Expression value;
    };

    struct ArrayAssignment
    {
        Identifier array;
        Expression position;
        Expression value;
    };

    struct EmptyStatement
    {};

    struct Block;
    struct If;
    struct While;
    struct For;

    using Statement = std::variant<
        SubprogramCall,
        Assignment, ArrayAssignment, EmptyStatement,
        ptr<Block>, ptr<If>, ptr<While>, ptr<For>>;

    struct Block
    {
        Many<Statement> statements;
    };

    struct If
    {
        Expression condition;
        Statement trueCode;
        std::optional<Statement> elseCode;
    };

    struct While
    {
        Expression condition;
        Statement code;
    };

    // `for` loopVariable `:=` initialization direction target `do` code
    struct For
    {
        enum class DIRECTION
        {
            TO, DOWNTO
        };

        Identifier loopVariable;
        Expression initialization;
        DIRECTION direction;
        Expression target;
        Statement code;
    };

    /***********************************/
    // Subprograms

    struct ProcedureDecl
    {
        Identifier name;
        Many<Variable> parameters;
    };

    struct FunctionDecl
    {
        Identifier name;
        Many<Variable> parameters;
        Type returnType;
    };

    struct Procedure
    {
        Identifier name;
        Many<Variable> parameters;

        Many<Variable> variables;
        Block code;
    };

    struct Function
    {
        Identifier name;
        Many<Variable> parameters;
        Type returnType;

        Many<Variable> variables;
        Block code;
    };

    using Subprogram =
        std::variant<ProcedureDecl, Procedure, FunctionDecl, Function>;

    /***********************************/
    // Program

    // TODO don't split definitions
    struct Program
    {
        Identifier name;
        Many<Subprogram> subprograms;
        Many<NamedConstant> constants;

        Many<Variable> variables;
        Block code;
    };
}

#endif // AST_HPP
