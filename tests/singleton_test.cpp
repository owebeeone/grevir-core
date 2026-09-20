#include <grevir/core/application.hpp>

// Compile-only bindings replace the Arduino pins and time-poller dependency.
// The BlinkModule declarations below are retained from ardo_singleton_test.h.
namespace ardo {
template <typename Seq>
struct CyclicTimeSequencePoller {
  bool poll() { return true; }
  unsigned state() { return 0; }
};
} // namespace ardo

namespace singleton_test {

template <typename Seq, typename Params>
class BlinkModule : 
    public ardo::Singleton<BlinkModule<Seq, Params>>, public ardo::ModuleBase<Params> {
public:
  using LedPin = typename Params::template Param<0>;

  static void runLoop() {
    BlinkModule::instance.instanceLoop();
  }

  void instanceLoop() {
    if (timeSequence.poll()) {
      LedPin::set(timeSequence.state() & 1);
    }
  }

  ardo::CyclicTimeSequencePoller<Seq> timeSequence;
};

} // namespace singleton_test

namespace moduleinstance_test {

template <typename Seq, typename Params>
class BlinkModule :
  public ardo::ModuleInstanceBase<BlinkModule<Seq, Params>, Params> {
public:
  using LedPin = typename Params::template Param<0>;

  void instanceLoop() {
    if (timeSequence.poll()) {
      LedPin::set(timeSequence.state() & 1);
    }
  }

  ardo::CyclicTimeSequencePoller<Seq> timeSequence;
};

} // namespace moduleinstance_test

namespace {
template <unsigned Id>
struct Pin {
  using Claims = ardo::ResourceClaim<ardo::GPIOResource<Id>>;
  static void set(bool) {}
  static void runSetup() {}
  static void runLoop() {}
};
struct Sequence {};
using Legacy = singleton_test::BlinkModule<Sequence, ardo::Parameters<Pin<1>>>;
using Instance1 = moduleinstance_test::BlinkModule<Sequence, ardo::Parameters<Pin<2>>>;
using Instance2 = moduleinstance_test::BlinkModule<Sequence, ardo::Parameters<Pin<3>>>;
static_assert(!std::is_copy_constructible_v<Legacy>);
static_assert(!std::is_move_constructible_v<Instance1>);
static_assert(!std::is_copy_assignable_v<Instance1>);
static_assert(!std::is_same_v<Instance1, Instance2>);
using App = ardo::Application<Legacy, Instance1, Instance2>;
static_assert(!App::has_conflict);
} // namespace

void grevir_core_compile_singletons() {
  App::runSetup();
  App::runLoop();
}
