# Deployment Guide for RISC-V SoC Architecture Project

This directory (`OpenSrc_Build_RISCV`) contains a complete set of source codes, configurations, and patches to deploy a RISC-V Chipyard SoC learning environment.

To setup the workspace and deploy the RISC-V environment, follow the sequence of steps below.

## Directory Structure
- `demos/`: Contains the software applications to run on the RISC-V processors.
- `runner/`: Contains `chipyard_runner.py`, the interactive execution script.
- `hardware_patches/`: Contains customized Chisel/Verilog hardware designs (RoCC, DMA, RVV configurations).

## Deployment Steps

### Step 1: Install Dependencies
Ensure `conda` and `git` are installed on the system.
```bash
sudo yum install -y git  # Or apt-get on Debian/Ubuntu
```

### Step 2: Clone Chipyard
Clone Chipyard inside this directory (`OpenSrc_Build_RISCV`) and checkout version `1.14.0`.
```bash
# Note: Navigate to the directory containing this GUIDE before proceeding
git clone https://github.com/ucb-bar/chipyard.git
cd chipyard
git checkout 1.14.0
```

### Step 3: Apply Hardware Patches
Apply the custom hardware modules (e.g., MAC accelerator, LFSR, DMA bridges) to the freshly cloned Chipyard repository before building.
```bash
# Note: Run this from the directory containing this GUIDE
rsync -a hardware_patches/ chipyard/
```

### Step 4: Build Chipyard Toolchains
Initialize Chipyard and compile the RISC-V toolchains. This process will take 30+ minutes depending on system performance.
```bash
cd chipyard
./build-setup.sh riscv-tools -s 6 -s 7 -s 8 -s 9
```

### Step 5: Test and Verify
After compilation is complete, verify the deployment using the interactive runner.
```bash
cd ../runner
source ../chipyard/env.sh
# Check if python dependencies are met
pip install inquirer pyyaml
python chipyard_runner.py
```

### Script Execution Rules
- The `chipyard_runner.py` uses **relative paths** (`../chipyard` and `../demos/...`) defined in `projects.yaml`.
- It dynamically resolves paths, meaning as long as `chipyard` and `demos` are adjacent to `runner`, it will work in any environment or directory structure.
