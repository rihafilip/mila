#ifndef STREAM_STACK_HPP
#define STREAM_STACK_HPP

#include <iostream>
#include <optional>

class StreamStack
{
private:
    std::istream& m_Stream;

    bool good() const;

public:
    StreamStack( std::istream& stream );
    ~StreamStack() = default;

    std::optional<char> top();
    std::optional<char> pop();

    char topForce();
    bool popIf( char ch );
};

#endif // STREAM_STACK_HPP
