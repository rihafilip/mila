#include "compiler.hpp"
#include "lexer.hpp"
#include "ast.hpp"
#include "parser.hpp"
#include "tokens.hpp"

#include <fstream>
#include <ios>
#include <iostream>
#include <llvm/Support/raw_ostream.h>
#include <stdexcept>
#include <string>

constexpr const char * USAGE =
    "Usage: \n"
    "mila <IN_FILE> [FLAGS]\n"
    "\t-h\t\t Print this help\n"
    "\t-l\t\t Print lexer output\n"
    "\t-p\t\t Print parser output\n"
    "\t-o <OUT_FILE>\t Compile the input to OUT_FILE\n";

void print_lexer( const std::string& in_file )
{
    std::fstream f( in_file, std::ios_base::in );

    lexer::Lexer lex { f };

    while ( true )
    {
        auto t = lex.next();
        if ( t.has_value() )
        {
            std::cout << token::to_string(t.value()) << '\n';
        }
        else
        {
            break;
        }
    }
}

void print_parser( const std::string& in_file )
{
    std::fstream f( in_file, std::ios_base::in );

    auto ast = parser::Parser::parse( f );
    std::cout << ast::to_string( ast ) << std::endl;
}

void compile( const std::string& in_file, const std::string& out_file )
{
    std::fstream inf ( in_file, std::ios_base::in );

    auto ast = parser::Parser::parse( inf );

    auto visitor = compiler::AstVisitor::compile( ast );
    const auto& module = visitor->get_module();

    module.print(llvm::outs(), nullptr);
}

int main( int argc, char const* argv [] )
{
    if ( argc == 2 && std::string(argv[1]) == "-h" ) {
        std::cout << USAGE << std::endl;
        return 0;
    }

    if ( argc < 3 ){
        std::cerr << USAGE << std::endl;
        return 2;
    }

    try
    {
        std::string flag ( argv[2] );

        if ( flag == "-l" ) {
            print_lexer(argv[1]);
        }
        else if ( flag == "-p" ) {
            print_parser(argv[1]);
        }
        else if ( flag == "-o" && argc == 4 ) {
            compile( argv[1], argv[3] );
        }
        else {
            std::cerr << USAGE << std::endl;
            return 2;
        }
    }
    catch ( std::runtime_error& e )
    {
        std::cerr << e.what() << std::endl;
        return 2;
    }

    return 0;
}

