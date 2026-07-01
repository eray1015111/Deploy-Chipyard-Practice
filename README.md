# RISC-V SoC Architecture & Hardware Acceleration Learning Environment

[English](#english) | [繁體中文](#繁體中文)

---

<a id="english"></a>
## English Version

This repository contains a complete deployment package for teaching the fundamentals of RISC-V hardware-software co-design using UC Berkeley's **Chipyard** framework. It demonstrates and compares the performance of different hardware architectures by running a Linear Feedback Shift Register (LFSR) algorithm across multiple hardware implementations.

---

### 1. Automated Environment Deployment

This repository includes an automated deployment script. To set up the environment, follow the steps outlined in `AGENT_DEPLOYMENT_GUIDE.md`. The guide provides instructions for cloning Chipyard, installing the RISC-V toolchains, and applying the custom hardware patches.

---

### 2. Manual Environment Deployment

To set up the environment manually, follow these steps:

#### Step 1: Install Dependencies
Ensure your Linux machine has `git`, `make`, and `conda` installed.

#### Step 2: Clone Chipyard
```bash
# In the OpenSrc_Build_RISCV directory:
git clone https://github.com/ucb-bar/chipyard.git
cd chipyard
git checkout 1.14.0
```

#### Step 3: Apply Custom Hardware Patches
Merge the custom RISC-V configurations (RoCC, DMA, RVV, MAC) into Chipyard:
```bash
# Run this from the OpenSrc_Build_RISCV directory:
rsync -a hardware_patches/ chipyard/
```

#### Step 4: Build the Toolchain
```bash
cd chipyard
./build-setup.sh riscv-tools -s 6 -s 7 -s 8 -s 9
source env.sh
```
*(Note: Compiling the RISC-V toolchain may take 30+ minutes depending on system specifications).*

---

### 3. Execution and Testing

Once the environment is deployed, use the interactive Python testing tool to compile the C programs, run the hardware simulations, and compare performance.

#### Running the Tool
```bash
cd runner
pip install inquirer pyyaml
python chipyard_runner.py
```

#### Tool Features
The interactive menu provides the following options:

1. **[Generate Performance Report]**
   - Parses execution logs from all implementations and generates a performance comparison table (Cycle counts).
2. **[Baseline] Pure Software Execution (Pure C Golden Model)**
   - Compiles the LFSR algorithm in pure C to establish a software baseline.
3. **[Architecture A] RoCC Custom Coprocessor**
   - Tests a tightly-coupled custom hardware accelerator integrated directly into the CPU pipeline.
4. **[Architecture B] RVV Vector Extension**
   - Tests a standard SIMD architecture using RISC-V Vector Intrinsics.
   - **Saturn RVV Architecture:** (Powered by UCB Saturn Vector Unit)
     <br><img src="https://raw.githubusercontent.com/ucb-bar/saturn-vectors/master/docs/diag/overview.png" width="600" alt="Saturn Vector Architecture">
5. **[Architecture C] TileLink MMIO with DMA**
   - Tests a loosely-coupled hardware IP that fetches data independently via Direct Memory Access (DMA).

For each architecture, the tool outputs the execution trace, indicating the compiled C file and the simulated Chipyard SoC configuration.

---

<a id="繁體中文"></a>
## 繁體中文版 (Traditional Chinese)

本儲存庫包含一個完整的部署套件，旨在利用加州大學柏克萊分校 (UC Berkeley) 的 **Chipyard** 框架，教授 RISC-V 硬體與軟體協同設計的基礎知識。它透過在多種硬體實作上執行線性反饋移位暫存器 (LFSR) 演算法，來展示並比較不同硬體架構的效能。

---

### 1. 自動化環境部署

本儲存庫提供自動化部署指南。若要設定環境，請參閱 `AGENT_DEPLOYMENT_GUIDE.md` 中的步驟。該指南涵蓋了複製 Chipyard、安裝 RISC-V 工具鏈，以及套用自訂硬體補丁的流程。

---

### 2. 手動環境部署

若需手動設定環境，請按照以下步驟進行：

#### 步驟 1：安裝相依套件
請確保您的 Linux 系統已安裝 `git`、`make` 與 `conda`。

#### 步驟 2：複製 Chipyard
```bash
# 在 OpenSrc_Build_RISCV 目錄下執行：
git clone https://github.com/ucb-bar/chipyard.git
cd chipyard
git checkout 1.14.0
```

#### 步驟 3：套用自訂硬體補丁
將自訂的 RISC-V 配置 (RoCC, DMA, RVV, MAC) 合併至 Chipyard 中：
```bash
# 在 OpenSrc_Build_RISCV 目錄下執行：
rsync -a hardware_patches/ chipyard/
```

#### 步驟 4：建置工具鏈
```bash
cd chipyard
./build-setup.sh riscv-tools -s 6 -s 7 -s 8 -s 9
source env.sh
```
*(注意：依據系統規格，編譯 RISC-V 工具鏈可能需要 30 分鐘以上的時間)。*

---

### 3. 執行測試與驗證

環境部署完成後，可使用互動式 Python 測試工具來編譯 C 程式、執行硬體模擬，並比較各架構的效能。

#### 執行工具
```bash
cd runner
pip install inquirer pyyaml
python chipyard_runner.py
```

#### 工具特色
執行此工具時，選單將提供以下選項：

1. **[產生效能報表 (Generate Performance Report)]**
   - 解析所有實作的執行日誌，並產生效能比較表 (執行週期數)。
2. **[基準線] 純軟體執行 (Pure C Golden Model)**
   - 以純 C 語言編譯 LFSR 演算法以建立軟體基準線。
3. **[架構 A] RoCC 自訂協同處理器**
   - 測試緊密耦合、直接整合進 CPU 管線的自訂硬體加速器。
4. **[架構 B] RVV 向量擴充**
   - 測試使用 RISC-V 向量內部函式 (Vector Intrinsics) 的標準 SIMD 架構。
   - **Saturn RVV 架構:** (由 UCB Saturn 向量單元提供技術支援)
     <br><img src="https://raw.githubusercontent.com/ucb-bar/saturn-vectors/master/docs/diag/overview.png" width="600" alt="Saturn Vector Architecture">
5. **[架構 C] TileLink MMIO 搭配 DMA**
   - 測試透過直接記憶體存取 (DMA) 獨立擷取資料、較為鬆散耦合的硬體 IP。

此工具會自動輸出詳細的執行追蹤紀錄，顯示當前編譯的程式檔與模擬的 Chipyard SoC 配置。
