#pragma once
#include <grevir/base/compat/array.hpp>
#include <grevir/base/compat/cstddef.hpp>
#include <grevir/base/compat/cstdint.hpp>

namespace grevir::allocation {

template <std::size_t R>
struct SearchResult {
  std::array<std::size_t,R> selected{};
  std::uint32_t visited = 0;
  bool success = false;
  bool exhausted = false;
};

// Inputs are already canonical and validated. A row is an ownership unit;
// columns are candidates. Compatibility is supplied by the capability policy.
// Runtime use supports host oracles; firmware callers use constant evaluation.
template <std::size_t R, std::size_t C, typename Compatible>
constexpr SearchResult<R> search(const std::array<std::array<bool,C>,R>& eligible,
    std::size_t units, Compatible compatible, std::uint32_t budget) {
  SearchResult<R> result;
  if (units > R) { return result; }
  const auto visit = [&](auto&& self, std::size_t u) -> bool {
    if (u == units) { return true; }
    for (std::size_t c = 0; c < C; ++c) {
      if (!eligible[u][c]) { continue; }
      if (result.visited == budget) { result.exhausted = true; return false; }
      ++result.visited;
      bool allowed = true;
      for (std::size_t earlier = 0; earlier < u; ++earlier) {
        if (!compatible(c, result.selected[earlier])) { allowed = false; break; }
      }
      if (allowed) {
        result.selected[u] = c;
        if (self(self, u + 1)) { return true; }
        if (result.exhausted) { return false; }
      }
    }
    return false;
  };
  result.success = visit(visit,0);
  if (!result.success) { result.selected.fill(0); }
  return result;
}

} // namespace grevir::allocation
