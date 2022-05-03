#include "ast.hpp"
#include "variant_helpers.hpp"
#include <cstddef>
#include <string>

std::string line ( std::string str, size_t lvl )
{
    std::string prep ( lvl, '\t' );
    return prep + str + "\n";
}

namespace ast
{
    template <typename T>
    std::string to_string( const Many<T>& many, size_t level){
        std::string acc {};

        for ( const auto& i : many ){
            acc += to_string(i, level);
        }

        return acc;
    }

    /*****************************************************************/

    std::string to_string ( UnaryOperator::OPERATOR op ){
        switch (op) {
        case UnaryOperator::OPERATOR::PLUS:
            return "+";
        case UnaryOperator::OPERATOR::MINUS:
            return "-";
        case UnaryOperator::OPERATOR::NOT:
            return "NOT";
        default:
            return "?";
        }
    }

    std::string to_string ( BinaryOperator::OPERATOR op ){
        switch (op) {
        case BinaryOperator::OPERATOR::EQ:
            return "EQ";
        case BinaryOperator::OPERATOR::NOT_EQ:
            return "NOT EQ";

        case BinaryOperator::OPERATOR::LESS_EQ:
            return "LESS EQ";
        case BinaryOperator::OPERATOR::MORE_EQ:
            return "MORE EQ";
        case BinaryOperator::OPERATOR::LESS:
            return "LESS";
        case BinaryOperator::OPERATOR::MORE:
            return "MORE";

        case BinaryOperator::OPERATOR::PLUS:
            return "PLUS";
        case BinaryOperator::OPERATOR::MINUS:
            return "MINUS";
        case BinaryOperator::OPERATOR::TIMES:
            return "TIMES";
        case BinaryOperator::OPERATOR::DIVISION:
            return "DIVISION";

        case BinaryOperator::OPERATOR::INTEGER_DIVISION:
            return "INTEGER DIVISION";
        case BinaryOperator::OPERATOR::MODULO:
            return "MODULO";

        case BinaryOperator::OPERATOR::AND:
            return "AND";
        case BinaryOperator::OPERATOR::OR:
            return "OR";
        case BinaryOperator::OPERATOR::XOR:
            return "XOR";
        default:
            return "?";
        }
    }

    /*****************************************************************/

    std::string to_string ( const Type& type, size_t level )
    {
        return wrap(type).visit(
            [level]( SimpleType t  )-> std::string {
                switch (t) {
                case SimpleType::INTEGER:
                    return line("int", level);

                case SimpleType::BOOLEAN:
                    return line("bool", level);

                default:
                    return line("?", level);
                }
            },
            [level] (const ptr<Array>& arr) -> std::string{
                return line ( "ARRAY:", level )
                    + line ( "Of:", level + 1)
                    + to_string( arr->elementType, level+ 2)
                    + line ( "Low:", level + 1)
                    + to_string(arr->lowBound, level+2)
                    + line ( "High:", level + 1)
                    + to_string(arr->highBound, level+2);
            }
        );
    }

    std::string to_string ( const Variable& var, size_t level )
    {
        return line("VARIABLE:<'" + var.name + "'>", level )
            + line("Type", level + 1)
            + to_string( var.type, level+2 );
    }

    std::string to_string ( const Constant& constant, size_t level )
    {
        return wrap(constant).visit(
            [level]( const auto& i ){
                return line("CONSTANT <" + std::to_string(i.value) + ">", level);
            }
        );
    }

    std::string to_string ( const NamedConstant& constant, size_t level )
    {
        return line( "CONSTANT <" + constant.name + ">", level)
        + to_string( constant.value, level+1);
    }

    /*****************************************************************/

    std::string to_string ( const Expression& expr, size_t level )
    {
        return wrap( expr ).visit(
            [level]( const VariableAccess& v ){
                return line("VARIABLE <'" + v.identifier + "'>", level);
            },
            [level] ( const ConstantExpression& c ){
                return to_string( c.value, level);
            },
            [level]( ptr<ArrayAccess> arr ){
                return line("ARRAY_ACCESS <" + arr->array + ">", level)
                    + to_string(arr->value, level+1);
            },
            [level] (ptr<SubprogramCall> sub){
                return to_string(*sub, level);
            },
            [level] ( ptr<UnaryOperator> unary ){
                return line("UNARY <'" + to_string(unary->op) + "'>", level)
                    + to_string(unary->expression, level+1);
            },
            [level]( ptr<BinaryOperator> bin ){
                return line("BINARY <'" + to_string( bin->op ) + "'>" , level)
                    + to_string( bin->left, level+1 )
                    + to_string( bin->right, level+1 );
            }
        );
    }

