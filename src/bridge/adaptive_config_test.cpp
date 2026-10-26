#include "adaptive_config.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main() {
  // Reverse the Qualcomm AIDL producer's negotiated-byte minus 128 mapping.
  // Exhaust all byte values so the 127/128 sign boundary cannot regress.
  for (int negotiated = 0; negotiated <= 255; ++negotiated) {
    const auto aidl_value = static_cast<int8_t>(negotiated - 128);
    assert(k40::DecodeAidlTtpByte(aidl_value) == negotiated);
  }
  k40::AdaptiveParameters p{48000, 1000, 0, 24, 0x1000,
      {20, 50, 20, 50, 20, 50}, {1, 2, 3, 4, 5, 6}, 0, 255,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
  const auto converted = k40::ConvertR2(p);
  assert(converted);
  // Regression fixture for legacy ABI offsets and units established from
  // SM8250 source + donor decompilation. Not a captured hardware output.
  std::array<uint8_t, 72> expected{};
  expected[0] = 1;
  expected[4] = 0xe8; expected[5] = 3;
  for (int offset : {12, 20, 28}) expected[offset] = 20;
  for (int offset : {16, 24, 32}) expected[offset] = 50;
  expected[37] = 0x10;
  for (int i = 0; i < 6; ++i) expected[40 + i] = i + 1;
  expected[48] = 24; expected[56] = 255;
  for (int i = 0; i < 11; ++i) expected[60 + i] = i;
  assert(std::memcmp(&*converted, expected.data(), expected.size()) == 0);
  p.sample_rate_hz = 44100; assert(k40::ConvertR2(p)->sampling_rate == 2);
  p.sample_rate_hz = 96000; assert(k40::ConvertR2(p)->sampling_rate == 3);
  p.sample_rate_hz = 88200; assert(!k40::ConvertR2(p));
  p.sample_rate_hz = 48000;
  p.sink_cap.resize(10); assert(!k40::ConvertR2(p));
  p.sink_cap.resize(11); p.peer_mtu = 65536; assert(!k40::ConvertR2(p));
  p.peer_mtu = 1000; p.input_mode = 1; assert(!k40::ConvertR2(p));
  p.input_mode = 0; p.sink_buffering_ms[0] = 51; assert(!k40::ConvertR2(p));
  p.sink_buffering_ms[0] = 20; p.ttp[0] = -1; assert(!k40::ConvertR2(p));
  std::cout << "R2 legacy ABI conversion tests passed\n";
}
