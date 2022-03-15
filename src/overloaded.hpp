#ifndef OVERLOADED_HPP
#define OVERLOADED_HPP

/// Simple helper struct allowing to construct overloaded lambdas
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded( Ts... )->overloaded<Ts...>;

#endif // OVERLOADED_HPP
