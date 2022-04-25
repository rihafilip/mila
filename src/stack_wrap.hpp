#ifndef STACK_WRAP_HPP
#define STACK_WRAP_HPP

#include "concept.hpp"
#include <functional>
#include <optional>

namespace cont
{
    /**
    * @brief Wrapper around arbitrary container, adding top and pop functions
    *
    * @tparam Element Element type
    * @tparam Container Container type
    * @tparam PopFun Type of function that pops one element from container
    */
    template <typename Element, typename Container>
    class StackWrapper
    {
    public:
        using PopFun = std::function<std::optional<Element>(Container&)>;

    private:
        Container m_Data;
        const PopFun m_Pop;
        std::optional<Element> m_TopElement = std::nullopt;

    public:
        StackWrapper( const Container& cont, const PopFun& poppingFunction )
        : m_Data (cont)
        , m_Pop (poppingFunction)
        {}

        ~StackWrapper() = default;

        std::optional<Element> pop (){
            // top is not available -> pop new value
            if ( ! m_TopElement.has_value() ){
                return m_Pop(m_Data);
            }

            // top is available -> reset it and return it's value
            Element el = m_TopElement.value();
            m_TopElement = std::nullopt;
            return el;
        }

        std::optional<Element> top(){
            // If we currently hold no top, get a new one
            if ( ! m_TopElement.has_value() ){
                m_TopElement = m_Pop(m_Data);
            }

            return m_TopElement;
        }
    };
}

#endif // STACK_WRAP_HPP
