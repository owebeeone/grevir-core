#include "application.hpp"

const Counter<1>* other_address() {
  return Counter<1>::address();
}
