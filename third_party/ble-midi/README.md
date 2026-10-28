# BLE MIDI 1.0 packet codec

Unmodified source-only codec from the official Microsoft MIDI project:

- Upstream commit: `f45a7c4c8ba24c00efa45d5df3a61ed93a307cea`
- Source: https://github.com/microsoft/MIDI/blob/f45a7c4c8ba24c00efa45d5df3a61ed93a307cea/src/in-box/Transport/BluetoothMidiTransport/midi_ble_midi1_codec.h
- License: MIT, retained in LICENSE.txt and the source header.

FengYin uses only this packet decoder and sender-clock correlator with Windows' existing BLE GATT APIs. It does not install the preview Windows MIDI Services transport, drivers or background services. `BleMidiInputDecoder.h` validates notifications and emits complete short performance messages; SysEx is skipped with bounded state.
