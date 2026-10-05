"""Read-only KiCad review exports. Python 3.10+, KiCad 10.0.3.

Exit 0: automated checks passed (not fabrication approval).
Exit 1: design violations. Exit 2: tool/environment failure.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kicad-cli", help="Path to KiCad 10.0.3 executable")
    parser.add_argument("--schematic", type=Path, default=ROOT / "hardware/kicad/Emiuet.kicad_sch",
                        help="Native schematic to review (including validation boards)")
    args = parser.parse_args()
    cli = args.kicad_cli or shutil.which("kicad-cli")
    if not cli and os.name == "nt":
        candidate = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "KiCad/10.0/bin/kicad-cli.exe"
        if candidate.is_file():
            cli = str(candidate)
    if not cli:
        print("UNVERIFIED: install KiCad 10.0.3 or supply --kicad-cli", file=sys.stderr)
        return 2
    cli = str(Path(cli).resolve())
    env = os.environ.copy()
    share = Path(cli).parent.parent / "share/kicad"
    if share.is_dir():
        # Libraries must come from the same installation as the verified CLI.
        env["KICAD10_SYMBOL_DIR"] = str(share / "symbols")
        env["KICAD10_FOOTPRINT_DIR"] = str(share / "footprints")
    version = subprocess.check_output([cli, "version"], text=True, env=env).strip()
    if version != "10.0.3":
        print(f"UNVERIFIED: expected KiCad 10.0.3, found {version}", file=sys.stderr)
        return 2
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    output = ROOT / "build/hardware" / stamp
    output.mkdir(parents=True, exist_ok=False)
    # Use project tables and packaged libraries, not a developer's global tables.
    env["KICAD_CONFIG_HOME"] = str(output / "kicad-config")
    source = args.schematic.resolve()
    if not source.is_file():
        print(f"UNVERIFIED: schematic not found: {source}", file=sys.stderr)
        return 2
    files = [source, source.with_suffix(".kicad_pro"),
             source.parent / "sym-lib-table", source.parent / "fp-lib-table"]
    files = [p for p in files if p.is_file()]
    before = {str(p): digest(p) for p in files}
    report = {"utc": stamp, "kicad": version, "source_sha256": before,
              "commands": [], "erc": "UNVERIFIED", "footprints": "UNVERIFIED",
              "pcb_drc": "UNVERIFIED", "schematic_pcb_parity": "UNVERIFIED",
              "physical_validation": "UNVERIFIED", "manufacturing_ready": False}
    try:
        report["commit"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source.parent, text=True).strip()
        report["working_tree"] = subprocess.check_output(["git", "status", "--short"], cwd=source.parent, text=True)
    except (OSError, subprocess.CalledProcessError):
        report["commit"] = "UNVERIFIED"

    def run(arguments, allowed=(0,)):
        command = [cli, *arguments, str(source)]
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", env=env)
        report["commands"].append({"argv": command, "exit": result.returncode,
                                   "stdout": result.stdout, "stderr": result.stderr})
        if result.returncode not in allowed:
            raise RuntimeError(f"KiCad failed ({result.returncode}): {' '.join(arguments[:3])}")
        return result.returncode

    exit_code = 2
    try:
        erc_exit = run(["sch", "erc", "--format", "json", "--exit-code-violations",
                        "--output", str(output / "erc.json")], allowed=(0, 5))
        erc = json.loads((output / "erc.json").read_text(encoding="utf-8"))
        violations = [v for sheet in erc.get("sheets", []) for v in sheet.get("violations", [])]
        report["erc_counts"] = {s: sum(v.get("severity") == s for v in violations) for s in ("error", "warning")}
        report["erc"] = "FAIL" if erc_exit or violations else "PASS"
        run(["sch", "export", "netlist", "--format", "kicadxml", "--output", str(output / "netlist.xml")])
        run(["sch", "export", "bom", "--fields", "Reference,Value,Footprint,Datasheet,MPN,LCSC,QUANTITY",
             "--labels", "Reference,Value,Footprint,Datasheet,MPN,LCSC,Quantity", "--output", str(output / "bom.csv")])
        run(["sch", "export", "svg", "--output", str(output / "svg") + os.sep])
        netlist = ET.parse(output / "netlist.xml")
        components = netlist.findall("./components/comp")
        report["components"] = len(components)
        report["missing_footprints"] = [c.attrib["ref"] for c in components if not (c.findtext("footprint") or "").strip()]
        report["footprints"] = "FAIL" if report["missing_footprints"] else "WARN"
        report["footprint_note"] = "Assignment only; geometry, pin numbering and procurement need review."
        hashes = {}
        for library in netlist.findall("./libraries/library"):
            uri = library.findtext("uri")
            if uri:
                uri = re.sub(r'\$\{([^}]+)\}', lambda m: env.get(m[1], m[0]), uri)
            if uri and Path(uri).is_file():
                hashes[library.attrib["logical"]] = digest(Path(uri))
            else:
                report.setdefault("unresolved_symbols", []).append(library.attrib["logical"])
        for component in components:
            footprint = component.findtext("footprint") or ""
            if ":" not in footprint:
                continue
            name, item = footprint.split(":", 1)
            directory = (source.parent / "Eminuet Library.pretty" if name == "Eminuet Library"
                         else Path(env.get("KICAD10_FOOTPRINT_DIR", "")) / (name + ".pretty"))
            path = directory / (item + ".kicad_mod")
            if path.is_file():
                hashes[footprint] = digest(path)
            else:
                report.setdefault("unresolved_footprints", []).append(component.attrib["ref"])
        report["library_sha256"] = hashes
        if report.get("unresolved_symbols"):
            raise RuntimeError("Referenced symbol libraries could not be resolved")
        if report.get("unresolved_footprints"):
            report["footprints"] = "FAIL"
        if before != {str(p): digest(p) for p in files}:
            raise RuntimeError("Source changed during export; run again after editing finishes")
        exit_code = 1 if report["erc"] == "FAIL" or report["footprints"] == "FAIL" else 0
    except (OSError, ValueError, ET.ParseError, RuntimeError) as error:
        report["tool_error"] = str(error)
    finally:
        report["exit_code"] = exit_code
        (output / "summary.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"ERC {report['erc']}; footprints {report['footprints']}; physical UNVERIFIED")
        print(f"Artifacts: {output}")
    return exit_code


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"UNVERIFIED: {error}", file=sys.stderr)
        sys.exit(2)
