#pragma once
#include <GrevirCore.h>

template <unsigned Identity>
struct Counter : ardo::ModuleInstanceBase<Counter<Identity>> {
  unsigned count = 0;
  void instanceSetup() { count = 0; }
  void instanceLoop() { ++count; }
  static const Counter* address() { return &Counter::instance; }
};
using App = ardo::Application<Counter<1>, Counter<2>>;
const Counter<1>* other_address();
