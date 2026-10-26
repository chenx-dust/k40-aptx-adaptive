/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace bluetooth::k40 {
// Qualcomm CAF bta/av/bta_av_int.h and bta_av_vendor_offload_start.
// Non-TWS Adaptive, Slimbus, single stream. Not the Android 0xFD5D ABI.
constexpr uint16_t kOpcode = 0xfc0a;
constexpr uint8_t kStart = 0x0a;
constexpr uint8_t kStop = 0x05;
// CAF passes param_len=21+AVDT_CODEC_SIZE (66), although its temporary
// buffer contains one more padding byte. Preserve that actual wire ABI.
using StartPacket = std::array<uint8_t, 66>;
// CAF uses a zero-initialized SCB role for ordinary (non-TWS+) headphones.
// This byte is not the PCM stereo channel count.
constexpr std::array<uint8_t, 6> kHighQualityMode{0x13, 1, 5, 2, 0, 0x10};
inline uint16_t TransportMtu(uint16_t stream_mtu, bool edr, bool edr_3mbps) {
  uint16_t payload = stream_mtu > 996 ? 996 : stream_mtu;
  if (edr && !edr_3mbps && payload > 663) payload = 663;
  return payload == 0 ? 0 : payload + 12;
}
inline std::optional<StartPacket> BuildStart(
        uint32_t rate, uint16_t acl, uint16_t cid, uint16_t mtu,
        uint8_t cp_active, uint8_t cp_flag, const uint8_t* codec, size_t codec_size) {
  if ((rate != 48000 && rate != 96000) || acl > 0x0eff || cid < 0x40 ||
      mtu < 12 || codec_size != 45 || codec == nullptr || codec[0] < 9 ||
      static_cast<size_t>(codec[0]) + 1 > 44 || codec[44] != 0 || cp_active > 1) return std::nullopt;
  StartPacket out{};
  size_t n = 0;
  auto byte = [&](uint8_t value) { out[n++] = value; };
  auto word = [&](uint16_t value) { byte(value & 0xff); byte(value >> 8); };
  byte(kStart); byte(10); byte(0); byte(10); // codec, Slimbus, stream type
  byte(0); byte(0); byte(0); // device, max latency, delay reporting
  byte(cp_active); byte(cp_flag);
  word(rate == 48000 ? 2 : 8);
  word(acl); word(cid); word(mtu);
  byte(1); byte(0); byte(0); // start, no split ACL, default non-TWS+ role
  word(150); // CAF time to play
  for (size_t i = 0; i < 44; ++i) byte(codec[i]);
  return out;
}
} // namespace bluetooth::k40
