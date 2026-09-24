#pragma once

#include <grevir/interrupt/catalog.hpp>

namespace grevir::interrupt {

template <Text Source>
struct SourceKey {
  inline static constexpr auto value = Source;
};

namespace detail {

template <class Spec>
bool install_bindings() noexcept;

} // namespace detail
} // namespace grevir::interrupt
