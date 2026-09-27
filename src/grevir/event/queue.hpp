#pragma once

#include <grevir/event/route.hpp>
#include <grevir/interrupt/catalog.hpp>
#include <grevir/base/compat/cstddef.hpp>
#include <grevir/base/compat/cstdint.hpp>
#include <grevir/base/compat/type_traits.hpp>

namespace grevir::event {

enum class PostResult { queued, coalesced, full, not_ready };

namespace detail {

struct DispatchRecord {
  void (*invoke)(void*) noexcept = nullptr;
  void* argument = nullptr;
};

template <class Spec, class Event>
struct Pending {
  inline static bool value = false;
};

template <class Spec>
class MainLoopQueue {
  using Board = typename Spec::Board;
  static constexpr std::size_t capacity = Board::event_queue_capacity;
  static_assert(capacity > 0 && capacity <= 255,
    "GREVIR_EVENT_CAPACITY_OUT_OF_RANGE");

 public:
  static void prepare() noexcept {
    typename Board::EventLock lock{};
    clear_records();
    head_ = 0;
    count_ = 0;
    overrun_ = false;
    ready_ = true;
  }

  static void stop() noexcept {
    typename Board::EventLock lock{};
    ready_ = false;
    clear_records();
    head_ = 0;
    count_ = 0;
  }

  template <class Event>
  static PostResult post() noexcept {
    static_assert(std::is_same_v<Event,
      typename interrupt::EventCatalog<Spec>::template ByKey<typename Event::Key>>,
      "GREVIR_EVENT_NOT_IN_APPLICATION");
    using Route = RouteFor<Event>;
    static_assert(std::is_same_v<typename Route::Context, MainLoop>
      && std::is_same_v<typename Route::Delivery, Elide>,
      "GREVIR_EVENT_DEFERRED_ROUTE_NOT_IMPLEMENTED");
    typename Board::EventLock lock{};
    if (!ready_) { return PostResult::not_ready; }
    if (Pending<Spec, Event>::value) { return PostResult::coalesced; }
    if (count_ == capacity) {
      overrun_ = true;
      return PostResult::full;
    }
    const auto tail = static_cast<std::uint8_t>((head_ + count_) % capacity);
    records_[tail] = {&invoke<Event>, &Pending<Spec, Event>::value};
    Pending<Spec, Event>::value = true;
    ++count_;
    return PostResult::queued;
  }

  static std::size_t dispatch(std::size_t budget) noexcept {
    std::size_t handled = 0;
    while (handled < budget) {
      DispatchRecord record{};
      {
        typename Board::EventLock lock{};
        if (!ready_ || count_ == 0) { break; }
        record = records_[head_];
        *static_cast<bool*>(record.argument) = false;
        head_ = static_cast<std::uint8_t>((head_ + 1) % capacity);
        --count_;
      }
      record.invoke(record.argument);
      ++handled;
    }
    return handled;
  }

  static bool overrun() noexcept {
    typename Board::EventLock lock{};
    return overrun_;
  }

  static void clear_overrun() noexcept {
    typename Board::EventLock lock{};
    overrun_ = false;
  }

 private:
  template <class Event>
  static void invoke(void*) noexcept { grevir::on_event<Event>(); }

  static void clear_records() noexcept {
    for (std::uint8_t i = 0; i < count_; ++i) {
      const auto index = static_cast<std::uint8_t>((head_ + i) % capacity);
      *static_cast<bool*>(records_[index].argument) = false;
    }
  }

  inline static DispatchRecord records_[capacity]{};
  inline static std::uint8_t head_ = 0;
  inline static std::uint8_t count_ = 0;
  inline static bool ready_ = false;
  inline static bool overrun_ = false;
};

} // namespace detail

template <class Spec>
void prepare() noexcept { detail::MainLoopQueue<Spec>::prepare(); }

template <class Spec>
void stop() noexcept { detail::MainLoopQueue<Spec>::stop(); }

template <class Spec, class Event>
PostResult post() noexcept { return detail::MainLoopQueue<Spec>::template post<Event>(); }

template <class Spec, class Event>
PostResult post_from_isr() noexcept {
  return detail::MainLoopQueue<Spec>::template post<Event>();
}

template <class Spec>
std::size_t dispatch(std::size_t budget) noexcept {
  return detail::MainLoopQueue<Spec>::dispatch(budget);
}

template <class Spec>
bool overrun() noexcept { return detail::MainLoopQueue<Spec>::overrun(); }

template <class Spec>
void clear_overrun() noexcept { detail::MainLoopQueue<Spec>::clear_overrun(); }

} // namespace grevir::event
