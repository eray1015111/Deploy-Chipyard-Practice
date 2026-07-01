import os
import re
import subprocess
import sys
from pathlib import Path

import yaml

try:
    import inquirer
except ImportError:
    print("Missing dependency: inquirer. Install it with: pip install inquirer pyyaml")
    sys.exit(1)


DEBUG_MODE = False


def load_config(yaml_path="projects.yaml"):
    path = Path(yaml_path)
    if not path.exists():
        print(f"Configuration file not found: {path}")
        sys.exit(1)

    try:
        with path.open("r", encoding="utf-8") as f:
            return yaml.safe_load(f)
    except yaml.YAMLError as exc:
        print(f"Error parsing YAML: {exc}")
        sys.exit(1)


def print_status(message):
    print(f"[status] {message}")


def print_error(message):
    print(f"[error] {message}")


def generate_commands(project, chipyard_dir):
    makefile_dir = project["makefile_dir"]
    hw_config = project["hw_config"]
    compile_cmd = project["compile_cmd"]
    binary_name = project.get("binary_name", "output.riscv")
    is_debug = project.get("is_debug", False)

    source_env = f"source {chipyard_dir}/env.sh"
    cmd_sw = f"{source_env} && cd {makefile_dir} && {compile_cmd}"

    waveform = "debug" if is_debug else "run-binary"
    cmd_hw = (
        f"{source_env} && cd {chipyard_dir}/sims/verilator && "
        f"make CONFIG={hw_config} {waveform} BINARY=../../{makefile_dir}/{binary_name}"
    )
    return cmd_sw, cmd_hw


def run_native_command(command, stage_name):
    print_status(f"Starting {stage_name}")
    print(command)

    if DEBUG_MODE:
        return True

    result = subprocess.run(command, shell=True, executable="/bin/bash")
    if result.returncode != 0:
        print_error(f"{stage_name} failed with exit code {result.returncode}")
        return False

    print_status(f"{stage_name} completed")
    return True


def export_script(cmd_sw, cmd_hw, script_name="run_task.sh"):
    script_content = f"""#!/usr/bin/env bash
set -euo pipefail

echo "[script] Compiling software"
{cmd_sw}

echo "[script] Running hardware simulation"
{cmd_hw}
"""
    Path(script_name).write_text(script_content, encoding="utf-8")
    if os.name == "posix":
        os.chmod(script_name, 0o755)
    print_status(f"Exported script: {Path(script_name).resolve()}")


def parse_cycles(log_text):
    patterns = [
        r"\[(?:SW|HW|DMA|RoCC|RVV)\].*cycles=(\d+)",
        r"cycles=(\d+)",
    ]
    for pattern in patterns:
        match = re.search(pattern, log_text)
        if match:
            return int(match.group(1))
    return None


def compare_simulation_results(projects, chipyard_dir):
    print("\nPerformance comparison")
    print("----------------------")
    chipyard_path = Path(chipyard_dir).expanduser()

    for project in projects:
        if project.get("is_debug", False):
            continue

        hw_config = project["hw_config"]
        binary_name = project.get("binary_name", "output.riscv")
        log_name = binary_name.replace(".riscv", ".log")
        log_path = (
            chipyard_path
            / "sims"
            / "verilator"
            / "output"
            / f"chipyard.harness.TestHarness.{hw_config}"
            / log_name
        )

        if not log_path.exists():
            print(f"{project['name']}: log not found ({log_path})")
            continue

        cycles = parse_cycles(log_path.read_text(encoding="utf-8", errors="replace"))
        if cycles is None:
            print(f"{project['name']}: cycles not found")
        else:
            print(f"{project['name']}: {cycles} cycles")

    print("\n[Architectural Performance Analysis]")
    print("1. Pure C (Software): Sequentially executed by the CPU ALU, constrained by instruction fetch, decode, and branch overheads.")
    print("2. MMIO (Register-Mapped Accelerator): Uses TileLink register access. It is easy to integrate and debug, but polling and MMIO traffic add overhead.")
    print("3. RoCC (Tightly Coupled): Integrated close to the CPU pipeline with low communication latency. Suitable for compute-heavy scalar tasks with small input sizes.")
    print("4. DMA (Loosely Coupled): Accesses memory through the system bus. It has bus latency, but can offload CPU work and is suitable for bulk transfers.")
    print("5. RVV (Vector Extension): Uses standard vector instructions for data-level parallelism. It is suitable for arrays and independent streams.")
    print("==================================================")
    print("\033[91m[Important: Workload Size Is Different]\033[0m")
    print("The RVV demo processes 256 independent LFSR streams, while the Pure C, MMIO, RoCC, and DMA demos process one scalar stream.")
    print("This difference is intentional: the RVV test is designed to show parallel acceleration capability, not a same-data-volume scalar comparison.")
    print("When comparing cycle counts, treat RVV as a throughput-oriented case and avoid comparing its raw cycle count directly with single-stream results.")
    print("==================================================\n")


def print_stage_info(stage, current_path, input_path, output_path):
    print(f"\n[{stage}]")
    print(f"  working directory: {current_path}")
    print(f"  input: {input_path}")
    print(f"  output: {output_path}")


def main():
    config = load_config()
    projects = config.get("projects", [])
    chipyard_dir = config.get("global_settings", {}).get("chipyard_dir", "../chipyard")

    if not projects:
        print_error("No projects are defined in projects.yaml")
        return

    choices = [("Generate performance report from existing logs", "compare_results")]
    for project in projects:
        choices.append((f"{project['name']} - {project['description']}", project))

    answer = inquirer.prompt([
        inquirer.List("selected_project", message="Select a task", choices=choices)
    ])
    if not answer:
        return

    selected = answer["selected_project"]
    if selected == "compare_results":
        compare_simulation_results(projects, chipyard_dir)
        return

    cmd_sw, cmd_hw = generate_commands(selected, chipyard_dir)
    binary_name = selected.get("binary_name", "output.riscv")
    makefile_dir = selected["makefile_dir"]

    export = inquirer.confirm("Export commands to run_task.sh instead of running now?", default=False)
    if export:
        export_script(cmd_sw, cmd_hw)
        return

    print_stage_info(
        "Software compilation",
        makefile_dir,
        binary_name.replace(".riscv", ".c"),
        binary_name,
    )
    if not run_native_command(cmd_sw, "software compilation"):
        sys.exit(1)

    print_stage_info(
        "Hardware simulation",
        f"{chipyard_dir}/sims/verilator",
        binary_name,
        binary_name.replace(".riscv", ".log"),
    )
    if not run_native_command(cmd_hw, "hardware simulation"):
        sys.exit(1)

    print_status("Task completed")


if __name__ == "__main__":
    main()
