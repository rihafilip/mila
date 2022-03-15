#ifndef CONCEPT_HPP
#define CONCEPT_HPP

#include <type_traits>

/**
 * @brief Concept than ensures that Fun returns Ret type on Args
 *
 * @tparam Fun
 * @tparam Ret
 * @tparam Args
 */
template <typename Fun, typename Ret, typename... Args>
concept Returns = std::is_same_v<Ret, std::invoke_result_t<Fun, Args...>>;

#endif // CONCEPT_HPP
