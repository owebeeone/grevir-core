#pragma once

#include <grevir/base/compat/tuple.hpp>

namespace ardo::sys {

enum class TimerAvailabilityMode {
  all_available,  // Running with scissors mode. Use of some timers may break.
  safe_to_use 
};

enum class BoardType {
  arduino_uno,
  arduino_nano,
  espressif_esp32,
};

/**
 * Specializations of this template are used to define the set of available
 * timers for a given MCU.
 */
template <typename w_Signature, TimerAvailabilityMode w_mode>
struct AvailableTimers {
  using Timers = std::tuple<>;
};


template <typename w_Signature, BoardType w_board>
struct IOMapping {
  using mapping = std::tuple<>;
};

} // namespace ardo::sys
