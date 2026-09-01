# Historical: Rev.A and superseded decisions

> battery、dual USB、TUSB320削除を含む旧案の履歴です。現行の正本は
> `../decisions.md` です。

This document records major design decisions made during the development
of Emiuet, including rejected alternatives and their rationale.

It exists to preserve context, not to justify correctness.

Future changes are allowed, but should be made with awareness
of these decisions and their original intent.

---

## 1. MIDI Output Strategy

### Decision
DIN MIDI OUT was removed from the design.
MIDI output is provided via USB, BLE, and TRS MIDI (Type-A).

### Context
DIN connectors increase enclosure thickness and mechanical complexity.
The project prioritizes a slim, instrument-like form factor.

### Alternatives Considered
- Retaining 5-pin DIN MIDI OUT
- Supporting both DIN and TRS outputs

### Reasoning
While DIN MIDI offers broad compatibility, its mechanical and spatial
requirements conflicted with the enclosure and design goals.
TRS MIDI preserves electrical compatibility while enabling a thinner design.

### Consequences
- Additional circuitry is required for TRS MIDI compliance
- Some legacy equipment may require adapters

Firmware transport policy (instrument-first):
- The performance-critical path must never block on transport I/O; MIDI generation only enqueues (0-wait) and dedicated sender tasks perform all I/O.
- Realtime priority is TRS > USB = BLE; simultaneous output is allowed and no automatic fallback behavior is assumed.
- USB targets drop-free delivery in normal operation by using a large discrete-event queue (default 1024) plus per-channel coalescing for continuous controls.
- Pitch Bend and CC#1 are treated as continuous and are coalesced per channel; discrete events (e.g., Note On/Off) preserve ordering.
- BLE-MIDI transport may be stubbed during development and should remain disabled in routing until the real transport is implemented.

---

## 2. Pitch Bend Direction and Range

### Decision
Pitch bend is limited to a single direction with a fixed range.

### Context
The controller is designed for guitarists.
Pitch gestures are intended to resemble string bending.

### Alternatives Considered
- Symmetrical pitch bend (up/down)
- User-configurable pitch bend range
- MPE-style continuous pitch control per note

### Reasoning
Allowing downward or symmetrical pitch bend introduced ambiguity
and reduced predictability during performance.
A fixed, upward-only bend provides consistent musical intent.

### Consequences
- Reduced configurability
- Increased reliability and repeatability in performance

---

## 3. MPE Toggle Mode Design

### Decision
An optional, toggleable MPE-style mode was adopted,
with string-based channel separation and constrained pitch expression.

### Context
Guitar performance often involves localized pitch expression
on a single string, rather than global pitch modulation.

At the same time, full MPE implementations introduce
significant complexity in channel management and performance predictability,
especially in jam or ensemble contexts.

### Alternatives Considered
- No MPE support
- Fully generalized MPE (per-note multidimensional control)
- Software-only per-note pitch bend without channel separation

### Reasoning
A constrained MPE approach was chosen to balance expressiveness
with reliability and musical clarity.

By assigning each string to its own channel while limiting pitch bend
to a single, recently active string, the system preserves
a guitar-like mental model without overwhelming the performer
or the MIDI environment.

### Consequences
- Compatible with MPE-capable receivers in a limited, predictable way
- Reduced configuration complexity compared to full MPE
- Clear distinction between normal mode and expressive mode

---

## 4. Key Matrix Size (6 × 13)

### Decision
The key matrix size was fixed at 6 rows × 13 columns.

### Context
The layout is designed to support guitar-style chord forms,
especially low-position barre and root-based voicings.

### Alternatives Considered
- 6 × 12 layout
- Reduced horizontal range with octave-based shifting
- Piano-style keyboard groupings

### Reasoning
Thirteen columns were chosen to allow familiar guitar chord forms
to be played without compromise.

For example, common low-position forms such as E-root and A-root chords
can be expressed naturally across strings without forced omission.