    std::string to_string ( const Statement& stmt, size_t level )
    {
        return wrap(stmt).visit(
            [level] (const SubprogramCall& sub){
                return line("CALL <" + sub.functionName + ">", level)
                    + to_string(sub.arguments, level+1);
            },
            [level] (const Assignment& ass){
                return line("ASSIGNEMENT <" + ass.variable + ">", level)
                    + to_string( ass.value, level+1 );
            },
            [level] ( const ArrayAssignment& arr_ass ){
                return line("ARRAY ASSIGNEMENT <" + arr_ass.array + ">", level)
                    + line("At:", level+1)
                    + to_string(arr_ass.position, level+2)
                    + line("Value:", level+1)
                    + to_string(arr_ass.value, level+2);
            },
            []( const EmptyStatement& ) -> std::string {
                return "";
            },
            [level]( const ExitStatement& ) -> std::string {
                return line("EXIT", level);
            },
            [level] ( ptr<Block> block ){
                return line("BLOCK:", level)
                    + to_string(block->statements, level+1);
            },
            [level] (ptr<If> if_ ){
                std::string else_;

                if ( if_->elseCode.has_value() ){
                    else_ = line("False case:", level+1)
                        + to_string (if_->elseCode.value(), level+2);
                }

                return line("IF:", level)
                    + line ( "Condition:", level+1)
                    + to_string( if_->condition, level+2 )
                    + line("True case:", level+1)
                    + to_string (if_->trueCode, level+2)
                    + else_;
            },
            [level]( ptr<While> while_ ){
                return line("WHILE:", level)
                    + line("Condition:", level+1)
                    + to_string( while_->condition, level+2 )
                    + line("Do:", level+1)
                    + to_string( while_->code, level+2 );
            },
            [level]( ptr<For> for_ ){
                std::string dir_str;

                switch ( for_->direction ){
                    case For::DIRECTION::TO:
                        dir_str = "TO";
                        break;

                    case For::DIRECTION::DOWNTO:
                        dir_str = "DOWNTO";
                        break;
                }

                return line(std::string("FOR:") + "<" + for_->loopVariable + ">", level)
                    + line( "Init:", level+1 )
                    + to_string( for_->initialization, level+2 )
                    + line( "Dir: <" + dir_str + ">", level+1 )
                    + line( "Target:", level+1 )
                    + to_string( for_->target, level+2 )
                    + line( "Code:", level+1 )
                    + to_string( for_->code, level+2 );
            }
        );
    }

    std::string to_string ( const Global& subprogram, size_t level )
    {
        return wrap(subprogram).visit(
            [level]( const ProcedureDecl & decl ){
                return line("PROCEDURE DECLARATION <" + decl.name + ">", level)
                    + line( "PARAMS:", level )
                    + to_string( decl.parameters, level+1 );
            },
            [level]( const FunctionDecl & decl ){
                return line("FUNCION DECLARATION <" + decl.name + ">", level)
                    + line( "RETURN TYPE: ", level)
                    + to_string(decl.returnType, level + 1 )
                    + line( "PARAMS:", level )
                    + to_string( decl.parameters, level+1 );
            },
            [level]( const Procedure & proc ){
                return line("PROCEDURE <" + proc.name + ">", level)
                    + line( "PARAMS:", level )
                    + to_string( proc.parameters, level+1 )
                    + line( "VARS:", level )
                    + to_string( proc.variables, level+1 )
                    + line( "BLOCK:", level )
                    + to_string( make_ptr<Block>(proc.code), level+1 );
            },
            [level]( const Function & fun ){
                return line("FUNCTION <" + fun.name + ">", level)
                    + line( "RETURN TYPE: ", level)
                    + to_string(fun.returnType, level+1)
                    + line( "VARS:", level )
                    + to_string( fun.variables, level+1 )
                    + line( "PARAMS:", level )
                    + to_string( fun.parameters, level+1 )
                    + line( "BLOCK:", level )
                    + to_string( make_ptr<Block>(fun.code), level+1 );
            },
            [level]( const NamedConstant& named ){
                return to_string( named, level );
            },
            [level]( const Variable& var ){
                return to_string( var, level );
            }
        );
    }

    std::string to_string ( const Program& program )
    {
        return "PROGRAM " + program.name + "\n"
            + to_string ( program.globals, 1 )
            + to_string ( make_ptr<Block>(program.code), 1 );
    }

}

