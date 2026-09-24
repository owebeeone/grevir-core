#pragma once

#include <grevir/interrupt/catalog.hpp>
#include <grevir/interrupt/handler.hpp>

#if defined(GREVIR_IRQ_PROBE)
namespace grevir::interrupt {

namespace detail {
template <class Event>
inline constexpr bool handler_present = requires { grevir::on_interrupt<Event>(); };

template <class... Events>
struct DemandData {
  inline static constexpr auto value = [] {
    DemandSummary<sizeof...(Events)> result;
    FixedArray<EventIdentity, sizeof...(Events)> keys{};
    FixedArray<bool, sizeof...(Events)> present{};
    std::size_t next = 0;
    ((keys[next++] = identity<typename Events::Key>()), ...);
    next = 0;
    ((present[next++] = handler_present<Events>), ...);
    for (std::size_t i = 0; i < keys.size(); ++i) {
      if (present[i]) { result.keys[result.count++] = keys[i]; }
    }
    for (std::size_t i = 0; i < result.count; ++i) {
      for (std::size_t j = i + 1; j < result.count; ++j) {
        if (result.keys[j] < result.keys[i]) {
          const auto temporary = result.keys[i];
          result.keys[i] = result.keys[j];
          result.keys[j] = temporary;
        }
      }
    }
    return result;
  }();
};
} // namespace detail

template <class Spec>
struct DemandSet {
  using Catalog = EventCatalog<Spec>;
  using Data = typename Catalog::Events::template eval<detail::DemandData>;
  inline static constexpr auto value = Data::value;
};

} // namespace grevir::interrupt
#endif
