#ifndef UNIQUE_MAP_HPP
#define UNIQUE_MAP_HPP

#include <map>
#include <optional>
#include <stdexcept>
namespace cont
{
    /// Simple wrapper around map for holding variables
    template <typename Key, typename Value>
    class UniqueMap
    {
    private:
        std::map<Key, Value> m_Data;

    public:
        UniqueMap()
        : m_Data{}
        {}

        /// Try to find a value to an identifier in the map
        std::optional<Value> find ( const Key& ident ) const
        {
            auto i = m_Data.find( ident );
            if ( i != m_Data.end() ){
                return i->second;
            }

            return std::nullopt;
        }

        /// Add an identifier and it's value to the map, throwing if it is a redefinition
        void add ( const Key& ident, Value val )
        {
            auto i = m_Data.find( ident );
            if ( i != m_Data.end() ){
                throw std::runtime_error( "Redefinition of " + ident );
            }

            m_Data.insert({ ident, val });
        }
    };
}

#endif // UNIQUE_MAP_HPP
