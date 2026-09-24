#pragma once

#include <grevir/interrupt/start.hpp>

namespace grevir::interrupt {

// For firmware setup paths that cannot be entered concurrently. In
// particular, this policy is not suitable for a multi-task ESP32 caller.
template <class Spec>
class SingleThreadStartPolicy {
 public:
  template <class Body>
  static StartResult execute(Body body) noexcept {
    if (started_) {
      auto result = result_;
      result.disposition = CallDisposition::replayed;
      return result;
    }
    started_ = true;
    result_ = body();
    return result_;
  }

 private:
  inline static bool started_ = false;
  inline static StartResult result_{};
};

} // namespace grevir::interrupt
