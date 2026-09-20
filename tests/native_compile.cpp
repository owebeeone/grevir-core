#include <GrevirCore.h>

namespace {
using namespace ardo;
struct Leaf : ModuleBase<> {};
struct Left : ModuleBase<Parameters<>, DependentModules<Leaf>> {};
struct Right : ModuleBase<Parameters<>, DependentModules<Leaf>> {};
using Closure = ModuleClosure<Left, Right, Leaf>;
static_assert(std::is_same_v<Closure, setl::TypeArgs<Left, Leaf, Right>>);
static_assert(std::is_same_v<ModuleClosure<>, setl::TypeArgs<>>);

struct Signature {};
static_assert(std::is_same_v<sys::AvailableTimers<Signature,
  sys::TimerAvailabilityMode::safe_to_use>::Timers, std::tuple<>>);
static_assert(std::is_same_v<sys::IOMapping<Signature,
  sys::BoardType::arduino_uno>::mapping, std::tuple<>>);
using Mapping = setl::PortMapping<Leaf, 7>;
static_assert(Mapping::pin_no == 7);
static_assert(std::is_same_v<Mapping::PinType, Leaf>);
static_assert(std::is_same_v<setl::DeviceMappings<std::tuple<Mapping>>::DigitalMap,
  std::tuple<Mapping>>);

// These legacy names are retained even though the graph has no AVR dependency.
namespace graph = ardo::sys::avr::base;
struct PinA : graph::Dependency<PinA, graph::ResourceType::digital_gpio> {};
struct PinB : graph::Dependency<PinB, graph::ResourceType::digital_gpio,
  std::tuple<>> {};
struct Bus : graph::Dependency<Bus, graph::ResourceType::spi_bus,
  std::tuple<PinA, PinB>> {};
struct Device : graph::Dependency<Device, graph::ResourceType::none,
  std::tuple<Bus>> {};
static_assert(std::is_same_v<graph::RootDependencies<Device>, std::tuple<PinA, PinB>>);
static_assert(std::is_same_v<graph::RootDependencies<PinA>, std::tuple<PinA>>);
// Preserve the legacy empty-dependency behavior of the public alias.
static_assert(std::is_same_v<graph::RootDependencies<PinB>, std::tuple<>>);
using Finder = graph::ResourceFinder<PinA, PinB, Bus>;
static_assert(std::is_same_v<Finder::Resource<PinA, graph::ResourceType::spi_bus>, Bus>);
static_assert(std::is_same_v<Finder::Resource<PinB, graph::ResourceType::spi_bus>, Bus>);
static_assert(std::is_same_v<Finder::Resource<Device, graph::ResourceType::spi_bus>, void>);
static_assert(std::is_same_v<Finder::Resource<PinA, graph::ResourceType::uart_rw>, void>);

// This is only a pass-through placeholder, not an allocation solver.
using Resolver = timers::nfp::SelectionResolver<PinA>;
static_assert(std::is_same_v<Resolver::Resolve<Bus>, Bus>);
static_assert(std::is_same_v<Resolver::Select<PinB>, Resolver>);

using Claims = ConcatenateResourceClaims<ResourceClaim<GPIOResource<1>>,
  ResourceClaim<GPIOResource<2>>>;
static_assert(Claims::has_resource<GPIOResource<1>>::value);
static_assert(Claims::has_resource<GPIOResource<2>>::value);
static_assert(!Claims::has_resource<GPIOResource<3>>::value);
using App = Application<Left, Right>;
static_assert(!App::has_conflict);
} // namespace

void grevir_core_compile_lifecycle() {
  App::runSetup();
  App::runLoop();
}
