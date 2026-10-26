#include "stream_ack.h"
#include <cassert>
#include <future>
using namespace std::chrono_literals;

int main() {
  k40::StreamAck ack;
  // Immediate callbacks must be accepted before Wait begins.
  assert(ack.Begin(true));
  ack.Reply(true, true);
  assert(ack.Wait(0ms));
  // Wrong-direction replies cannot satisfy suspend; timeout quarantines.
  assert(ack.Begin(false));
  ack.Reply(true, true);
  assert(!ack.Wait(0ms));
  ack.Reply(false, true);
  assert(!ack.Begin(false));
  ack.SessionChanged();
  assert(ack.Begin(false));
  auto waiter = std::async(std::launch::async, [&] { return ack.Wait(1000ms); });
  ack.Reply(false, true);
  assert(waiter.get());
  // A disconnect cancels a pending operation even if a stale ACK follows.
  assert(ack.Begin(true));
  ack.SessionChanged();
  ack.Reply(true, true);
  assert(!ack.Wait(0ms));
  assert(ack.Begin(true));
  ack.Reject();
  assert(!ack.Wait(0ms));
  assert(!ack.Begin(true));
  ack.SessionChanged();
  assert(ack.Begin(true));
  ack.Reply(true, false);
  assert(!ack.Wait(0ms));
  assert(ack.Begin(true));
  assert(!ack.Begin(false));
  ack.Reply(true, true);
  assert(ack.Wait(0ms));
}
