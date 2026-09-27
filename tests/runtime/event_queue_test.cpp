#include <grevir/core/allocated_application.hpp>
#include <grevir/event/queue.hpp>
#include <grevir/test/event_lock.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <thread>
#include <vector>

namespace event_queue_test {

namespace irq = grevir::interrupt;
struct A { using Key = irq::EventKey<"test", "a", "event">; };
struct B { using Key = irq::EventKey<"test", "b", "event">; };
struct C { using Key = irq::EventKey<"test", "c", "event">; };
struct S { using Key = irq::EventKey<"test", "s", "event">; };
struct Request { using InterruptEvents = setl::TypeArgs<A, B, C, S>; };
template <class>
struct Module : ardo::ModuleBase<ardo::Parameters<>> {};
using Owner = grevir::RequestedModule<setl::TypeArgs<Request>, Module>;
struct MainLoopContext {
  inline static thread_local unsigned char token = 0;
  inline static std::atomic<const void*> owner{nullptr};
  static void bind() noexcept {
    owner.store(&token, std::memory_order_release);
  }
  static bool is_current() noexcept {
    return owner.load(std::memory_order_acquire) == &token;
  }
};
struct Board {
  using EventLock = grevir::test::EventLock;
  using MainLoopContext = event_queue_test::MainLoopContext;
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
struct grevir::event::RouteFor<event_queue_test::S> {
  using Context = grevir::event::MainLoop;
  using Delivery = grevir::event::Stream;
};

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

template <>
inline void grevir::on_event<event_queue_test::S>() noexcept {
  event_queue_test::seen.push_back('s');
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

TEST_CASE("stream retains each accepted firing and recovers after overflow", "[core][event]") {
  using namespace event_queue_test;
  using grevir::event::PostResult;
  grevir::event::stop<App>();
  seen.clear();
  grevir::event::prepare<App>();
  REQUIRE(grevir::event::post_from_isr<App, S>() == PostResult::queued);
  REQUIRE(grevir::event::post_from_isr<App, S>() == PostResult::queued);
  REQUIRE(grevir::event::post_from_isr<App, S>() == PostResult::full);
  REQUIRE(grevir::event::overrun<App>());
  REQUIRE(grevir::event::dispatch<App>(1) == 1);
  REQUIRE(seen == std::vector<char>{'s'});
  REQUIRE(grevir::event::post<App, S>() == PostResult::queued);
  REQUIRE(grevir::event::dispatch<App>(2) == 2);
  REQUIRE(seen == std::vector<char>{'s', 's', 's'});
  REQUIRE(grevir::event::overrun<App>());
  REQUIRE(grevir::event::post<App, S>() == PostResult::queued);
  grevir::event::stop<App>();
  grevir::event::prepare<App>();
  REQUIRE(grevir::event::dispatch<App>(1) == 0);
  REQUIRE_FALSE(grevir::event::overrun<App>());
  REQUIRE(grevir::event::post<App, A>() == PostResult::queued);
  REQUIRE(grevir::event::post<App, S>() == PostResult::queued);
  REQUIRE(grevir::event::post<App, A>() == PostResult::coalesced);
  REQUIRE(grevir::event::dispatch<App>(2) == 2);
  REQUIRE(seen == std::vector<char>{'s', 's', 's', 'a', 's'});
  grevir::event::stop<App>();
}

TEST_CASE("main-loop callbacks stay on the task that prepared the queue", "[core][event]") {
  using namespace event_queue_test;
  grevir::event::stop<App>();
  seen.clear();
  grevir::event::prepare<App>();
  REQUIRE(grevir::event::post<App, A>() == grevir::event::PostResult::queued);
  std::size_t foreign_dispatch = 99;
  grevir::event::PostResult foreign_post = grevir::event::PostResult::not_ready;
  std::thread foreign([&] {
    foreign_post = grevir::event::post<App, B>();
    foreign_dispatch = grevir::event::dispatch<App>(1);
  });
  foreign.join();
  REQUIRE(foreign_post == grevir::event::PostResult::queued);
  REQUIRE(foreign_dispatch == 0);
  REQUIRE(seen.empty());
  REQUIRE(grevir::event::dispatch<App>(2) == 2);
  REQUIRE(seen == std::vector<char>{'a', 'b'});
  grevir::event::stop<App>();
}

TEST_CASE("concurrent ISR publications keep the bounded Stream queue consistent", "[core][event]") {
  using namespace event_queue_test;
  grevir::event::stop<App>();
  seen.clear();
  grevir::event::prepare<App>();
  std::atomic<unsigned> queued{0};
  std::atomic<unsigned> full{0};
  auto publish = [&] {
    for (unsigned i = 0; i < 100; ++i) {
      const auto result = grevir::event::post_from_isr<App, S>();
      if (result == grevir::event::PostResult::queued) {
        queued.fetch_add(1, std::memory_order_relaxed);
      } else if (result == grevir::event::PostResult::full) {
        full.fetch_add(1, std::memory_order_relaxed);
      }
    }
  };
  std::thread first(publish);
  std::thread second(publish);
  first.join();
  second.join();
  REQUIRE(queued.load() == 2);
  REQUIRE(full.load() == 198);
  REQUIRE(grevir::event::overrun<App>());
  REQUIRE(grevir::event::dispatch<App>(2) == 2);
  REQUIRE(seen == std::vector<char>{'s', 's'});
  grevir::event::stop<App>();
}
