#pragma once

#include <grevir/base/meta/tuple_types.hpp>

namespace ardo::sys::avr::base {

/**
 * The types of resources provided.
 */
enum class ResourceType : unsigned {
  none,
  digital_gpio,
  analog_in,
  analog_comparator_in,
  analog_comparator,
  pulse_width_modulation,
  counter_input,
  counter_capture,
  spi_bus,
  i2c_bus,
  uart_w,
  uart_r,
  uart_rw,
  uart_external_clock,
  interrupt_input,
  timed_event,
  clock_out,
  reset_input,
  xtal
};

/**
 * RootDependencies et al. Allows for a graph of resource dependencies
 * to determine what resources conflict if attempting to use them together.
 * The depencency graph must be acyclic (DAG). A resource that depends on
 * itself or nothing else is a root resource.
 */
namespace nfp {
// The nfp namespace is not part of the supported interface.
template <typename...T>
struct RootDependencyFinder;

template <typename T, typename w_Deps>
struct RootDependencyOfHelper;

template <typename T>
struct RootDependencyOfHelper<T, std::tuple<T>> {
  using result = std::tuple<T>;
};

template <typename T>
struct RootDependencyOfHelper<T, std::tuple<>> {
  using result = std::tuple<T>;
};

template <typename T, typename...D>
struct RootDependencyOfHelper<T, std::tuple<D...>> {
  using result = typename RootDependencyFinder<D...>::root_deps;
};

template <typename T>
struct RootDependencyOf {
  using result = typename RootDependencyOfHelper<
    T, typename T::dependencies>::result;
};

template <>
struct RootDependencyFinder<> {
  using root_deps = std::tuple<>;
};

template <typename T, typename...Ts>
struct RootDependencyFinder<T, Ts...> {
  using t_deps = typename RootDependencyOf<T>::result;
  using ts_deps = typename RootDependencyFinder<Ts...>::root_deps;
  using root_deps = typename setl::tuple_concat<t_deps, ts_deps>::type;
};

template <typename T>
struct RootDependencyHelper;

template <typename...P>
struct RootDependencyHelper<std::tuple<P...>> {
  using root_deps = typename RootDependencyFinder<P...>::root_deps;
};
} // namespace nfp

/**
 * Defines a resouce dependency.
 * T is a resource type.
 * P is a tuple of all resources T depends on.
 * 
 * For example: The SPI resource depends on the MISO, MOSI, SCK, SS
 * resources. Hence Dependency<SPI, std::tuple<MISO, MOSI, SCK, SS>> 
 * defines such a relationship. All the dependent resources must also
 * be derived from Dependency.
 */
template <typename T, ResourceType w_resource_type, typename P=std::tuple<T>>
struct Dependency;

template <typename T, ResourceType w_resource_type, typename...Ps>
struct Dependency<T, w_resource_type, std::tuple<Ps...>> {
  using Resource = T;
  static constexpr ResourceType resource_type = w_resource_type;
  using dependencies = std::tuple<Ps...>;
};

/**
 * Tuple of root dependencies of a resource.
 */
template <typename T>
using RootDependencies = typename nfp::RootDependencyHelper<
  typename T::dependencies>::root_deps;

namespace nfp {

template <typename T, ResourceType w_resource_type, typename w_resources>
struct ResourceFinderHelper {};

template <typename T, ResourceType w_resource_type>
struct ResourceFinderHelper<T, w_resource_type, std::tuple<>> {
  using Resource = void;
};

template <typename T, ResourceType w_resource_type, typename R, typename...Rs>
struct ResourceFinderHelper<T, w_resource_type, std::tuple<R, Rs...>> {
  using Rest = ResourceFinderHelper<T, w_resource_type, std::tuple<Rs...>>;

  using Resource = typename std::conditional_t<
      R::resource_type == w_resource_type
      && setl::tuple_contained_in<T, typename R::dependencies>::value, R, Rest>::Resource;
};

}  // namespace nfp

template <typename...w_Resources>
struct ResourceFinder {
  using Resources = std::tuple<w_Resources...>;

  // Find the resouce related to the root resources.
  template <typename T, ResourceType w_resource_type>
  using Resource = typename nfp::ResourceFinderHelper<
      T, w_resource_type, Resources>::Resource;
};

} // namespace ardo::sys::avr::base
