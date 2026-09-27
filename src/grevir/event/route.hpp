#pragma once

namespace grevir::event {

struct MainLoop {};
struct IsrLevel {};
struct Elide {};
struct Stream {};
struct Direct {};

template <class Event>
struct RouteFor {
  using Context = MainLoop;
  using Delivery = Elide;
};

} // namespace grevir::event

namespace grevir {

template <class Event>
void on_event() noexcept = delete;

} // namespace grevir
