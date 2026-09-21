#pragma once
#include <grevir/core/application.hpp>

namespace grevir {
namespace nfp {

template <typename... Lists> struct Join;
template <> struct Join<> { using type = setl::TypeArgs<>; };
template <typename First, typename... Rest> struct Join<First,Rest...> {
  using type = typename First::template cat_type_arg<typename Join<Rest...>::type>;
};
template <typename... Params> struct ParameterClaims {
  using type = typename Join<typename Params::Claims::Resources...>::type;
};
template <typename... Modules> struct ModuleClaims {
  using type = typename Join<typename Modules::Params::Params::template eval<ParameterClaims>::type...>::type;
};
template <typename Claim> struct ClaimParameter {
  using Claims = Claim;
  static void runSetup() {}
  static void runLoop() {}
};

} // namespace nfp

// Static requirements and fixed claims are available before driver binding.
// Dependencies are descriptors too, and are collected once through ModuleClosure.
template <typename Requests_, template <typename> typename Module,
    typename Claims_ = ardo::ResourceClaim<>, typename... Dependencies>
struct RequestedModule {
  using Requests = Requests_;
  using Claims = Claims_;
  using Deps = setl::TypeArgs<Dependencies...>;
  template <typename Allocation>
  struct Bind : Module<Allocation> {
    using Impl = Module<Allocation>;
    using Deps = typename Impl::Deps::template cat<typename Dependencies::template Bind<Allocation>...>;
    using Params = typename Impl::Params::Params
      ::template cat<nfp::ClaimParameter<Claims>>::template eval<ardo::Parameters>;
  };
};

// Include an existing module and all its dependency claims in allocation input.
template <typename Module>
struct ExistingModule {
  using Requests = setl::TypeArgs<>;
  using Deps = setl::TypeArgs<>;
  using Claims = typename ardo::ModuleClosure<Module>::template eval<nfp::ModuleClaims>
    ::type::template eval<ardo::ResourceClaim>;
  template <typename> using Bind = Module;
};

namespace nfp {
template <typename Backend, typename... Descriptors>
struct Assemble {
  using Requests = typename Join<typename Descriptors::Requests...>::type;
  using Claims = typename Join<typename Descriptors::Claims::Resources...>::type;
  using Allocation = typename Backend::template Allocate<Requests,Claims>;
  static_assert(Allocation::plan.ok(), "GREVIR_APPLICATION_ALLOCATION_FAILED");
  struct Owner : ardo::ModuleBase<ardo::Parameters<ClaimParameter<typename Allocation::Claims>>> {};
  using Modules = ardo::Application<Owner,typename Descriptors::template Bind<Allocation>...>;
  static_assert(!Modules::has_conflict, "GREVIR_APPLICATION_RESOURCE_CONFLICT");
  static void runSetup() {
    // Register setup precedes every parameter and module setup callback.
    Allocation::setup();
    Modules::runSetup();
  }
  static void runLoop() { Modules::runLoop(); }
};
} // namespace nfp

// Backend owns capability generation and typed binding; Core owns module
// collection, lifecycle and final claim validation. No peripheral dependency.
template <typename Backend, typename... Descriptors>
struct AllocatedApplication
  : ardo::ModuleClosure<Descriptors...>::template eval_arg1<Backend,nfp::Assemble> {};

} // namespace grevir
