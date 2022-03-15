#ifndef BIMAP_HPP
#define BIMAP_HPP

#include "concept.hpp"

#include <optional>
#include <map>

namespace cont
{
    /**
     * @brief Bidirectional map, allowing indexing both by keys and values
     *
     * @tparam Key key type
     * @tparam Value value type
     */
    template <typename Key, typename Value>
    class Bimap
    {
    private:
        std::map<Key, Value> m_KeyMap;
        std::map<Value, Key> m_ValueMap;

    public:
        constexpr Bimap( std::initializer_list<std::pair<Key, Value>> init )
            : m_KeyMap()
            , m_ValueMap()
        {
            for ( const auto& [k, v] : init )
            {
                m_KeyMap.insert( std::make_pair( k, v ) );
                m_ValueMap.insert( std::make_pair( v, k ) );
            }
        }

        Bimap() = default;

        constexpr ~Bimap() = default;

        /**
         * @brief Insert a pair of key and value into the map.
         * If either of given values is already present, it's value is deleted
         * from the other side
         *
         * @param k key
         * @param v value
         */
        constexpr void insert( const Key& k, const Value& v )
        {
            auto k1 = m_KeyMap.find( k );
            auto v1 = m_ValueMap.find( v );

            if ( k1 != m_KeyMap.end() )
                m_ValueMap.erase( k1.second );

            if ( v1 != m_ValueMap.end() )
                m_KeyMap.erase( v1.second );

            m_KeyMap.insert( std::make_pair( k, v ) );
            m_ValueMap.insert( std::make_pair( v, k ) );
        }

        std::optional<Value> byKeySafe( const Key& k ) const
        {
            auto i = m_KeyMap.find( k );
            if ( i == m_KeyMap.end() )
                return std::nullopt;

            return i->second;
        }

        std::optional<Key> byValueSafe( const Value& v ) const
        {
            auto i = m_ValueMap.find( v );
            if ( i == m_ValueMap.end() )
                return std::nullopt;

            return i->second;
        }

        Value byKey( const Key& k ) const
        {
            return m_KeyMap.at( k );
        }

        Key byValue( const Value& v ) const
        {
            return m_ValueMap.at( v );
        }

        Bimap( const Bimap& ) = default;
        Bimap( Bimap&& ) = default;

        Bimap& operator= ( const Bimap& ) = default;
        Bimap& operator= ( Bimap&& ) = default;
    };
}

#endif // BIMAP_HPP
