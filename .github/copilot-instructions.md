# Copilot Instructions for Emiuet (EUB-04)

You are assisting development of **Emiuet**, a guitar-oriented fretboard-style MIDI controller.
This project has intentional constraints. Do not generalize it into a generic MIDI keyboard.

## Source of Truth (Priority Order)

1. `/docs/` (design intent, decisions, constraints)
2. Existing `/firmware/` code behavior (do not change observable behavior casually)
3. `/hardware/` schematics and PCB design (electrical requirements)
4. `README.md` (project overview)

If sources conflict, prefer `/docs/` and existing code behavior.
Ask for clarification only if a change would affect musical response or power stability.

## Non-Goals (Do NOT do these)

- Do NOT add **DIN MIDI** support.
- Do NOT make pitch bend **symmetrical** (no downward bend).
- Do NOT convert the design into a **piano-style** controller or layout.
- Do NOT introduce complex on-device menus or configuration UI on the OLED.
- Do NOT “optimize” by removing hardware safety/stability requirements.

## Core Musical Constraints (Must Preserve)

- **Pitch bend** is a musical constraint:
  - Upward-only
  - Fixed range
  - Predictable reset-to-center behavior
- **MPE toggle mode** exists and must remain supported:
  - String/row-based channel separation
  - Pitch bend applies only to the most recently active string in MPE mode
  - Do not “upgrade” this into full generalized MPE without explicit direction

## Hardware / Electrical Constraints (Must Not Be Simplified)

- TRS MIDI (Type-A) output must remain **MIDI-spec compliant** and must not be driven directly by 3.3V logic.
- OLED rendering assumes **u8g2**. Do not replace display libraries.
- Boot and power stability requirements in the hardware design are not optional.
  Do not propose replacing them with software workarounds.

## Preferred Engineering Style

- Preserve behavior first; refactor second.
- Use explicit state handling and clear naming.
- Keep functions small and purpose-specific.
- Avoid magic numbers; define constants with intent and comments.
- Comments should explain **why**, especially for constraints.

## When Proposing Changes

Include:
- What observable behavior changes (if any)
- Why it does not violate musical constraints
- How it preserves timing stability (input scan, MIDI output, UI)

Avoid broad rewrites. Prefer minimal diffs.

## ESP-IDF Command Execution (IMPORTANT)

- Do NOT attempt to run ESP-IDF commands from non-ESP-IDF shells.
- Do NOT ask to run `idf.py build/flash/monitor`.
- Assume the user will run ESP-IDF commands in the ESP-IDF terminal and paste logs when needed.
- When a build/flash/monitor step is required for validation, request only the minimal command and the relevant output sections.
