#pragma once

#include <grevir/core/resource_claims.hpp>
#include <grevir/core/module.hpp>

namespace ardo {

namespace nfp {
// Not part of the public API.
// Processing conflict checks for claims.

// Tests a list of resources for conflict within the list itself.
template <typename... Res>
struct SelfParamsConflictTest;

template <>
struct SelfParamsConflictTest<> {
  using value_type = bool;
  constexpr static bool value = false;
};

template <typename Res>
struct SelfParamsConflictTest<Res> {
  using value_type = bool;
  constexpr static value_type value = false;
};

template <typename Res1, typename Res2, typename... Ress>
struct SelfParamsConflictTest<Res1, Res2, Ress...> {
  using value_type = bool;
  static constexpr value_type res1_res2_conflict = has_conflict<Res1, Res2>::value;
  static constexpr value_type value =
    res1_res2_conflict
    || SelfParamsConflictTest<Res1, Ress...>::value
    || SelfParamsConflictTest<Res2, Ress...>::value;

  static_assert(!res1_res2_conflict, "Found resource conflict in same claim.");
};

template <typename PL, typename PR>
struct ClaimsClaimConflictTest : PL::template has_resource<PR> {};

template <typename PL, typename PR>
struct ParamParamConflictTest {
  using value_type = bool;
  static constexpr value_type value = PL::Claims::template Scanner<
    setl::Operator<ClaimsClaimConflictTest, setl::OrEval>, typename PR::Claims>::value
    || PL::Claims::Resources::template eval<nfp::SelfParamsConflictTest>::value
    || PR::Claims::Resources::template eval<nfp::SelfParamsConflictTest>::value;
};

template <typename PSR, typename PL>
struct ParamParamsConflictTest {
  using value_type = bool;
  static constexpr value_type value = PSR::template Scanner<
    setl::Operator<ParamParamConflictTest, setl::OrEval>, PL>::value;
};

template <typename PSL, typename PSR>
struct ParamsParamsConflictTest {
  using value_type = bool;
  static constexpr value_type value = PSL::template Scanner<
    setl::Operator<ParamParamsConflictTest, setl::OrEval>, PSR>::value;

  static_assert(!value, "Application has resource conflict.");
};

}  // namespace nfp.

namespace nfp {
// Helper for testing module parameters self conflict.
template <typename... Params>
struct SelfModuleParamsConflictTest;

template <>
struct SelfModuleParamsConflictTest<> {
  using value_type = bool;
  constexpr static bool value = false;
};

template <typename Param>
struct SelfModuleParamsConflictTest<Param> {
  using value_type = bool;
  // A single parameter can contain several resources; validate its own claims
  // even when no second parameter exists to trigger a pairwise comparison.
  constexpr static value_type value =
    Param::Claims::Resources::template eval<SelfParamsConflictTest>::value;
};

template <typename Param1, typename Param2, typename... Params>
struct SelfModuleParamsConflictTest<Param1, Param2, Params...> {
  using value_type = bool;
  static constexpr value_type value =
    ParamParamConflictTest<Param1, Param2>::value
    || SelfModuleParamsConflictTest<Param1, Params...>::value
    || SelfModuleParamsConflictTest<Param2, Params...>::value;

  static_assert(!value, "Application has resource conflict within same module.");
};
}  // namespace nfp

// Resource conflict test for modules. Test all embedded params against each other.
template <typename M0, typename Mi>
struct ModuleConflictTest {
  using value_type = bool;
  static constexpr value_type value =
    nfp::ParamsParamsConflictTest<typename M0::Params, typename Mi::Params>::value;
};

// Each module also needs not to have a conflicting set of params.
template <typename... Ms>
struct SelfModuleConflictTest;

template <>
struct SelfModuleConflictTest<> {
  using value_type = bool;
  static constexpr value_type value = false;
};

template <typename M, typename... Ms>
struct SelfModuleConflictTest<M, Ms...> {
  using value_type = bool;
  static constexpr value_type value =
    M::Params::Params::template eval<nfp::SelfModuleParamsConflictTest>::value
    || SelfModuleConflictTest<Ms...>::value;

  static_assert(!value, "Application has resource conflict.");
};

} // namespace ardo
