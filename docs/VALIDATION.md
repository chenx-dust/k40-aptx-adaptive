# Protocol changes and tests

## Version 0.5

- Uses the CAF single-VSC start packet's 66-byte wire length. Version 0.4 still received controller error `0x12` after the length fix alone.
- Sets the non-TWS+ role field to 0, independently of the PCM stereo enumeration.
- Caps non-multicast payload MTU at 996 bytes, or 663 bytes for EDR without 3 Mbps support, then adds the 12-byte media header.
- Queues HQ mode command `13 01 05 02 00 10` after the start command.
- Reports start success from controller ACKs and handles rejection, short responses, late responses and stop transitions.

## Tests

- Android components compile, with AIDL host tests passing on both 32-bit and 64-bit hosts.
- Differential tests execute CAF serialization statements across 72 start-packet input combinations and compare the HQ command separately.
- Bundled host tests cover configuration conversion, the ACK state machine, controller packets, module activation and rollback, and archive content identity.
- Version 0.5 boots and plays Adaptive audio on the target setup.

Additional device regression coverage: pause/resume, reconnection, extended playback, 96 kHz playback and switching to other codecs.
