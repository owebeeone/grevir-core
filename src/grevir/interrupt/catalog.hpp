#pragma once

#include <grevir/core/application.hpp>
#include <grevir/base/compat/cstddef.hpp>
#include <grevir/base/compat/string_view.hpp>
#include <grevir/base/compat/type_traits.hpp>

namespace grevir::interrupt {

template <class T, std::size_t N>
struct FixedArray {
  T values[N == 0 ? 1 : N]{};
  constexpr std::size_t size() const { return N; }
  constexpr T& operator[](std::size_t index) { return values[index]; }
  constexpr const T& operator[](std::size_t index) const { return values[index]; }
};

template <std::size_t N>
struct Text {
  char value[N];
  consteval Text(const char (&input)[N]) {
    for (std::size_t i = 0; i < N; ++i) { value[i] = input[i]; }
  }
  constexpr std::string_view view() const { return {value, N - 1}; }
  constexpr bool operator==(const Text&) const = default;
};

template <Text Instance, Text Request, Text Kind>
struct EventKey {
  inline static constexpr auto instance = Instance;
  inline static constexpr auto request = Request;
  inline static constexpr auto kind = Kind;
};

struct EventIdentity {
  std::string_view instance;
  std::string_view request;
  std::string_view kind;
  constexpr auto operator<=>(const EventIdentity&) const = default;
};

template <std::size_t N>
struct DemandSummary {
  FixedArray<EventIdentity, N> keys{};
  std::size_t count = 0;
};

template <class Spec>
struct DemandSet;

template <std::size_t N>
constexpr std::string_view literal(const char (&value)[N]) {
  return {value, N - 1};
}

template <class Key>
constexpr EventIdentity identity() {
  return {Key::instance.view(), Key::request.view(), Key::kind.view()};
}

constexpr bool valid_component(std::string_view value) {
  if (value.empty()) { return false; }
  for (char c : value) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_')) { return false; }
  }
  return true;
}

namespace detail {
template <class... Lists>
struct Concat;
template <>
struct Concat<> { using type = setl::TypeArgs<>; };
template <class... First, class... Rest>
struct Concat<setl::TypeArgs<First...>, Rest...> {
  using type = typename Concat<Rest...>::type::template catr<First...>;
};

template <class...>
using Void = void;

template <class Request, class = void>
struct RequestEvents { using type = setl::TypeArgs<>; };
template <class Request>
struct RequestEvents<Request, Void<typename Request::InterruptEvents>> {
  using type = typename Request::InterruptEvents;
};

template <class... Requests>
struct CollectRequests {
  using type = typename Concat<typename RequestEvents<Requests>::type...>::type;
};
template <class Descriptor>
struct DescriptorEvents {
  using type = typename Descriptor::Requests::template eval<CollectRequests>::type;
};
template <class... Descriptors>
struct CollectDescriptors {
  using type = typename Concat<typename DescriptorEvents<Descriptors>::type...>::type;
};

template <class Key, class... Events>
struct Find;
template <class Key>
struct Find<Key> { using type = void; };
template <class Key, class First, class... Rest>
struct Find<Key, First, Rest...> {
  using type = std::conditional_t<std::is_same_v<Key, typename First::Key>,
    First, typename Find<Key, Rest...>::type>;
};

template <class... Events>
struct Unique;
template <>
struct Unique<> { static constexpr bool value = true; };
template <class First, class... Rest>
struct Unique<First, Rest...> {
  static constexpr bool value = (!std::is_same_v<typename First::Key,
    typename Rest::Key> && ...) && Unique<Rest...>::value;
};

template <class... Events>
struct CatalogData {
  static constexpr bool unique = Unique<Events...>::value;
  template <class Key>
  using ByKey = typename Find<Key, Events...>::type;
  inline static constexpr auto keys = [] {
    FixedArray<EventIdentity, sizeof...(Events)> result{};
    std::size_t next = 0;
    ((result[next++] = identity<typename Events::Key>()), ...);
    for (std::size_t i = 0; i < result.size(); ++i) {
      for (std::size_t j = i + 1; j < result.size(); ++j) {
        if (result[j] < result[i]) {
          const auto temporary = result[i];
          result[i] = result[j];
          result[j] = temporary;
        }
      }
    }
    return result;
  }();
  inline static constexpr bool valid = [] {
    for (std::size_t i = 0; i < keys.size(); ++i) {
      const auto& key = keys[i];
      if (!valid_component(key.instance) || !valid_component(key.request)
          || !valid_component(key.kind)) { return false; }
    }
    return true;
  }();
};
} // namespace detail

template <class Board_, class... Descriptors>
struct ApplicationSpec {
  using Board = Board_;
  using ModuleDescriptors = setl::TypeArgs<Descriptors...>;
  using Closure = ardo::ModuleClosure<Descriptors...>;
};

template <class Spec>
struct EventCatalog {
  using Events = typename Spec::Closure::template eval<detail::CollectDescriptors>::type;
  using Data = typename Events::template eval<detail::CatalogData>;
  static_assert(Data::unique, "GREVIR_IRQ_DUPLICATE_EVENT_KEY");
  static_assert(Data::valid, "GREVIR_IRQ_INVALID_EVENT_KEY");
  template <class Key>
  using ByKey = typename Data::template ByKey<Key>;
  inline static constexpr auto keys = Data::keys;
};

} // namespace grevir::interrupt

namespace grevir {

template <class Board, class... Descriptors>
using ApplicationSpec = interrupt::ApplicationSpec<Board, Descriptors...>;

} // namespace grevir
