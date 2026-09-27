#include <grevir/core/allocated_application.hpp>
#include <grevir/event/queue.hpp>
#include <grevir/test/event_lock.hpp>
#include <catch2/catch_test_macros.hpp>
#include <vector>

namespace event_queue_test {

namespace irq = grevir::interrupt;
struct A { using Key = irq::EventKey<"test", "a", "event">; };
struct B { using Key = irq::EventKey<"test", "b", "event">; };
struct C { using Key = irq::EventKey<"test", "c", "event">; };
struct Request { using InterruptEvents = setl::TypeArgs<A, B, C>; };
template <class>
struct Module : ardo::ModuleBase<ardo::Parameters<>> {};
using Owner = grevir::RequestedModule<setl::TypeArgs<Request>, Module>;
struct Board {
  using EventLock = grevir::test::EventLock;
  inline static constexpr unsigned event_queue_capacity = 2;
};
using App = grevir::ApplicationSpec<Board, Owner>;

inline std::vector<char> seen{};
inline bool rearm_a = false;
inline grevir::event::PostResult rearm_result{};
inline bool try_nested_dispatch = false;
inline std::size_t nested_dispatch_count = 0;
inline unsigned callback_depth = 0;
inline unsigned maximum_callback_depth = 0;

} // namespace event_queue_test

template <>
inline void grevir::on_event<event_queue_test::A>() noexcept {
  ++event_queue_test::callback_depth;
  if (event_queue_test::callback_depth > event_queue_test::maximum_callback_depth) {
    event_queue_test::maximum_callback_depth = event_queue_test::callback_depth;
  }
  event_queue_test::seen.push_back('a');
  if (event_queue_test::rearm_a) {
    event_queue_test::rearm_a = false;
    event_queue_test::rearm_result =
      grevir::event::post<event_queue_test::App, event_queue_test::A>();
  }
  if (event_queue_test::try_nested_dispatch) {
    event_queue_test::try_nested_dispatch = false;
    event_queue_test::nested_dispatch_count =
      grevir::event::dispatch<event_queue_test::App>(1);
  }
  --event_queue_test::callback_depth;
}

template <>
inline void grevir::on_event<event_queue_test::B>() noexcept {
  event_queue_test::seen.push_back('b');
}

template <>
inline void grevir::on_event<event_queue_test::C>() noexcept {
  event_queue_test::seen.push_back('c');
}

TEST_CASE("main-loop events elide and preserve bounded queue order", "[core][event]") {
  using namespace event_queue_test;
  using grevir::event::PostResult;
  grevir::event::stop<App>();
  seen.clear();
  REQUIRE(grevir::event::post<App, A>() == PostResult::not_ready);
  grevir::event::prepare<App>();
  REQUIRE(grevir::event::post_from_isr<App, A>() == PostResult::queued);
  REQUIRE(grevir::event::post_from_isr<App, A>() == PostResult::coalesced);
  REQUIRE(grevir::event::post<App, B>() == PostResult::queued);
  REQUIRE(grevir::event::post<App, C>() == PostResult::full);
  REQUIRE(grevir::event::overrun<App>());
  rearm_a = true;
  REQUIRE(grevir::event::dispatch<App>(1) == 1);
  REQUIRE(rearm_result == PostResult::queued);
  REQUIRE(seen == std::vector<char>{'a'});
  REQUIRE(grevir::event::dispatch<App>(1) == 1);
  REQUIRE(seen == std::vector<char>{'a', 'b'});
  REQUIRE(grevir::event::dispatch<App>(1) == 1);
  REQUIRE(seen == std::vector<char>{'a', 'b', 'a'});
  REQUIRE(grevir::event::post<App, C>() == PostResult::queued);
  REQUIRE(grevir::event::dispatch<App>(1) == 1);
  REQUIRE(seen == std::vector<char>{'a', 'b', 'a', 'c'});
  grevir::event::clear_overrun<App>();
  REQUIRE_FALSE(grevir::event::overrun<App>());
  grevir::event::stop<App>();
  REQUIRE(grevir::event::post<App, A>() == PostResult::not_ready);
}

TEST_CASE("main-loop dispatch does not nest callbacks", "[core][event]") {
  using namespace event_queue_test;
  using grevir::event::PostResult;
  grevir::event::stop<App>();
  seen.clear();
  callback_depth = 0;
  maximum_callback_depth = 0;
  nested_dispatch_count = 99;
  rearm_a = true;
  try_nested_dispatch = true;
  grevir::event::prepare<App>();
  REQUIRE(grevir::event::post<App, A>() == PostResult::queued);
  REQUIRE(grevir::event::dispatch<App>(1) == 1);
  REQUIRE(nested_dispatch_count == 0);
  REQUIRE(maximum_callback_depth == 1);
  REQUIRE(seen == std::vector<char>{'a'});
  REQUIRE(grevir::event::dispatch<App>(1) == 1);
  REQUIRE(seen == std::vector<char>{'a', 'a'});
  grevir::event::stop<App>();
}
