#pragma once

#include <grevir/interrupt/binding.hpp>

#if defined(GREVIR_IRQ_PROBE)
namespace grevir::interrupt {

template <std::size_t N>
struct ProbeRecord {
  unsigned char bytes[N]{};
};

constexpr std::size_t encoded_text_size(std::string_view value) {
  return 1 + value.size();
}

template <class Spec>
consteval std::size_t probe_record_size() {
  constexpr auto demands = BindingPlan<Spec>::demands;
  constexpr auto selected = BindingPlan<Spec>::selected;
  // Magic, schema, total length, demand and binding counts, then checksum.
  std::size_t size = 4 + 2 + 4 + 2 + 2 + 4;
  size += encoded_text_size(Spec::Board::backend);
  size += encoded_text_size(Spec::Board::target);
  size += encoded_text_size(Spec::Board::board);
  size += encoded_text_size(Spec::Board::compiler);
  size += encoded_text_size(Spec::Board::application);
  for (std::size_t i = 0; i < demands.count; ++i) {
    const auto& event = demands.keys[i];
    size += encoded_text_size(event.instance) + encoded_text_size(event.request)
      + encoded_text_size(event.kind);
  }
  for (std::size_t i = 0; i < selected.count; ++i) {
    const auto& binding = selected.bindings[i];
    size += encoded_text_size(binding.event.instance)
      + encoded_text_size(binding.event.request)
      + encoded_text_size(binding.event.kind)
      + encoded_text_size(binding.owner)
      + encoded_text_size(binding.configuration)
      + encoded_text_size(binding.source)
      + encoded_text_size(binding.selector)
      + encoded_text_size(binding.entry)
      + encoded_text_size(binding.snapshot_policy)
      + encoded_text_size(binding.acknowledge_policy) + 3;
  }
  return size;
}

template <class Spec>
consteval auto encode_probe_record() {
  constexpr auto demands = BindingPlan<Spec>::demands;
  constexpr auto selected = BindingPlan<Spec>::selected;
  constexpr auto length = probe_record_size<Spec>();
  static_assert(length <= 65535, "GREVIR_IRQ_PLAN_TOO_LARGE");
  ProbeRecord<length> output{};
  std::size_t cursor = 0;
  const auto byte = [&](unsigned value) {
    output.bytes[cursor++] = static_cast<unsigned char>(value & 0xffu);
  };
  const auto word = [&](unsigned value) {
    byte(value);
    byte(value >> 8u);
  };
  const auto dword = [&](unsigned long value) {
    byte(static_cast<unsigned>(value));
    byte(static_cast<unsigned>(value >> 8u));
    byte(static_cast<unsigned>(value >> 16u));
    byte(static_cast<unsigned>(value >> 24u));
  };
  const auto string = [&](std::string_view value) {
    if (value.size() > 255) { while (true) {} }
    byte(static_cast<unsigned>(value.size()));
    for (char character : value) { byte(static_cast<unsigned char>(character)); }
  };
  byte('G'); byte('I'); byte('R'); byte('Q');
  word(1); dword(static_cast<unsigned long>(length));
  word(static_cast<unsigned>(demands.count));
  word(static_cast<unsigned>(selected.count));
  string(Spec::Board::backend);
  string(Spec::Board::target);
  string(Spec::Board::board);
  string(Spec::Board::compiler);
  string(Spec::Board::application);
  for (std::size_t i = 0; i < demands.count; ++i) {
    const auto& event = demands.keys[i];
    string(event.instance);
    string(event.request);
    string(event.kind);
  }
  for (std::size_t i = 0; i < selected.count; ++i) {
    const auto& binding = selected.bindings[i];
    string(binding.event.instance);
    string(binding.event.request);
    string(binding.event.kind);
    string(binding.owner);
    string(binding.configuration);
    string(binding.source);
    string(binding.selector);
    string(binding.entry);
    string(binding.snapshot_policy);
    string(binding.acknowledge_policy);
    word(binding.dispatch_order);
    byte(binding.shared_source ? 1u : 0u);
  }
  unsigned long checksum = 2166136261ul;
  for (std::size_t i = 0; i < cursor; ++i) {
    checksum ^= output.bytes[i];
    checksum *= 16777619ul;
  }
  dword(checksum);
  if (cursor != length) { while (true) {} }
  return output;
}

} // namespace grevir::interrupt
#endif
