#pragma once

#include <grevir/interrupt/probe_record.hpp>

#if defined(GREVIR_IRQ_PROBE)

#if defined(_MSC_VER)
#pragma section(".grevir_irq_plan", read)
#define GREVIR_IRQ_SECTION __declspec(allocate(".grevir_irq_plan"))
#elif defined(__APPLE__)
#define GREVIR_IRQ_SECTION __attribute__((used, section("__DATA,__grevir_irq")))
#else
#define GREVIR_IRQ_SECTION __attribute__((used, section(".grevir_irq_plan")))
#endif

#define GREVIR_EMIT_IRQ_PROBE(Spec) \
  extern "C" GREVIR_IRQ_SECTION constinit const auto grevir_irq_plan = \
    ::grevir::interrupt::encode_probe_record<Spec>()

#endif
