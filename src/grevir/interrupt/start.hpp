#pragma once

#include <grevir/interrupt/install.hpp>
#include <grevir/interrupt/binding.hpp>
#include <grevir/event/queue.hpp>

namespace grevir::interrupt {

enum class SetupOutcome {
  success, event_context_failed, configuration_failed, registration_failed,
  pending_policy_failed, cleanup_failed, in_progress
};

enum class CallDisposition { initiated, waited, replayed, in_progress };

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
      if constexpr (DeferredContextPlan<Spec>::selected) {
        if (!event::prepare<Spec>()) {
          return fail(SetupOutcome::event_context_failed, false);
        }
      }
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
  static StartResult fail(SetupOutcome reason, bool queue_prepared = true) noexcept {
    Board::mask_owned();
    if constexpr (DeferredContextPlan<Spec>::selected) {
      if (queue_prepared) { event::stop<Spec>(); }
    }
    if (!Board::cleanup()) {
      return {SetupOutcome::cleanup_failed, reason, CallDisposition::initiated};
    }
    return {reason, reason, CallDisposition::initiated};
  }
};

} // namespace grevir::interrupt
