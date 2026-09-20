#include <grevir/core/module.hpp>

namespace {
struct Pin {};
struct Clock {};
struct Storage {};
using Params = ardo::Parameters<Pin, Clock, Storage>;
static_assert(std::is_same_v<Params::Param<0>, Pin>);
static_assert(std::is_same_v<Params::Param<1>, Clock>);
static_assert(std::is_same_v<Params::Param<2>, Storage>);
static_assert(std::is_same_v<ardo::Parameters<Clock>::Param<0>, Clock>);

using Repeated = ardo::Parameters<Pin, Clock, Pin>;
static_assert(std::is_same_v<Repeated::Param<1>, Clock>);
static_assert(std::is_same_v<Repeated::Param<2>, Pin>);

// Selection preserves the exact type and does not require an object instance.
struct Incomplete;
using Exact = ardo::Parameters<void, const Clock&, Incomplete>;
static_assert(std::is_same_v<Exact::Param<0>, void>);
static_assert(std::is_same_v<Exact::Param<1>, const Clock&>);
static_assert(std::is_same_v<Exact::Param<2>, Incomplete>);
static_assert(std::is_same_v<ardo::ParamByIndex<2, Pin, Clock, Storage>::param, Storage>);
} // namespace
