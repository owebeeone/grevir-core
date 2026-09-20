#pragma once

#include <grevir/base/compat/tuple.hpp>

namespace ardo::timers::nfp {

// Legacy placeholder only; this does not allocate or validate resources.
template <typename...w_Configs>
struct SelectionResolver {
    using configs = std::tuple<w_Configs...>;

    template <typename...w_SelectedConfigs>
    using Select = SelectionResolver;

    template <typename w_Config>
    using Resolve = w_Config;
};

} // namespace ardo::timers::nfp
