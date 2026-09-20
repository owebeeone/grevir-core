#pragma once

#include <grevir/base/meta/type_algorithms.hpp>

namespace ardo::nfp {

// Runner visits its type list backwards. Build a reversed dependency-first
// order, retaining reverse declaration order for otherwise independent modules.
template <typename Done, typename Active, typename Modules>
struct LifecycleVisitList;

template <typename Done, typename Active, typename Module,
  bool Complete = Done::template eval_arg1<Module, setl::Contains>::type::value,
  bool Visiting = Active::template eval_arg1<Module, setl::Contains>::type::value>
struct LifecycleVisit;

template <typename Done, typename Active>
struct LifecycleVisitList<Done, Active, setl::TypeArgs<>> {
  using type = Done;
};

template <typename Done, typename Active, typename Module, typename... Rest>
struct LifecycleVisitList<Done, Active, setl::TypeArgs<Module, Rest...>> {
  using tail = typename LifecycleVisitList<Done, Active, setl::TypeArgs<Rest...>>::type;
  using type = typename LifecycleVisit<tail, Active, Module>::type;
};

template <typename Done, typename Active, typename Module, bool Visiting>
struct LifecycleVisit<Done, Active, Module, true, Visiting> {
  using type = Done;
};

template <typename Done, typename Active, typename Module>
struct LifecycleVisit<Done, Active, Module, false, true> {
  static_assert(!std::is_same_v<Module, Module>, "GREVIR_CORE_DEPENDENCY_CYCLE");
  using type = Done;
};

template <typename Done, typename Active, typename Module>
struct LifecycleVisit<Done, Active, Module, false, false> {
  using dependencies = typename LifecycleVisitList<Done,
    typename Active::template cat<Module>, typename Module::Deps>::type;
  using type = typename dependencies::template catr<Module>;
};

template <typename Modules>
using LifecycleOrder = typename LifecycleVisitList<
  setl::TypeArgs<>, setl::TypeArgs<>, Modules>::type;

} // namespace ardo::nfp
