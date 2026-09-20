#pragma once

#include <grevir/core/module.hpp>
#include <grevir/core/lifecycle_order.hpp>
#include <grevir/core/resource_checks.hpp>

namespace ardo {

template <typename...w_Ms>
using ModuleClosure = typename nfp::ModuleClosureVa<setl::TypeArgs<>, w_Ms...>::type;

template <typename... Modules> 
class Application {
public:
  // Find the closure of all dependent modules.
  using AllModules = ModuleClosure<Modules...>;

  // Closure membership is unchanged; callbacks require a dependency-first order.
  using ModuleRunner = typename nfp::LifecycleOrder<AllModules>::template eval<Runner>;

  template <typename Op, typename VL>
  using Scanner = typename AllModules::template eval_arg2<Op, VL, setl::For>;

  using AutoScanner = typename AllModules::template eval<setl::AutoFor>;

  static void runSetup() {
    ModuleRunner::runParamsSetup();
    ModuleRunner::runSetup();
  }

  static void runLoop() {
    ModuleRunner::runParamsLoop();
    ModuleRunner::runLoop();
  }

  // Evaluates if any module has conflicts.
  static constexpr bool has_conflict = AutoScanner::template FullScanner<
    setl::Operator<ModuleConflictTest, setl::OrEval>>::value
    || AllModules::template eval<SelfModuleConflictTest>::value;

  static_assert(!has_conflict, "Application has resource conflict.");
};

} // namespace ardo
