#include "mock_modules.hpp"

namespace core_mock {
const CrossUnitModule* address_from_other_unit() {
  return &CrossUnitModule::state();
}
void loop_from_other_unit() {
  ardo::Application<CrossUnitModule>::runLoop();
}
} // namespace core_mock
