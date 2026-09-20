#include <grevir/core/application.hpp>

namespace {
using namespace ardo;
struct Bank {};
struct OtherBank {};
struct Timer {};
struct Slow {};
struct Fast {};

template <typename... Resources>
struct Param {
  using Claims = ResourceClaim<Resources...>;
  static void runSetup() {}
  static void runLoop() {}
};

template <int Identity, typename... Params>
struct Module : ModuleBase<Parameters<Params...>> {};

using Pin1 = Param<GPIOResource<1>>;
using Pin2 = Param<GPIOResource<2>>;
using SharedSlow = Param<shared_use_claim<Timer, 0, Slow>>;
using SharedFast = Param<shared_use_claim<Timer, 0, Fast>>;
using LowRange = Param<range_claim<Bank, 0, 4>>;
using HighRange = Param<range_claim<Bank, 4, 8>>;
using Overlap = Param<range_claim<Bank, 3, 8>>;
using Leaf = Module<0, Pin1>;
struct ViaDependency : ModuleBase<Parameters<>, DependentModules<Leaf>> {};
struct ViaParameter {
  using Deps = DependentModules<Leaf>;
  using Claims = ResourceClaim<>;
  static void runSetup() {}
  static void runLoop() {}
};

struct CycleA;
struct CycleB : ModuleBase<Parameters<>, DependentModules<CycleA>> {};
struct CycleA : ModuleBase<Parameters<>, DependentModules<CycleB>> {};
struct SelfCycle : ModuleBase<Parameters<>, DependentModules<SelfCycle>> {};

template <int Id>
struct Case;
template <>
struct Case<0> { using App = Application<>; };
template <>
struct Case<1> {
  using App = Application<ViaDependency, Module<1, ViaParameter>,
    Module<2, Pin2, SharedSlow, LowRange>, Module<3, SharedSlow, HighRange>>;
};
template <>
struct Case<2> { using App = Application<Leaf, Module<1, Pin1>>; };
template <>
struct Case<3> { using App = Application<Module<0, Pin1, Pin1>>; };
template <>
struct Case<4> { using App = Application<Module<0, LowRange>, Module<1, Overlap>>; };
template <>
struct Case<5> {
  using App = Application<Module<0, LowRange>, Module<1, Param<Bank>>>;
};
template <>
struct Case<6> {
  using App = Application<Module<0, SharedSlow>, Module<1, SharedFast>>;
};
template <>
struct Case<7> { using App = Application<ViaDependency, Module<1, Pin1>>; };
template <>
struct Case<8> {
  using App = Application<Module<0, Param<GPIOResource<1>, GPIOResource<1>>, Pin2>>;
};
template <>
struct Case<9> {
  using App = Application<Module<0, Param<range_claim<Bank, 4, 4>>>,
    Module<1, LowRange>>;
};
template <>
struct Case<10> { using App = Application<CycleA>; };
template <>
struct Case<11> { using App = Application<SelfCycle>; };
template <>
struct Case<12> {
  using App = Application<Module<0, Param<GPIOResource<1>, GPIOResource<1>>>>;
};
template <>
struct Case<13> {
  using App = Application<Module<0, Param<range_claim<Bank, 0, 4>, range_claim<Bank, 3, 8>>>>;
};
template <>
struct Case<14> {
  using App = Application<Module<0, Param<range_claim<Bank, 0, 8>, range_claim<Bank, 2, 4>>>>;
};
template <>
struct Case<15> {
  using App = Application<Module<0, Param<Bank, range_claim<Bank, 0, 4>>>>;
};
template <>
struct Case<16> {
  using App = Application<Module<0, Param<range_claim<Bank, 0, 4>, Bank>>>;
};
template <>
struct Case<17> {
  using App = Application<Module<0, Param<range_claim<Bank, 0, 4>, range_claim<Bank, 0, 4>>>>;
};
template <>
struct Case<18> {
  using App = Application<Module<0, Param<shared_use_claim<Timer, 0, Slow>,
    shared_use_claim<Timer, 0, Fast>>>>;
};
template <>
struct Case<19> {
  using App = Application<Module<0, Param<shared_use_claim<Timer, 0, Slow>,
    shared_use_claim<Timer, 0, Slow>>>>;
};
template <>
struct Case<20> {
  using App = Application<Module<0, Param<range_claim<Bank, 0, 4>,
    range_claim<OtherBank, 0, 4>>>>;
};
template <>
struct Case<21> { using App = Application<Module<0, Param<>>>; };
template <>
struct Case<22> {
  using App = Application<Module<0, Param<GPIOResource<1>, GPIOResource<2>,
    range_claim<Bank, 0, 4>, range_claim<Bank, 4, 8>,
    shared_use_claim<Timer, 1, Slow>, shared_use_claim<Timer, 2, Fast>>>>;
};
template <>
struct Case<23> {
  using App = Application<Module<0, Param<GPIOResource<1>, GPIOResource<2>, GPIOResource<1>>>>;
};
using InvalidDependency = Module<0, Param<GPIOResource<1>, GPIOResource<1>>>;
struct DependsOnInvalid : ModuleBase<Parameters<>, DependentModules<InvalidDependency>> {};
template <>
struct Case<24> { using App = Application<DependsOnInvalid>; };

using Selected = typename Case<CASE_ID>::App;
static_assert(sizeof(Selected) > 0);
void instantiate_lifecycle() {
  Selected::runSetup();
  Selected::runLoop();
}
} // namespace
