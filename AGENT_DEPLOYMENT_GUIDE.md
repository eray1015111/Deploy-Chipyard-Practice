# Deployment Guide

This guide describes how to set up the `OpenSrc_Build_RISCV` package with Chipyard.

## Prerequisites

- Linux environment
- `git`
- `conda`
- C/C++ build tools
- Python 3

On Ubuntu or Debian:

```bash
sudo apt-get update
sudo apt-get install -y git make gcc g++ python3 python3-pip rsync
```

## Step 1: Clone Chipyard

Run from the `OpenSrc_Build_RISCV` directory:

```bash
git clone https://github.com/ucb-bar/chipyard.git
cd chipyard
git checkout 1.14.0
cd ..
```

## Step 2: Apply Hardware Patches

Copy the provided Chipyard patches into the cloned Chipyard tree:

```bash
rsync -a hardware_patches/ chipyard/
```

The patches add these demonstration configurations:

- `LFSRDemoRocketConfig`
- `RoCCDemoRocketConfig`
- `RVVDemoRocketConfig`
- `DMADemoRocketConfig`

## Step 3: Build the Toolchain

```bash
cd chipyard
./build-setup.sh riscv-tools -s 6 -s 7 -s 8 -s 9
source env.sh
```

This step may take a long time because it builds the RISC-V toolchain and simulator dependencies.

## Step 4: Run a Demo

```bash
cd ../runner
pip install inquirer pyyaml
python chipyard_runner.py
```

Select one of the listed implementations. The runner compiles the C program first, then runs the corresponding Verilator simulation.

## Notes

- The runner assumes `chipyard/`, `demos/`, and `runner/` remain next to each other under `OpenSrc_Build_RISCV`.
- The demos compare each accelerator against the same C golden model.
- If a simulation fails, check the generated compile command and the Chipyard Verilator log first.
