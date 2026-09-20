#include "mock_modules.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace core_mock;

TEST_CASE_METHOD(Fixture, "empty application has no callbacks", "[core][mock]") {
  ardo::Application<>::runSetup();
  ardo::Application<>::runLoop();
  REQUIRE(events.empty());
}

TEST_CASE_METHOD(Fixture, "parameter callbacks precede module callbacks", "[core][mock]") {
  using A = Module<1, ardo::Parameters<Parameter<1>, Parameter<2>>>;
  using B = Module<2, ardo::Parameters<Parameter<3>>>;
  reset<A, B>();
  using App = ardo::Application<A, B>;
  App::runSetup();
  REQUIRE(events == std::vector<std::string>{
    "p3.setup", "p2.setup", "p1.setup", "m2.setup", "m1.setup"});
  events.clear();
  App::runLoop();
  REQUIRE(events == std::vector<std::string>{
    "p3.loop", "p2.loop", "p1.loop", "m2.loop", "m1.loop"});
  REQUIRE(A::state().loops == 1);
  REQUIRE(B::state().loops == 1);
}

TEST_CASE_METHOD(Fixture, "dependency chain runs from dependency to consumer", "[core][mock]") {
  using Leaf = Module<1>;
  using Middle = Module<2, ardo::Parameters<>, ardo::DependentModules<Leaf>>;
  using Root = Module<3, ardo::Parameters<>, ardo::DependentModules<Middle>>;
  reset<Leaf, Middle, Root>();
  ardo::Application<Root>::runSetup();
  REQUIRE(events == std::vector<std::string>{"m1.setup", "m2.setup", "m3.setup"});
  events.clear();
  ardo::Application<Root>::runLoop();
  REQUIRE(events == std::vector<std::string>{"m1.loop", "m2.loop", "m3.loop"});
}

TEST_CASE_METHOD(Fixture, "shared dependency runs once before both consumers", "[core][mock]") {
  using Shared = Module<1>;
  using Left = Module<2, ardo::Parameters<>, ardo::DependentModules<Shared>>;
  using Right = Module<3, ardo::Parameters<>, ardo::DependentModules<Shared>>;
  reset<Shared, Left, Right>();
  using App = ardo::Application<Left, Right>;
  App::runSetup();
  REQUIRE(events == std::vector<std::string>{"m1.setup", "m3.setup", "m2.setup"});
  REQUIRE(Shared::state().setups == 1);
  events.clear();
  App::runLoop();
  REQUIRE(events == std::vector<std::string>{"m1.loop", "m3.loop", "m2.loop"});
  REQUIRE(Shared::state().loops == 1);
}

TEST_CASE_METHOD(Fixture, "parameter dependencies and explicit roots share one instance", "[core][mock]") {
  using Shared = Module<1>;
  using Consumer = Module<2, ardo::Parameters<Parameter<1, Shared>>>;
  reset<Shared, Consumer>();
  using App = ardo::Application<Shared, Consumer, Shared, Consumer>;
  App::runSetup();
  REQUIRE(events == std::vector<std::string>{"p1.setup", "m1.setup", "m2.setup"});
  REQUIRE(Shared::state().setups == 1);
  REQUIRE(Consumer::state().setups == 1);
}

TEST_CASE_METHOD(Fixture, "uneven shared dependency graph respects every dependency", "[core][mock]") {
  using Leaf = Module<1>;
  using Middle = Module<2, ardo::Parameters<>, ardo::DependentModules<Leaf>>;
  using Left = Module<3, ardo::Parameters<>, ardo::DependentModules<Leaf>>;
  using Right = Module<4, ardo::Parameters<>, ardo::DependentModules<Middle>>;
  reset<Leaf, Middle, Left, Right>();
  ardo::Application<Left, Right, Leaf>::runSetup();
  REQUIRE(events == std::vector<std::string>{
    "m1.setup", "m2.setup", "m4.setup", "m3.setup"});
  REQUIRE(Leaf::state().setups == 1);
  REQUIRE(Middle::state().setups == 1);
}
