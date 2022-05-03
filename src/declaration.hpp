#ifndef DECLARATION_HPP
#define DECLARATION_HPP

#include <llvm/IR/Value.h>
#include <map>
#include <optional>
#include <string>
#include <vector>
namespace compiler
{

    class DeclarationMap
    {
    private:
        using Declaration = llvm::Value*;
        using Map = std::map<std::string, Declaration>;
        std::vector<Map> m_Data;

    public:
        DeclarationMap ()
        : m_Data ( 1, Map{} )
        {}

        void insert ( const std::string& name, const Declaration& dec )
        {
            m_Data[ m_Data.size() - 1 ].insert( { name, dec } );
        }

        std::optional<Declaration> find ( const std::string& name )
        {
            for ( const auto& m : m_Data )
            {
                auto i = m.find(name);
                if ( i != m.end() ){
                    return i->second;
                }
            }

            return std::nullopt;
        }

        void step_down ()
        {
            m_Data.push_back( Map{} );
        }

        void step_up ()
        {
            m_Data.pop_back();
        }
    };
}

#endif // DECLARATION_HPP