This enables voicings such as:
- E major shapes spanning all six strings
- A major seventh shapes with string-specific omissions

The goal was not numeric symmetry, but preservation of guitar-centric
muscle memory and harmonic thinking.

### Consequences
- Increased PCB width
- Higher routing and scanning complexity
- Improved expressiveness for guitar-derived voicings

---

## 5. Display Scope Limitation

### Decision
The OLED display is intentionally limited in scope.

### Context
The device includes Bluetooth connectivity,
allowing external devices to provide richer user interfaces if needed.

### Alternatives Considered
- Menu-driven on-device UI
- Parameter editing via the OLED
- High-information-density displays

### Reasoning
Limiting the on-device display reduces visual distraction during performance
and simplifies firmware responsibilities.

More complex UI interactions are intentionally deferred to external devices,
where richer interaction models are more appropriate.

This separation allowed the on-device display to remain minimal
without sacrificing future expandability.

### Consequences
- Minimal on-device configuration
- Simpler firmware and UI logic
- Clear separation between performance feedback and configuration

---

## 6. Power Architecture Choice

### Decision
A power-path charging architecture was adopted.

### Context
The device is expected to be used while charging
and in environments with variable USB power quality.

### Alternatives Considered
- Simple battery charger without power-path
- USB-powered operation only

### Reasoning
Separating system power from charging behavior
improves reliability and avoids performance disruption.

### Consequences
- Increased hardware complexity
- Improved stability during live use

---

## 7. What Was Explicitly Not Pursued

- General-purpose MIDI controller features
- Piano-style keyboard abstractions
- Feature parity with commercial MIDI keyboards
- Extensive on-device configuration menus

---

## 8. Standalone Product and Power Boundary

### Decision
Emiuet remains a self-contained MIDI keyboard and performance instrument.
Battery charging, power-path control, and all rails required for normal
operation are implemented within Emiuet.

Hearth is not a required external power unit, and EUB-BUS is not a required
Emiuet product interface.

### Context
Rev.A combined all subsystems on one board, which made individual failures
hard to isolate. Power therefore needs independent validation before Rev.B,
but separating validation hardware must not turn that temporary test boundary
into a permanent product dependency.

### Consequences
- Rev.B must operate from its own battery and USB power architecture.
- The Emiuet validation project includes a dedicated internal-power test board.
- Hearth may be used as an optional comparison source or protected bench supply.
- A validation setup using Hearth must also be repeated with Emiuet's own power
  circuit before the corresponding Rev.B gate can pass.

---

## 9. Auxiliary USB HID Keyboard Mode

### Decision

Emiuet formally supports MIDI Mode and an auxiliary USB HID Keyboard Mode
(`TYPE`). USB-MIDI and HID Keyboard remain present in one composite USB Device;
the input mode selects which event path is active without re-enumerating USB.

### Context

The 78-key matrix can cover ordinary text and shortcut input. This reduces the
need to keep another keyboard beside Emiuet, but does not change the product's
primary identity or guitar-first ergonomics.

### Consequences

- TYPE is limited to practical auxiliary text/navigation input and a single Fn layer.
- The four physical matrix corners held for 2 seconds switch modes.
- Every transition releases tracked notes/keys and resets pending transport state.
- Composite Device mode is firmware-only on Rev.A.
- USB role switching is not required by this feature.

---

## 10. Rev.B Fixed USB Device Role

### Decision

Emiuet Rev.B uses a fixed USB 2.0 Device / UFP role on USB-C #2. USB Host,
DRP/OTG role switching, Host VBUS sourcing, and runtime Device/Host transitions
are intentionally excluded from the initial product scope. MIDI and TYPE change
only the internal event route; the persistent USB MIDI + HID configuration does
not change or re-enumerate.

USB-C #1 remains the charging and power input. USB-C #2 VBUS is a protected
presence-sense input for the self-powered USB Device and is not a product power
input or an output from Emiuet.

### Context

