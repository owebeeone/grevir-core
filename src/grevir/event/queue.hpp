#pragma once

#include <grevir/event/route.hpp>
#include <grevir/event/queue_capacity.hpp>
#include <grevir/event/context_policy.hpp>
#include <grevir/interrupt/catalog.hpp>
#include <grevir/base/type_for_size.hpp>
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
  using Context = typename Board::template MainLoopContext<Spec>;
  static constexpr std::size_t capacity = QueueCapacity<Board>::value;
  using index_type = typename setl::TypeForMaxValue<
    static_cast<std::uint32_t>(capacity)>::selected::type_unsigned;

 public:
  static bool prepare() noexcept {
    const auto current = Context::current_token();
    typename Board::EventLock::TaskGuard lock{};
    if (ready_ || dispatching_) { return false; }
    Context::bind(current);
    clear_records();
    head_ = 0;
    count_ = 0;
    overrun_ = false;
    ready_ = true;
    return true;
  }

  static void stop() noexcept {
    typename Board::EventLock::TaskGuard lock{};
    ready_ = false;
    clear_records();
    head_ = 0;
    count_ = 0;
  }

  template <class Event>
  static PostResult post() noexcept {
    return post_with_guard<Event, typename Board::EventLock::TaskGuard>();
  }

  template <class Event>
  static PostResult post_from_isr() noexcept {
    return post_with_guard<Event, typename Board::EventLock::IsrGuard>();
  }

  static std::size_t dispatch(std::size_t budget) noexcept {
    if (budget == 0) { return 0; }
    const auto current = Context::current_token();
    {
      typename Board::EventLock::TaskGuard lock{};
      if (!ready_ || dispatching_ || !Context::is_owner(current)) { return 0; }
      dispatching_ = true;
    }
    std::size_t handled = 0;
    while (handled < budget) {
      DispatchRecord record{};
      {
        typename Board::EventLock::TaskGuard lock{};
        if (!ready_ || count_ == 0) { break; }
        record = records_[head_];
        if (record.argument != nullptr) {
          *static_cast<bool*>(record.argument) = false;
        }
        head_ = static_cast<index_type>((head_ + 1) % capacity);
        --count_;
      }
      record.invoke(record.argument);
      ++handled;
    }
    {
      typename Board::EventLock::TaskGuard lock{};
      dispatching_ = false;
    }
    return handled;
  }

  static bool overrun() noexcept {
    typename Board::EventLock::TaskGuard lock{};
    return overrun_;
  }

  static void clear_overrun() noexcept {
    typename Board::EventLock::TaskGuard lock{};
    overrun_ = false;
  }

 private:
  template <class Event, class Guard>
  static PostResult post_with_guard() noexcept {
    static_assert(std::is_same_v<Event,
      typename interrupt::EventCatalog<Spec>::template ByKey<typename Event::Key>>,
      "GREVIR_EVENT_NOT_IN_APPLICATION");
    using Route = RouteFor<Event>;
    static_assert(std::is_same_v<typename Route::Context, MainLoop>
      && (std::is_same_v<typename Route::Delivery, Elide>
        || std::is_same_v<typename Route::Delivery, Stream>),
      "GREVIR_EVENT_DEFERRED_ROUTE_NOT_IMPLEMENTED");
    Guard lock{};
    if (!ready_) { return PostResult::not_ready; }
    if constexpr (std::is_same_v<typename Route::Delivery, Elide>) {
      if (Pending<Spec, Event>::value) { return PostResult::coalesced; }
    }
    if (count_ == capacity) {
      overrun_ = true;
      return PostResult::full;
    }
    const auto tail = static_cast<index_type>((head_ + count_) % capacity);
    if constexpr (std::is_same_v<typename Route::Delivery, Elide>) {
      records_[tail] = {&invoke<Event>, &Pending<Spec, Event>::value};
      Pending<Spec, Event>::value = true;
    } else {
      records_[tail] = {&invoke<Event>, nullptr};
    }
    ++count_;
    return PostResult::queued;
  }

  template <class Event>
  static void invoke(void*) noexcept { grevir::on_event<Event>(); }

  static void clear_records() noexcept {
    for (index_type i = 0; i < count_; ++i) {
      const auto index = static_cast<index_type>((head_ + i) % capacity);
      if (records_[index].argument != nullptr) {
        *static_cast<bool*>(records_[index].argument) = false;
      }
    }
  }

  inline static DispatchRecord records_[capacity]{};
  inline static index_type head_ = 0;
  inline static index_type count_ = 0;
  inline static bool ready_ = false;
  inline static bool overrun_ = false;
  inline static bool dispatching_ = false;
};

} // namespace detail

template <class Spec>
bool prepare() noexcept { return detail::MainLoopQueue<Spec>::prepare(); }

template <class Spec>
void stop() noexcept { detail::MainLoopQueue<Spec>::stop(); }

template <class Spec, class Event>
PostResult post() noexcept { return detail::MainLoopQueue<Spec>::template post<Event>(); }

template <class Spec, class Event>
PostResult post_from_isr() noexcept {
  return detail::MainLoopQueue<Spec>::template post_from_isr<Event>();
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
