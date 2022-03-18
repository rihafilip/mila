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

    struct ArrayAccess;
    struct SubprogramCall;
    struct UnaryPlus;
    struct UnaryMinus;
    struct UnaryNot;
    struct BinaryOperator;

    using Expression =
        std::variant<
            ptr<ArrayAccess>,
            ptr<SubprogramCall>,
            Constant, Identifier,
            ptr<UnaryPlus>,
            ptr<UnaryMinus>,
            ptr<UnaryNot>,
            ptr<BinaryOperator>>;

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

    struct UnaryPlus
    {
        Expression expression;
    };

    struct UnaryMinus
    {
        Expression expression;
    };

    struct UnaryNot
    {
        Expression expression;
    };

    struct BinaryOperator
    {
        enum class Operator
        {
            EQ, NOT_EQ,
            LESS_EQ, LESS, MORE_EQ, MORE,
            PLUS, MINUS, TIMES, DIVISION, MODULO,
            DIV, MOD,
            AND, OR, XOR
        };

        Operator op;
        Expression left;
        Expression right;
    };

    /***********************************/
    // Statements

    struct Block;
    struct While;
    struct For;
    struct If;
    struct Assignment
    {
        Identifier variable;
        Expression value;
    };

    using Statement =
        std::variant<ptr<Block>, ptr<While>, ptr<For>, ptr<If>, ptr<Assignment>>;

    struct Block
    {
        Many<Statement> statements;
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

    struct If
    {
        Expression condition;
        Statement trueCode;
        std::optional<Statement> elseCode;
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

    using SubprogramDecl = std::variant<ProcedureDecl, FunctionDecl>;

    using Subprogram =
        std::variant<ProcedureDecl, Procedure, FunctionDecl, Function>;

    /***********************************/
    // Program

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
