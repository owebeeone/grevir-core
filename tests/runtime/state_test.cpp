#include "mock_modules.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace core_mock;

TEST_CASE_METHOD(Fixture, "module state persists across application loops", "[core][mock]") {
  using A = Module<10>;
  reset<A>();
  using App = ardo::Application<A>;
  App::runSetup();
  for (unsigned n = 0; n < 3; ++n) {
    App::runLoop();
  }
  REQUIRE(A::state().setups == 1);
  REQUIRE(A::state().loops == 3);
  App::runSetup();
  REQUIRE(A::state().setups == 2);
  REQUIRE(A::state().loops == 0);
  App::runLoop();
  REQUIRE(A::state().loops == 1);
}

TEST_CASE_METHOD(Fixture, "distinct module identities have independent state", "[core][mock]") {
  using A = Module<10>;
  using B = Module<11>;
  reset<A, B>();
  ardo::Application<A, B>::runSetup();
  ardo::Application<A>::runLoop();
  REQUIRE(static_cast<const void*>(&A::state()) != static_cast<const void*>(&B::state()));
  REQUIRE(A::state().loops == 1);
  REQUIRE(B::state().loops == 0);
  ardo::Application<A, B>::runLoop();
  REQUIRE(A::state().loops == 2);
  REQUIRE(B::state().loops == 1);
}

TEST_CASE_METHOD(Fixture, "one singleton is shared across translation units", "[core][mock]") {
  reset<CrossUnitModule>();
  REQUIRE(address_from_other_unit() == &CrossUnitModule::state());
  ardo::Application<CrossUnitModule>::runSetup();
  loop_from_other_unit();
  REQUIRE(CrossUnitModule::state().loops == 1);
  ardo::Application<CrossUnitModule>::runLoop();
  REQUIRE(CrossUnitModule::state().loops == 2);
}

TEST_CASE_METHOD(Fixture, "fixture reset prevents state leaking into the next scenario", "[core][mock]") {
  using A = Module<10>;
  reset<A>();
  using App = ardo::Application<A>;
  App::runSetup();
  App::runLoop();
  const auto first_trace = events;
  reset<A>();
  REQUIRE(A::state().loops == 0);
  REQUIRE(A::state().setups == 0);
  REQUIRE(events.empty());
  App::runSetup();
  App::runLoop();
  REQUIRE(events == first_trace);
  REQUIRE(A::state().loops == 1);
}
