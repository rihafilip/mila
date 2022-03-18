#ifndef VARIANT_HELPERS_HPP
#define VARIANT_HELPERS_HPP

#include <variant>
#include <optional>

/// Simple helper struct allowing to construct overloaded lambdas
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded( Ts... )->overloaded<Ts...>;

template <class... Types>
class VariantWrapper
{
public:
    using inner_t = std::variant<Types...>;
    inner_t variant;

    VariantWrapper( const inner_t& v )
        : variant( v )
    {}

    template <typename T>
    constexpr std::optional<T> get()
    {
        if ( const T* t = std::get_if<T>( &variant ) )
            return *t;

        return std::nullopt;
    }

    template <typename... Visitors>
    constexpr auto visit( Visitors&&... vs )
    {
        return std::visit( overloaded{ vs... }, variant );
    }

    template <typename T>
    constexpr bool isEq ( const T& t )
    {
        auto v = get<T>();
        if ( v.has_value() )
            return *v == t;

        return false;
    }
};

template <class... Types>
VariantWrapper<Types...> wrap( const std::variant<Types...>& v )
{
    return VariantWrapper( v );
}

#endif // VARIANT_HELPERS_HPP
