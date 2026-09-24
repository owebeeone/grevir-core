#pragma once

#include <grevir/interrupt/catalog.hpp>
#if defined(GREVIR_IRQ_PROBE)
#include <grevir/interrupt/demand.hpp>
#endif

namespace grevir::interrupt {

// These values exist at constant evaluation only. Target backends turn the
// selected source and policy keys into registers or callback registrations.
struct BindingRecord {
  EventIdentity event{};
  std::string_view owner{};
  std::string_view configuration{};
  std::string_view source{};
  std::string_view selector{};
  std::string_view entry{};
  std::string_view snapshot_policy{};
  std::string_view acknowledge_policy{};
  unsigned dispatch_order = 0;
  bool shared_source = false;
  constexpr bool operator==(const BindingRecord&) const = default;
};

template <std::size_t Capacity>
struct SelectedPlan {
  FixedArray<BindingRecord, Capacity> bindings{};
  std::size_t count = 0;
  bool allocation_ok = false;
  bool search_exhausted = false;
};

enum class PlanError {
  none, allocation_failed, search_exhausted, count_mismatch,
  unknown_event, duplicate_event, missing_event, invalid_source,
  source_owner_conflict, entry_conflict, ambiguous_selector,
  inconsistent_source_policy, unstable_order, unsupported_shared_source
};

template <std::size_t CatalogSize, std::size_t DemandCapacity,
          std::size_t BindingCapacity>
constexpr PlanError validate(const FixedArray<EventIdentity, CatalogSize>& catalog,
    const DemandSummary<DemandCapacity>& demands,
    const SelectedPlan<BindingCapacity>& plan) {
  if (plan.search_exhausted) { return PlanError::search_exhausted; }
  if (!plan.allocation_ok) { return PlanError::allocation_failed; }
  if (plan.count != demands.count || plan.count > BindingCapacity) {
    return PlanError::count_mismatch;
  }
  for (std::size_t i = 0; i < plan.count; ++i) {
    const auto& binding = plan.bindings[i];
    bool known = false;
    bool demanded = false;
    for (std::size_t j = 0; j < catalog.size(); ++j) {
      if (catalog[j] == binding.event) { known = true; }
    }
    for (std::size_t j = 0; j < demands.count; ++j) {
      if (demands.keys[j] == binding.event) { demanded = true; }
    }
    if (!known || !demanded) { return PlanError::unknown_event; }
    if (binding.shared_source) { return PlanError::unsupported_shared_source; }
    if (!valid_component(binding.owner) || !valid_component(binding.configuration)
        || !valid_component(binding.source)
        || !valid_component(binding.selector) || !valid_component(binding.entry)
        || !valid_component(binding.snapshot_policy)
        || !valid_component(binding.acknowledge_policy)) {
      return PlanError::invalid_source;
    }
    if (i != 0 && !(plan.bindings[i - 1].event < binding.event)) {
      return plan.bindings[i - 1].event == binding.event
        ? PlanError::duplicate_event : PlanError::unstable_order;
    }
    for (std::size_t j = 0; j < i; ++j) {
      const auto& earlier = plan.bindings[j];
      if (earlier.source == binding.source) {
        return PlanError::unsupported_shared_source;
      } else if (earlier.entry == binding.entry) {
        return PlanError::entry_conflict;
      }
    }
  }
  for (std::size_t i = 0; i < demands.count; ++i) {
    bool found = false;
    for (std::size_t j = 0; j < plan.count; ++j) {
      if (demands.keys[i] == plan.bindings[j].event) { found = true; }
    }
    if (!found) { return PlanError::missing_event; }
  }
  return PlanError::none;
}

template <std::size_t CatalogSize, class Problem, std::size_t BindingCapacity>
constexpr bool validate_inventory(
    const FixedArray<EventIdentity, CatalogSize>& catalog,
    const Problem& problem, const SelectedPlan<BindingCapacity>& plan) {
  if (problem.requests.size() != catalog.size()) { return false; }
  for (std::size_t i = 0; i < catalog.size(); ++i) {
    std::size_t matches = 0;
    for (std::size_t j = 0; j < problem.requests.size(); ++j) {
      if (problem.requests[j].event == catalog[i]) { ++matches; }
    }
    if (matches != 1) { return false; }
  }
  for (std::size_t i = 0; i < problem.candidates.size(); ++i) {
    const auto& candidate = problem.candidates[i];
    if (candidate.owner != candidate.request.instance) { return false; }
  }
  if (plan.count > BindingCapacity) { return false; }
  for (std::size_t i = 0; i < plan.count; ++i) {
    const auto& binding = plan.bindings[i];
    bool offered = false;
    bool pwm_required = false;
    for (std::size_t j = 0; j < problem.requests.size(); ++j) {
      if (problem.requests[j].event == binding.event) {
        pwm_required = problem.requests[j].pwm_required;
      }
    }
    for (std::size_t j = 0; j < problem.candidates.size(); ++j) {
      const auto& candidate = problem.candidates[j];
      if (candidate.request == binding.event && !candidate.reserved
          && candidate.period_event && (!pwm_required || candidate.pwm)
          && candidate.owner == binding.owner
          && candidate.configuration == binding.configuration
          && candidate.source == binding.source
          && candidate.selector == binding.selector
          && candidate.entry == binding.entry
          && candidate.snapshot_policy == binding.snapshot_policy
          && candidate.acknowledge_policy == binding.acknowledge_policy) {
        offered = true;
      }
    }
    if (!offered) { return false; }
  }
  return true;
}

template <class Spec>
struct BindingPlan {
  using Catalog = EventCatalog<Spec>;
  inline static constexpr auto demands = DemandSet<Spec>::value;
  using Allocation = typename Spec::Board::template Allocate<Spec>;
  inline static constexpr auto selected = Allocation::plan;
  static_assert(validate_inventory(Catalog::keys, Allocation::problem, selected),
    "GREVIR_IRQ_BINDING_NOT_IN_INVENTORY");
  static_assert(Allocation::validates(demands, selected),
    "GREVIR_IRQ_ALLOCATION_NOT_FROM_INVENTORY");
  inline static constexpr auto error = validate(Catalog::keys, demands, selected);
  static_assert(error == PlanError::none, "GREVIR_IRQ_BINDING_PLAN_INVALID");
};

} // namespace grevir::interrupt
