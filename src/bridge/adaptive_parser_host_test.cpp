#include "stack/include/a2dp_vendor_aptx_adaptive.h"
#include <array>
#include <cassert>

// Host-only logging substitute. No codec/session implementation is mocked.
extern "C" int __android_log_print(int, const char*, const char*, ...) { return 0; }

int main() {
  std::array<uint8_t, 43> wire{};
  wire[0] = 42;
  wire[2] = 0xff;
  wire[3] = 0xd7;
  wire[7] = 0xad;
  wire[9] = 0x10;
  wire[10] = 0x08;
  wire[11] = 0x50;
  wire[12] = 0x64;
  wire[16] = 0xff;
  wire[18] = 1;
  wire[19] = 0x12;
  wire[20] = 0x34;
  wire[21] = 0x56;
  wire[22] = 0x78;
  wire[28] = 0xaa;
  tA2DP_APTX_ADAPTIVE_CIE parsed{};
  assert(A2DP_GetAptxAdaptiveCIE(wire.data(), &parsed));
  assert(parsed.aptx_data.ttp_ll_0 == 0x50);
  assert(parsed.aptx_data.ttp_tws_1 == 0xff);
  assert(parsed.aptx_data.aptx_adaptive_sup_features == 0x78563412);
  assert(parsed.aptx_data.eoc1 == 0xaa);
  assert(A2DP_VendorGetTrackSampleRateAptxAdaptive(wire.data()) == 48000);
  assert(A2DP_VendorGetTrackBitsPerSampleAptxAdaptive(wire.data()) == 24);
  wire[9] = 0xa0;
  assert(A2DP_GetAptxAdaptiveCIE(wire.data(), &parsed));
  assert(A2DP_VendorGetTrackSampleRateAptxAdaptive(wire.data()) == 96000);
  wire[9] = 0x30;
  assert(!A2DP_GetAptxAdaptiveCIE(wire.data(), &parsed));
  wire[9] = 0x10;
  wire[10] = 0x09;
  assert(!A2DP_GetAptxAdaptiveCIE(wire.data(), &parsed));
  wire[10] = 0x08;
  wire[7] = 0x24;
  assert(!A2DP_GetAptxAdaptiveCIE(wire.data(), &parsed));
  wire[7] = 0xad;
  wire[0] = 19;
  assert(!A2DP_GetAptxAdaptiveCIE(wire.data(), &parsed));
}
