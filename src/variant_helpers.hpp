#ifndef VARIANT_HELPERS_HPP
#define VARIANT_HELPERS_HPP

#include <variant>
#include <optional>

/// Simple helper struct allowing to construct overloaded lambdas
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded( Ts... )->overloaded<Ts...>;

/// Simple std::variant wrapper that is easier to use
template <class... Types>
class VariantWrapper
{
public:
    /// The actual variant class
    using inner_t = std::variant<Types...>;

    /// The wrapped type
    inner_t variant;

    VariantWrapper( const inner_t& v )
        : variant( v )
    {}

    /// Wrapper around std::get_if
    template <typename T>
    constexpr std::optional<T> get()
    {
        if ( const T* t = std::get_if<T>( &variant ) )
            return *t;

        return std::nullopt;
    }

    /// Wrapper around std::visit( overloaded{...} )
    template <typename... Visitors>
    constexpr auto visit( Visitors&&... vs )
    {
        return std::visit( overloaded{ vs... }, variant );
    }

    /**
     * @brief If input type is contained and is equal to contained value,
     * return true, otherwise false
     *
     * @tparam T Type we want to be contained
     * @param t value we want to compare equal to
     */
    template <typename T>
    constexpr bool isEq ( const T& t )
    {
        auto v = get<T>();
        if ( v.has_value() )
            return *v == t;

        return false;
    }
};

/// Wrap a variant
template <class... Types>
VariantWrapper<Types...> wrap( const std::variant<Types...>& v )
{
    return VariantWrapper( v );
}

#endif // VARIANT_HELPERS_HPP
