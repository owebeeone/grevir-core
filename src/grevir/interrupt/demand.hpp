#pragma once

#include <grevir/interrupt/catalog.hpp>
#include <grevir/interrupt/handler.hpp>

namespace grevir::interrupt {

namespace detail {
template <class... Events>
struct DemandData {
  inline static constexpr auto value = [] {
    DemandSummary<sizeof...(Events)> result;
    FixedArray<EventIdentity, sizeof...(Events)> keys{};
    FixedArray<bool, sizeof...(Events)> present{};
    FixedArray<std::string_view, sizeof...(Events)> handlers{};
    FixedArray<std::string_view, sizeof...(Events)> contexts{};
    FixedArray<std::string_view, sizeof...(Events)> deliveries{};
    std::size_t next = 0;
    ((keys[next++] = identity<typename Events::Key>()), ...);
    next = 0;
    ((present[next++] = HandlerChoice<Events>::raw
      || HandlerChoice<Events>::event), ...);
    next = 0;
    ((handlers[next++] = HandlerChoice<Events>::handler), ...);
    next = 0;
    ((contexts[next++] = HandlerChoice<Events>::context), ...);
    next = 0;
    ((deliveries[next++] = HandlerChoice<Events>::delivery), ...);
    for (std::size_t i = 0; i < keys.size(); ++i) {
      if (present[i]) {
        const auto position = result.count++;
        result.keys[position] = keys[i];
        result.handlers[position] = handlers[i];
        result.contexts[position] = contexts[i];
        result.deliveries[position] = deliveries[i];
      }
    }
    for (std::size_t i = 0; i < result.count; ++i) {
      for (std::size_t j = i + 1; j < result.count; ++j) {
        if (result.keys[j] < result.keys[i]) {
          const auto temporary = result.keys[i];
          result.keys[i] = result.keys[j];
          result.keys[j] = temporary;
          const auto handler = result.handlers[i];
          result.handlers[i] = result.handlers[j];
          result.handlers[j] = handler;
          const auto context = result.contexts[i];
          result.contexts[i] = result.contexts[j];
          result.contexts[j] = context;
          const auto delivery = result.deliveries[i];
          result.deliveries[i] = result.deliveries[j];
          result.deliveries[j] = delivery;
        }
      }
    }
    return result;
  }();
};
} // namespace detail

template <class Spec>
struct LiveDemandSet {
  using Catalog = EventCatalog<Spec>;
  using Data = typename Catalog::Events::template eval<detail::DemandData>;
  inline static constexpr auto value = Data::value;
};

template <std::size_t LiveCapacity, std::size_t RecordedCapacity>
constexpr bool same_demands(const DemandSummary<LiveCapacity>& live,
    const DemandSummary<RecordedCapacity>& recorded) {
  if (live.count != recorded.count) { return false; }
  for (std::size_t i = 0; i < live.count; ++i) {
    if (live.keys[i] != recorded.keys[i]
        || live.handlers[i] != recorded.handlers[i]
        || live.contexts[i] != recorded.contexts[i]
        || live.deliveries[i] != recorded.deliveries[i]) {
      return false;
    }
  }
  return true;
}

#if defined(GREVIR_IRQ_PROBE)
template <class Spec>
struct DemandSet : LiveDemandSet<Spec> {};
#endif

} // namespace grevir::interrupt
