#include "application.hpp"

int main() {
  App::runSetup();
  App::runLoop();
  setl::logDebugFatal("installed Core resolves the compiled Base dependency");
  return other_address() == Counter<1>::address() ? 0 : 1;
}
