# KiCad source status

- `Emiuet.kicad_sch`: Rev.B current schematic draft (KiCad 10 format)
- `Emiuet_RevA.kicad_sch`: Rev.A historical schematic
- `Emiuet.kicad_pcb`: Rev.A historical PCB layout; intentionally untouched in this task

Rev.B schematic and Rev.A PCB do not have matching connectivity. Do not run PCB update from
schematic against the historical board. Create the Rev.B placement/routing as a separate PCB redesign.

The Rev.B draft makes the one-port UFP/power path, TUSB320, ESP32-S3 GPIO nets,
3.3 V regulator candidate, RGB level shifter, 78-pixel serpentine data chain,
bulk/local bypass intent, TRS MIDI IN/OUT, and the matrix/UI buses reviewable. It is
an architecture schematic, not a production release: protection, regulator, MIDI
interface details and passive values still require component selection and ERC cleanup.
