#include "src/controller/qcom_adaptive_offload.h"
#include <cassert>
#include <cstdio>
int main() {
  std::array<uint8_t, 45> codec{};
  codec[0] = 42;
  codec[42] = 0x7e;
  auto packet = bluetooth::k40::BuildStart(96000, 0x123, 0x456, 1000, 1, 2, codec.data(), codec.size());
  assert(packet);
  // CAF transmitted wire length is 66, omitting one unused trailing pad byte.
  const uint8_t expected[] = {10,10,0,10,0,0,0,1,2,8,0,0x23,1,0x56,4,0xe8,3,1,0,0,150,0};
  for (unsigned i=0; i<sizeof(expected); ++i) assert((*packet)[i] == expected[i]);
  assert(packet->size() == 66 && (*packet)[64] == 0x7e && (*packet)[65] == 0);
  packet = bluetooth::k40::BuildStart(48000,0,0x40,1000,0,0,codec.data(),45);
  assert(packet && (*packet)[9] == 2); // ACL zero is valid.
  assert(!bluetooth::k40::BuildStart(44100,0,0x40,1000,0,0,codec.data(),45));
  assert(!bluetooth::k40::BuildStart(48000,0xffff,0x40,1000,0,0,codec.data(),45));
  assert(!bluetooth::k40::BuildStart(48000,0,0,1000,0,0,codec.data(),45));
  assert(!bluetooth::k40::BuildStart(48000,0,0x40,1000,0,0,codec.data(),44));
  codec[0] = 45;
  assert(!bluetooth::k40::BuildStart(48000,0,0x40,1000,0,0,codec.data(),45));
  codec[0] = 42; codec[44] = 1;
  assert(!bluetooth::k40::BuildStart(48000,0,0x40,1000,0,0,codec.data(),45));
  puts("Qualcomm packet serialization and invalid-input tests PASS");
  assert(bluetooth::k40::TransportMtu(883,true,true)==895);
  assert(bluetooth::k40::TransportMtu(883,true,false)==675);
  assert(bluetooth::k40::TransportMtu(65535,true,true)==1008);
  assert(bluetooth::k40::TransportMtu(500,true,false)==512);
  assert(bluetooth::k40::TransportMtu(0,true,true)==0);
}
