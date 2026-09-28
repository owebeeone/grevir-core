#pragma once

#include <grevir/base/compat/cstddef.hpp>
#include <grevir/base/compat/string_view.hpp>

namespace grevir::event {

// AVR and the host mock have no selected task identity restriction. Boards
// choose this policy explicitly instead of omitting the context contract.
template <class Spec>
struct UnrestrictedMainLoopContext {
  inline static constexpr std::string_view identity{"unrestricted_v1", 15};
  using Token = unsigned char;
  static constexpr Token current_token() noexcept { return 0; }
  static constexpr void bind(Token) noexcept {}
  static constexpr bool is_owner(Token) noexcept { return true; }
};

namespace detail {

template <class Lock, class Context>
struct ContextPolicyIdentity {
  inline static constexpr std::string_view lock{Lock::identity};
  inline static constexpr std::string_view context{Context::identity};
  inline static constexpr std::size_t length = lock.size() + 1 + context.size();

  struct Storage { char bytes[length + 1]{}; };
  inline static constexpr Storage storage = [] {
    Storage result{};
    std::size_t offset = 0;
    for (char value : lock) { result.bytes[offset++] = value; }
    result.bytes[offset++] = '_';
    for (char value : context) { result.bytes[offset++] = value; }
    return result;
  }();
  inline static constexpr std::string_view value{storage.bytes, length};
};

} // namespace detail
} // namespace grevir::event
