#ifndef STREAM_STACK_HPP
#define STREAM_STACK_HPP

#include <iostream>
#include <optional>

/**
 * @brief Stack-like operations on std::istream (only extractions)
 *
 */
class StreamStack
{
private:
    std::istream& m_Stream;

    /// Throws if m_Stream is in fail state, returns if m_Stream is not eof
    bool good() const;

public:
    StreamStack( std::istream& stream );
    ~StreamStack() = default;

    std::optional<char> top();
    std::optional<char> pop();

    /// Return a top character or throw an exception
    char topForce();
    /// Pop the stack of `top() == ch`, return if stack has been popped
    bool popIf( char ch );
};

#endif // STREAM_STACK_HPP
