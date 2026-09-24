#pragma once

#include <grevir/interrupt/install.hpp>

namespace grevir::interrupt {

enum class SetupOutcome {
  success, configuration_failed, registration_failed,
  pending_policy_failed, cleanup_failed
};

enum class CallDisposition { initiated, waited, replayed };

struct StartResult {
  SetupOutcome outcome = SetupOutcome::success;
  SetupOutcome originating_failure = SetupOutcome::success;
  CallDisposition disposition = CallDisposition::initiated;
};

template <class Spec>
struct Application {
  using Board = typename Spec::Board;

  static StartResult start() noexcept {
    return Board::template StartPolicy<Spec>::execute([]() noexcept {
      Board::mask_owned();
      if (!Board::template configure<Spec>()) {
        return fail(SetupOutcome::configuration_failed);
      }
      Board::setup_modules();
      if (!detail::install_bindings<Spec>()) {
        return fail(SetupOutcome::registration_failed);
      }
      if (!Board::settle_pending()) {
        return fail(SetupOutcome::pending_policy_failed);
      }
      Board::template enable_owned<Spec>();
      return StartResult{};
    });
  }

 private:
  static StartResult fail(SetupOutcome reason) noexcept {
    Board::mask_owned();
    if (!Board::cleanup()) {
      return {SetupOutcome::cleanup_failed, reason, CallDisposition::initiated};
    }
    return {reason, reason, CallDisposition::initiated};
  }
};

} // namespace grevir::interrupt
