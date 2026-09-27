#pragma once

#include <grevir/base/compat/type_traits.hpp>
#include <grevir/base/compat/string_view.hpp>
#include <grevir/event/queue.hpp>

namespace grevir::interrupt::detail {

template <class Key>
struct BoundEventKey;
template <class Event>
struct BindingGate;

} // namespace grevir::interrupt::detail

namespace grevir {

template <class Event, class Gate = interrupt::detail::BindingGate<Event>,
          unsigned = Gate::id>
void on_interrupt() noexcept = delete;

} // namespace grevir

namespace grevir::interrupt::detail {

template <class Event>
struct HandlerChoice {
  inline static constexpr bool raw = requires { grevir::on_interrupt<Event>(); };
  inline static constexpr bool event = requires { grevir::on_event<Event>(); };
  static_assert(!(raw && event), "GREVIR_IRQ_DUPLICATE_HANDLER");
  using Context = typename grevir::event::RouteFor<Event>::Context;
  using Delivery = typename grevir::event::RouteFor<Event>::Delivery;
  inline static constexpr bool is_direct =
    std::is_same_v<Context, grevir::event::IsrLevel>
      && std::is_same_v<Delivery, grevir::event::Direct>;
  inline static constexpr bool is_deferred =
    std::is_same_v<Context, grevir::event::MainLoop>
      && (std::is_same_v<Delivery, grevir::event::Elide>
        || std::is_same_v<Delivery, grevir::event::Stream>);
  static_assert(!event || is_direct || is_deferred, "GREVIR_IRQ_INVALID_EVENT_ROUTE");
  inline static constexpr std::string_view handler = raw
    ? std::string_view{"raw", 3} : std::string_view{"event", 5};
  inline static constexpr std::string_view context = raw || is_direct
    ? std::string_view{"isr", 3} : std::string_view{"main_loop", 9};
  inline static constexpr std::string_view delivery = raw || is_direct
    ? std::string_view{"direct", 6}
    : std::is_same_v<Delivery, grevir::event::Elide>
      ? std::string_view{"elide", 5} : std::string_view{"stream", 6};
};

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

namespace grevir::interrupt::detail {

template <class Event>
void dispatch_bound_interrupt() noexcept {
  using Choice = HandlerChoice<Event>;
  using Binding = BoundEventKey<typename Event::Key>;
  static_assert(Choice::raw || Choice::event, "GREVIR_IRQ_HANDLER_MISSING");
  static_assert(std::string_view{Choice::handler} == Binding::handler,
    "GREVIR_IRQ_STALE_HANDLER_KIND");
  static_assert(std::string_view{Choice::context} == Binding::context,
    "GREVIR_IRQ_STALE_EVENT_CONTEXT");
  static_assert(std::string_view{Choice::delivery} == Binding::delivery,
    "GREVIR_IRQ_STALE_EVENT_DELIVERY");
  if constexpr (Choice::raw) {
    grevir::on_interrupt<Event>();
  } else if constexpr (Choice::event && Choice::is_direct) {
    grevir::on_event<Event>();
  } else {
    using Application = typename Binding::Application;
    static_assert(Choice::is_deferred, "GREVIR_IRQ_INVALID_EVENT_ROUTE");
    if constexpr (requires {
      Application::Board::event_queue_capacity;
      typename Application::Board::EventLock;
    }) {
      (void)grevir::event::post_from_isr<Application, Event>();
    } else {
      static_assert(!Choice::is_deferred, "GREVIR_EVENT_CONTEXT_UNAVAILABLE");
    }
  }
}

} // namespace grevir::interrupt::detail
