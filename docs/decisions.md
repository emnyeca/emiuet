# Emiuet – Design Decisions

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
