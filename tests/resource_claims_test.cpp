#include <grevir/core/resource_claims.hpp>

// Resource claim testers.
using su_claim1 = ardo::shared_use_claim<int, 1, long>;
using su_claim2 = ardo::shared_use_claim<int, 1, long>;
using su_claim3 = ardo::shared_use_claim<int, 1, unsigned>;
using su_claim4 = ardo::shared_use_claim<int, 2, unsigned>;
using su_claim5 = ardo::shared_use_claim<short, 1, unsigned>;

static_assert(
  false == ardo::has_conflict<su_claim1, su_claim2>::value,
  "Identical shared_use_claim claims should be OK");

static_assert(
  true == ardo::has_conflict<su_claim1, su_claim3>::value,
  "shared_use_claim with the same id and different params should be in conflict");

static_assert(
  false == ardo::has_conflict<su_claim1, su_claim4>::value,
  "shared_use_claim with different resource types should be OK");

static_assert(
  false == ardo::has_conflict<su_claim1, su_claim5>::value,
  "shared_use_claim with different resource types should be OK");

using r_claim1 = ardo::range_claim<int, 1, 4>;
using r_claim2 = ardo::range_claim<int, 4, 8>;
using r_claim3 = ardo::range_claim<int, 3, 8>;
using r_claim4 = ardo::range_claim<short, 1, 4>;

static_assert(
  true == ardo::has_conflict<r_claim1, r_claim1>::value,
  "range_claims that are idential have conflict");

static_assert(
  false == ardo::has_conflict<r_claim1, r_claim2>::value,
  "range_claims that don't have overlapping ranges are OK");

static_assert(
  true == ardo::has_conflict<r_claim1, r_claim3>::value,
  "range_claims that have overlapping ranges conflict");

static_assert(
  false == ardo::has_conflict<r_claim1, r_claim4>::value,
  "range_claims for different types are OK");
