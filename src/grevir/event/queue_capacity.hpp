#pragma once

#include <grevir/base/compat/cstddef.hpp>
#include <grevir/base/compat/type_traits.hpp>

namespace grevir::event {

inline constexpr unsigned maximum_queue_capacity = 2048;

namespace detail {

template <class Board>
struct QueueCapacity {
  using Declared = std::remove_cv_t<decltype(Board::event_queue_capacity)>;
  inline static constexpr bool valid = [] {
    if constexpr (!std::is_integral_v<Declared>
                  || std::is_same_v<Declared, bool>) {
      return false;
    } else {
      return Board::event_queue_capacity > 0
        && Board::event_queue_capacity <= maximum_queue_capacity;
    }
  }();
  static_assert(valid, "GREVIR_EVENT_CAPACITY_OUT_OF_RANGE");
  inline static constexpr std::size_t value =
    static_cast<std::size_t>(Board::event_queue_capacity);
};

template <class Board, bool Selected>
struct SelectedQueueCapacity {
  inline static constexpr unsigned value = 0;
};

template <class Board>
struct SelectedQueueCapacity<Board, true> {
  inline static constexpr unsigned value =
    static_cast<unsigned>(QueueCapacity<Board>::value);
};

} // namespace detail
} // namespace grevir::event
