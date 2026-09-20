#include <grevir/core/module.hpp>

using Params = std::conditional_t<EMPTY_PARAMS,
  ardo::Parameters<>, ardo::Parameters<int, char>>;
using Selected = Params::Param<PARAM_INDEX>;
static_assert(sizeof(Selected) > 0);
