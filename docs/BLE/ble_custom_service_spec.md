# BLE_CUSTOM_SERVICE_SPEC.md
# Emiuet Custom BLE Services Specification

This document specifies custom services used by the Emiuet companion app.
It is normative for both firmware and app.

Conventions:
- MUST / MUST NOT / SHOULD are used in the RFC sense.
- All multibyte fields are little-endian unless stated otherwise.

## 1. Overview

Custom services are split by responsibility:

1) Telemetry Service
   - Device -> App, real-time status frames (60 Hz active)
2) Control Service
   - App -> Device, configuration + harmony context updates
3) DFU/OTA Service
   - App -> Device, firmware update (device stops all performance functions during OTA)

App Central is the only allowed client for these services.
Only one App Central connection is allowed concurrently.

## 2. Telemetry Service

### 2.1 Purpose
Provide a real-time view of performance state for GUI:
- key press visualization (6x13)
- mode indicators (octave, MPE, etc.)
- optional slider states (PB/Mod/Vel)
- allow app-side chord/harmony analysis

### 2.2 Characteristics
This spec uses placeholder UUID names. Replace with actual UUIDs later.

- TELEMETRY_FRAME (Notify)
  - Properties: Notify
  - CCCD: required

### 2.3 Frame rate policy
- Active mode: 60 Hz target.
- Idle mode: reduced rate (default 10 Hz) OR event-driven low rate.
- Transition rules:
  - Active mode SHOULD engage when key state changes or recent activity is detected.
  - Idle mode SHOULD engage after a quiet period (suggested 1.0 s, adjustable).

### 2.4 Backpressure policy
- Telemetry MUST be latest-wins.
- Firmware MUST NOT buffer more than 1 pending telemetry frame per connection.
- If BLE notify cannot send in time, older frames MUST be dropped.
- The app MUST tolerate missing frames and use `seq` to detect gaps.

### 2.5 Telemetry frame format (v1)
All fields are REQUIRED unless marked optional.

TelemetryFrameV1:
- u8  version        : 0x01
- u8  seq            : increments each emitted frame (wrap allowed)
- u32 t_ms           : monotonic device-local milliseconds since boot (wrap allowed)
- u8  octave         : signed offset encoded as two's complement (e.g. -2..+2 typical)
- u8  mode_flags     : bitfield (see below)
- u8  keys_len_bytes : MUST be 10 for v1 (78 bits packed)
- u8  keys_bitmap[10]: packed key state bits (LSB-first within each byte)
- [optional] sliders block:
  - u8  sliders_flags
  - u16 pb   (normalized)
  - u16 mod  (normalized)
  - u16 vel  (normalized)

Mode flags (mode_flags):
- bit0: MPE enabled
- bit1: reserved
- bit2: reserved
- bit3: reserved
- bit4..7: reserved for future

Keys bitmap packing:
- 78 keys are packed into 10 bytes (80 bits total).
- Bits 78..79 MUST be zero.
- The mapping of (string,row,col) to bit index MUST be defined in firmware + app consistently.
  Recommendation:
  - bit_index = row * 13 + col
  - row = 0..5 (String 1..6)
  - col = 0..12 (Fret 0..12)
If the physical mapping differs, define a mapping table in a dedicated doc.

### 2.6 App-side chord analysis
- The device MUST NOT compute chord names for Telemetry v1.
- The app SHOULD compute chord/harmony from keys_bitmap plus known tuning/mapping state.

## 3. Control Service

### 3.1 Purpose
Provide controlled, transactional updates from app to device:
- configuration changes
- harmony context updates for real-time key mapping

### 3.2 Characteristics
- CONTROL_CMD (Write / Write With Response depending on command)
- CONTROL_EVT (Notify) optional, for acknowledgements and state changes

### 3.3 Transaction model: epoch + COMMIT (REQUIRED)

Rationale:
- Prevent partial application and timing jitter.
- Enable future timed commits using timestamps (e.g., commit at a musical boundary).

Definitions:
- epoch: u16 or u32 chosen by firmware, increments per transaction
- staged state: data received but not yet applied to active performance state

Rules:
1) App sends one or more SET_* commands tagged with the same epoch.
2) Device stores them as "staged" under that epoch.
3) App sends COMMIT(epoch).
4) Device atomically swaps staged -> active for that epoch.

MUST requirements:
- Device MUST NOT apply staged changes before COMMIT for that epoch.
- Device MUST apply all staged changes atomically at COMMIT.
- If COMMIT is received for an unknown epoch, device MUST reject and optionally notify error.
- Device MUST discard stale staged epochs after a timeout (suggested 2 s) to avoid memory leak.

Default UX:
- App MAY send SET_* immediately followed by COMMIT to achieve "instant feel".
  This preserves the option for timed commits later.

### 3.4 Commands (v1 set)

Encoding (suggested):
- u8  version  : 0x01
- u8  cmd_id
- u16 epoch
- payload...

Command IDs (suggested):
- 0x01 SET_HARMONY_CONTEXT
- 0x02 COMMIT
- 0x03 SET_CONFIG_PARAM
- 0x04 REQUEST_DEVICE_INFO
- 0x05 ENTER_OTA_MODE

#### 3.4.1 SET_HARMONY_CONTEXT (cmd_id=0x01)
Goal: allow app to update mapping in a high-level way.

Payload (example):
- u8  root_note      : 0..11 (C..B)
- u8  scale_id       : enumerated
- u16 chord_id       : enumerated or bitmask-based
- u8  strategy_flags : mapping strategy options
- u8  reserved

Notes:
- The precise model can evolve. Keep it compact and versioned.

#### 3.4.2 COMMIT (cmd_id=0x02)
Payload:
- none (epoch is in header)

#### 3.4.3 SET_CONFIG_PARAM (cmd_id=0x03)
Payload:
- u16 param_id
- u8  value_type  : (u8,u16,i16,u32,i32,f32,bytes)
- value...

The device SHOULD expose a parameter table for forward compatibility.

#### 3.4.4 ENTER_OTA_MODE (cmd_id=0x05)
- MUST require app confirmation UX before sending.
- When accepted, device MUST:
  - stop all performance services and MIDI output
  - switch to OTA/DFU mode
  - notify state change if CONTROL_EVT exists

## 4. DFU/OTA Service

This document defines behavioral requirements, not the full DFU protocol.

### 4.1 OTA entry and exclusivity
- OTA MUST be initiated by App Central.
- OTA MUST require explicit in-app confirmation.
- During OTA:
  - BLE MIDI MUST be stopped
  - Telemetry MUST be stopped
  - Control MUST be stopped except for DFU operations

### 4.2 Post-OTA
- On success: device MUST reboot.
- On failure: device MUST provide a safe recovery path.
  - Minimum requirement: allow USB-based recovery in development.
  - Future requirement: robust A/B or bootloader strategy (out of scope here).

## 5. Versioning and compatibility

- Telemetry frames and control commands MUST include a version field.
- Breaking changes MUST bump version.
- Unknown fields SHOULD be ignored by the receiver where possible.

End of document.