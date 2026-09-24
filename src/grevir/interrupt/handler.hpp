#pragma once

#include <grevir/base/compat/type_traits.hpp>

namespace grevir::interrupt::detail {

template <class Key>
struct BoundEventKey;

} // namespace grevir::interrupt::detail

#if defined(GREVIR_GENERATED_IRQ_HEADER) && !defined(GREVIR_IRQ_PROBE)
#include GREVIR_GENERATED_IRQ_HEADER
#endif

namespace grevir::interrupt::detail {

#if defined(GREVIR_IRQ_PROBE)
template <class Event>
struct BindingGate {
  static constexpr unsigned id = 0;
};
#else
template <class Event>
struct BindingGate {
  using Binding = BoundEventKey<typename Event::Key>;
  static_assert(std::is_same_v<Event, typename Binding::Event>,
    "GREVIR_IRQ_EVENT_TYPE_MISMATCH");
  static constexpr unsigned id = Binding::id;
};
#endif

} // namespace grevir::interrupt::detail

namespace grevir {

template <class Event, class Gate = interrupt::detail::BindingGate<Event>,
          unsigned = Gate::id>
void on_interrupt() noexcept = delete;

} // namespace grevir
