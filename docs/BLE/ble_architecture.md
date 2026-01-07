# BLE_ARCHITECTURE.md
# Emiuet BLE Architecture

This document defines the BLE architecture of Emiuet.
It is normative for firmware + app implementation.

## 0. Goals

Emiuet is a performance instrument.
BLE integration MUST NOT compromise musical stability.

Primary app integration goals (priority order):
1) OTA firmware update and configuration
2) Real-time performance GUI (key state + analysis on app)
3) Real-time harmony sharing: app -> Emiuet mapping updates (with future device-to-device sync)

## 1. Roles

- Emiuet is ALWAYS a BLE Peripheral.
- External devices (PC, iPhone, etc.) are BLE Centrals.

### Central roles (logical)
- MIDI Host Central:
  - Uses the BLE MIDI Service for MIDI I/O.
  - MUST be exclusive (only one MIDI Host Central at a time).
- App Central:
  - Uses Emiuet Custom Services (Telemetry / Control / DFU).
  - MUST be exclusive (only one App Central at a time).

## 2. Concurrent connections

### Policy
- Emiuet MUST support up to 2 concurrent Central connections:
  - 1 x MIDI Host Central
  - 1 x App Central

- Emiuet MUST NOT support multiple App Centrals concurrently.
- Emiuet MUST NOT allow App Central to access BLE MIDI characteristics.

Rationale:
- Protect stability and bandwidth for 60 Hz telemetry and musical I/O.

## 3. Services

Emiuet exposes the following services:

### 3.1 BLE MIDI Service (standard)
- Purpose: MIDI transport for DAW / PC / compatible hosts.
- Access: MIDI Host Central only.
- Concurrency: exclusive.
- Priority: musical transport priority aligns with existing policy:
  TRS > USB = BLE (simultaneous output allowed).

### 3.2 Telemetry Service (custom)
- Purpose: real-time performance state to App Central.
- Access: App Central only.
- Mode: Notify-only (primarily).
- Update:
  - Active: 60 Hz target (16.67 ms period)
  - Idle: reduced rate (default 10 Hz) or event-driven low rate
- Backpressure:
  - Telemetry MUST be "latest-wins".
  - Telemetry MUST NOT build unbounded queues.
  - If notifications are delayed, older frames MUST be dropped.

### 3.3 Control Service (custom)
- Purpose: app -> device control for configuration and real-time harmony context.
- Access: App Central only.
- Mode: Write (with response where required for correctness).
- Control commands MUST be transactional using epoch + COMMIT (see spec).

### 3.4 DFU/OTA Service (custom)
- Purpose: firmware update over BLE.
- Access: App Central only.
- Safety:
  - OTA MUST require explicit confirmation in the app.
  - During OTA mode, Emiuet MUST stop all performance functions:
    - Stop MIDI I/O
    - Stop Telemetry
    - Stop Control (except DFU)
  - After OTA completion, Emiuet MUST reboot into normal mode.

## 4. Advertising strategy

- Emiuet SHOULD advertise Custom Services in normal operation (Telemetry/Control).
- Emiuet MAY advertise BLE MIDI Service concurrently, but the implementation MUST enforce:
  - App Central cannot use BLE MIDI characteristics.
  - Only one MIDI Host Central is allowed.

Optional hardening:
- Emiuet MAY use a "MIDI connect window" concept:
  - Strongly advertise BLE MIDI only when no MIDI Host is connected
  - Or when user explicitly enables MIDI pairing mode
This is allowed if needed for stability across host OS behavior.

## 5. Timing and timestamps

Telemetry frames MUST include:
- A device-local monotonic timestamp (t_ms or t_us)
- A sequence counter (seq) for loss detection

Notes:
- The timestamp is NOT synchronized across devices by default.
- Future synchronized performance (Emiuet-to-Emiuet) will require a separate time sync mechanism.
- Keeping device-local timestamp now is REQUIRED to enable future sync designs.

## 6. Non-blocking policy alignment

The firmware transport policy applies to BLE:
- Musical generation / input scanning MUST NOT block on BLE I/O.
- BLE MIDI sending MUST be performed by a dedicated sender task/loop.
- Telemetry is "best-effort" and MUST yield to musical timing constraints.

## 7. Failure modes

- If App Central disconnects:
  - Emiuet MUST continue to operate as an instrument without degradation.
  - No musical logic may depend on app presence.

- If MIDI Host disconnects:
  - Emiuet SHOULD resume advertising availability for MIDI host connection.
  - No UI-heavy operations should run in tight loops due to disconnection.

End of document.