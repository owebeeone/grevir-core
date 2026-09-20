#pragma once

#include <grevir/core/application.hpp>
#include <string>
#include <vector>

namespace core_mock {
inline std::vector<std::string> events;

inline void record(const char* kind, unsigned id, const char* phase) {
  events.push_back(std::string(kind) + std::to_string(id) + "." + phase);
}

template <unsigned Id, typename... Dependencies>
struct Parameter {
  using Claims = ardo::ResourceClaim<>;
  using Deps = ardo::DependentModules<Dependencies...>;
  static void runSetup() { record("p", Id, "setup"); }
  static void runLoop() { record("p", Id, "loop"); }
};

template <unsigned Id, typename Params = ardo::Parameters<>,
  typename Dependencies = ardo::DependentModules<>>
struct Module : ardo::ModuleInstanceBase<Module<Id, Params, Dependencies>, Params, Dependencies> {
  unsigned loops = 0;
  unsigned setups = 0;

  void instanceSetup() {
    ++setups;
    loops = 0;
    record("m", Id, "setup");
  }
  void instanceLoop() {
    ++loops;
    record("m", Id, "loop");
  }
  static Module& state() { return Module::instance; }
  static void reset() {
    state().loops = 0;
    state().setups = 0;
  }
};

struct Fixture {
  Fixture() { events.clear(); }
  ~Fixture() { events.clear(); }
  template <typename... Modules>
  static void reset() {
    (Modules::reset(), ...);
    events.clear();
  }
};

using CrossUnitModule = Module<100>;
const CrossUnitModule* address_from_other_unit();
void loop_from_other_unit();
} // namespace core_mock
