#include "stream_stack.hpp"

#include <stdexcept>

bool StreamStack::good() const
{
    if ( m_Stream.fail() )
        throw std::runtime_error( "Error while reading input." );

    return !m_Stream.eof();
}

StreamStack::StreamStack( std::istream& stream )
    : m_Stream( stream )
{}

std::optional<char> StreamStack::top()
{
    if ( !good() )
        return std::nullopt;

    return m_Stream.peek();
}

std::optional<char> StreamStack::pop()
{
    if ( !good() )
        return std::nullopt;

    return m_Stream.get();
}

char StreamStack::topForce()
{
    if ( !good() )
        throw std::runtime_error( "Unexpected EOF." );

    return m_Stream.peek();
}

bool StreamStack::popIf( char ch )
{
    auto optCh = top();
    if ( optCh.has_value() && optCh.value() == ch )
    {
        pop();
        return true;
    }

    return false;
}