USB Host is not part of Emiuet's central musical purpose. USB MIDI Device, TRS
MIDI OUT, and BLE-MIDI already provide the intended output paths. Standalone
USB routing may use an external USB MIDI host/router, but no particular router
is an Emiuet dependency. The ESP32-S3 stack-transition burden and Rev.A's
incomplete role/VBUS control create validation cost without enough product
value.

This follows Emiuet's preference for musical intent and reliability over
feature breadth.

### Rev.A evidence

- U5 TUSB320 has `PORT` unconnected (DRP), while `ID` and `INT_N` are not routed
  to the ESP32-S3.
- The Rev.A netlist connects internal `+5V` through U3 LM66100 to `VBUS2`, then
  through FB1/F2 to USB-C #2 VBUS. This is an outward VBUS source path, not a
  USB-C #2 input powering Emiuet.
- The current firmware contains only a USB Device stack; there is no Host stack,
  role detector, role state machine, or Host-specific Kconfig option to remove.

### Rev.B hardware delta

| Component / circuit | Rev.A purpose | Device-only action | Reason |
|---|---|---|---|
| USB-C #2 connector (J2) | Reversible USB 2.0 data/power connector | **KEEP** | The physical data port remains, but only as Device/UFP. |
| TUSB320 (U5) | DRP CC-role detection and VBUS detection | **REMOVE** | Fixed UFP needs no role negotiation. |
| CC1 / CC2 | Connected to TUSB320 | **REPLACE** | Fit one USB-C `Rd` on each CC pin for a fixed Sink/UFP. |
| LM66100 (U3) | Gates internal +5V toward USB-C #2 VBUS | **REMOVE** from USB-C #2 | Device-only Emiuet must not source Host VBUS. |
| Internal 5V boost branch to U3 | Supplies the Rev.A Host VBUS path | **REMOVE** | Keep the 5V boost for internal loads, but remove its USB-C #2 branch. |
| USB-C #2 VBUS path | Host-source path plus TUSB320 detection | **REPLACE** | Use protected VBUS presence sensing only; do not feed system power. |
| VBUS monitor | TUSB320 RC input only | **REPLACE** | Route protected VBUS through a validated divider/comparator to a free ESP32-S3 GPIO for self-powered detach detection. GPIO42 is reserved as the current Rev.B candidate. |
| VBUS TVS (D1) | VBUS transient protection | **KEEP** | VBUS remains exposed even when used only for sensing. Revalidate voltage/current ratings. |
| VBUS PTC fuse (F2) | Protects the Host VBUS source path | **REVIEW** | The high-current source disappears; replace with only the series protection required by the final sense circuit. |
| VBUS ferrite (FB1) | Filters the Rev.A sourced VBUS path | **REVIEW** | Retain only if the final presence-sense EMC/protection analysis requires it. |
| USB data ESD (U2) | Protects D+/D- | **KEEP** | Independent of USB role. |
| USB common-mode choke (L1) | D+/D- EMI filtering | **REVIEW** | Keep only if USB full-speed signal-integrity/EMI measurements justify it. |

The checked-in KiCad schematic, PCB, BOM, and PCBA exports describe Rev.A and
are retained as historical manufacturing data. They are not silently rewritten
into Rev.B before the VBUS divider/comparator, protection values, placement,
and routing are reviewed together. Rev.B therefore requires schematic, BOM,
PCB routing, and regenerated PCBA output changes before fabrication.

### Consequences

- Firmware remains one persistent USB MIDI + HID Composite Device.
- Host validation is removed rather than recorded as incomplete.
- Rev.B must enable the self-powered VBUS-monitor firmware option after its
  physical monitor circuit is populated.
- USB-C #1 + #2 simultaneous connection, reverse current, physical detach, and
  real-host enumeration remain mandatory hardware validations.
- The prototype `303A:4005` VID/PID remains a release/compliance issue; an
  authorized production/public-distribution policy is required, but this is
  not a PCB blocker.
- BLE-MIDI remains in scope and its existing transport stub is not expanded by
  this decision.
