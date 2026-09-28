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

  static consteval std::size_t digits(std::size_t value) {
    std::size_t count = 1;
    while (value >= 10) { value /= 10; ++count; }
    return count;
  }

  inline static constexpr std::size_t lock_digits = digits(lock.size());
  inline static constexpr std::size_t context_digits = digits(context.size());
  // Lengths make the one-field encoding injective even when names contain '_'.
  // Format: l<lock-length>_<lock>_c<context-length>_<context>.
  inline static constexpr std::size_t length = 1 + lock_digits + 1 + lock.size()
    + 2 + context_digits + 1 + context.size();
  // The interrupt probe stores this complete identity in one length byte.
  static_assert(length <= 255, "GREVIR_EVENT_POLICY_ID_TOO_LONG");

  struct Storage { char bytes[length + 1]{}; };
  inline static constexpr Storage storage = [] {
    Storage result{};
    std::size_t offset = 0;
    const auto append_number = [&](std::size_t value, std::size_t width) {
      for (std::size_t i = 0; i < width; ++i) {
        result.bytes[offset + width - 1 - i] = static_cast<char>('0' + value % 10);
        value /= 10;
      }
      offset += width;
    };
    result.bytes[offset++] = 'l';
    append_number(lock.size(), lock_digits);
    result.bytes[offset++] = '_';
    for (char value : lock) { result.bytes[offset++] = value; }
    result.bytes[offset++] = '_';
    result.bytes[offset++] = 'c';
    append_number(context.size(), context_digits);
    result.bytes[offset++] = '_';
    for (char value : context) { result.bytes[offset++] = value; }
    return result;
  }();
  inline static constexpr std::string_view value{storage.bytes, length};
};

} // namespace detail
} // namespace grevir::event
