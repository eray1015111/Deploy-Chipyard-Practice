# RISC-V LFSR Hardware Acceleration Examples

This package contains Chipyard patches, bare-metal RISC-V demos, and a small runner for comparing several LFSR implementations.

The examples use the same 32-bit Galois LFSR rule:

```c
state = (state & 1) ? ((state >> 1) ^ 0x80200003) : (state >> 1);
```

## Directory Layout

- `demos/`: bare-metal C programs and linker scripts.
- `hardware_patches/`: Chipyard, Chisel, and Verilog patches.
- `runner/`: `projects.yaml` and `chipyard_runner.py` for compiling and running simulations.
- `AGENT_DEPLOYMENT_GUIDE.md`: setup instructions.
- `COMPREHENSIVE_RISCV_SIMULATION_REPORT.md`: architecture notes and project assessment.

## Implementations

| Implementation | Path | Purpose |
| --- | --- | --- |
| Pure C | `demos/demo/pure_c_test.c` | Golden model and baseline cycle count |
| MMIO peripheral | `demos/demo/lfsr_test.c`, `example/LFSR.scala`, `LFSRMMIOBlackBox.v` | Register-mapped LFSR accelerator at `0x6000` |
| RoCC | `demos/demo_rocc/lfsr_rocc_test.c`, `example/LFSRRoCC.scala` | Custom instruction accelerator |
| RVV | `demos/demo_rvv/rvv_test.c` | Vectorized software implementation using the RISC-V vector unit |
| DMA | `demos/demo_dma/dma_test.c`, `example/LFSRDMA.scala` | TileLink master peripheral that reads and writes memory |

## Basic Setup

Run these commands from `OpenSrc_Build_RISCV` on a Linux machine with `git`, `conda`, and build tools installed:

```bash
git clone https://github.com/ucb-bar/chipyard.git
cd chipyard
git checkout 1.14.0
cd ..
rsync -a hardware_patches/ chipyard/
cd chipyard
./build-setup.sh riscv-tools -s 6 -s 7 -s 8 -s 9
source env.sh
```

Toolchain setup can take more than 30 minutes depending on the machine.

## Running Simulations

```bash
cd runner
pip install inquirer pyyaml
python chipyard_runner.py
```

The runner reads `runner/projects.yaml`, compiles the selected bare-metal program, and launches the matching Chipyard Verilator configuration.

## Current Project Status

This package is useful as a learning and comparison project, but it should be treated as simulation-oriented code rather than production IP. The strongest parts are the clear architecture split and the shared LFSR golden model. The main risks are integration fragility, limited automated testing, and dependence on a specific Chipyard version.
