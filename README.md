# RISC-V SoC Architecture & Hardware Acceleration Learning Environment

[English](#english) | [繁體中文](#繁體中文)

---

<a id="english"></a>
## English Version

Welcome to the RISC-V SoC Architecture & Hardware Acceleration Learning Environment! 

This repository contains a complete, portable deployment package designed to teach the fundamentals of RISC-V hardware-software co-design using UC Berkeley's **Chipyard** framework. It demonstrates and compares the performance of different hardware architectures by running a Linear Feedback Shift Register (LFSR) algorithm across multiple hardware implementations.

---

### 🚀 1. Automated Environment Deployment (For AI Agents)

This repository is uniquely designed to be deployed by an AI Agent (like GitHub Copilot, Claude, or Google Gemini). 

If you are on a new machine and want to set this environment up, you **do not** need to install it manually. Simply open your AI assistant in this folder and prompt it with:

> *"Please deploy the RISC-V environment using the guide in `AGENT_DEPLOYMENT_GUIDE.md`."*

The AI Agent will read the system prompt, automatically clone Chipyard, install the RISC-V toolchains, and apply the custom hardware patches for you.

---

### 💻 2. Manual Environment Deployment

If you prefer to set up the environment manually, follow these steps:

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
Our custom RISC-V configurations (RoCC, DMA, RVV, MAC) need to be merged into Chipyard:
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
*(Note: Compiling the RISC-V toolchain may take 30+ minutes depending on your CPU).*

---

### 📊 3. How to Run the Tests

Once the environment is deployed (either by the AI Agent or manually), you can use our interactive Python testing tool to compile the C programs, run the hardware simulations, and compare performance.

#### Running the Tool
```bash
cd runner
# Make sure you have the python requirements
pip install inquirer pyyaml

# Launch the interactive testing tool
python chipyard_runner.py
```

#### Tool Features
When you run the tool, an interactive menu will appear. You can use the `UP` and `DOWN` arrows to select an option, and `Enter` to confirm:

1. **[Generate Performance Report]**
   - Automatically parses the execution logs from all implementations and generates a side-by-side performance comparison table (Cycle counts).
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

For each architecture you run, the tool will automatically output the step-by-step execution trace so you can see exactly which C file is being compiled and which Chipyard SoC configuration is being simulated!

---

<a id="繁體中文"></a>
## 繁體中文版 (Traditional Chinese)

歡迎來到 **RISC-V SoC 架構與硬體加速學習環境**！

本儲存庫包含一個完整、具可攜性的部署套件，旨在利用加州大學柏克萊分校 (UC Berkeley) 的 **Chipyard** 框架，來教授 RISC-V 硬體與軟體協同設計的基礎知識。它透過在多種硬體實作上執行線性反饋移位暫存器 (LFSR) 演算法，來展示並比較不同硬體架構的效能。

---

### 🚀 1. 自動化環境部署 (適用於 AI 助理)

本儲存庫經過特別設計，可由 AI 助理（如 GitHub Copilot、Claude 或 Google Gemini）直接部署。

如果您在一台新機器上並希望設定此環境，您**不需**手動安裝。只需在此資料夾中開啟您的 AI 助理並輸入以下提示：

> *"請使用 `AGENT_DEPLOYMENT_GUIDE.md` 中的指南部署 RISC-V 環境。"*

AI 助理會讀取系統提示、自動複製 Chipyard、安裝 RISC-V 工具鏈，並為您套用自訂的硬體補丁。

---

### 💻 2. 手動環境部署

若您偏好手動設定環境，請按照以下步驟進行：

#### 步驟 1：安裝相依套件
請確保您的 Linux 機器已安裝 `git`、`make` 與 `conda`。

#### 步驟 2：複製 Chipyard
```bash
# 在 OpenSrc_Build_RISCV 目錄下執行：
git clone https://github.com/ucb-bar/chipyard.git
cd chipyard
git checkout 1.14.0
```

#### 步驟 3：套用自訂硬體補丁
我們自訂的 RISC-V 配置 (RoCC, DMA, RVV, MAC) 需要合併至 Chipyard 中：
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
*(注意：依據您的 CPU 效能，編譯 RISC-V 工具鏈可能需要 30 分鐘以上的時間)。*

---

### 📊 3. 如何執行測試

一旦環境部署完成（無論是透過 AI 助理或手動完成），您可以使用我們互動式的 Python 測試工具來編譯 C 程式、執行硬體模擬，並比較效能。

#### 執行工具
```bash
cd runner
# 確保您已安裝所需的 python 套件
pip install inquirer pyyaml

# 啟動互動式測試工具
python chipyard_runner.py
```

#### 工具特色
當您執行此工具時，將會出現一個互動式選單。您可以使用 `上` 與 `下` 方向鍵選擇項目，並按 `Enter` 確認：

1. **[產生效能報表 (Generate Performance Report)]**
   - 自動解析所有實作的執行日誌，並產生並排的效能比較表 (執行週期數)。
2. **[基準線] 純軟體執行 (Pure C Golden Model)**
   - 以純 C 語言編譯 LFSR 演算法以建立軟體基準。
3. **[架構 A] RoCC 自訂協同處理器**
   - 測試緊密耦合、直接整合進 CPU 管線的自訂硬體加速器。
4. **[架構 B] RVV 向量擴充**
   - 測試使用 RISC-V 向量內部函式 (Vector Intrinsics) 的標準 SIMD 架構。
   - **Saturn RVV 架構:** (由 UCB Saturn 向量單元提供技術支援)
     <br><img src="https://raw.githubusercontent.com/ucb-bar/saturn-vectors/master/docs/diag/overview.png" width="600" alt="Saturn Vector Architecture">
5. **[架構 C] TileLink MMIO 搭配 DMA**
   - 測試透過直接記憶體存取 (DMA) 獨立擷取資料、較為鬆散耦合的硬體 IP。

對於您執行的每一個架構，此工具將自動輸出逐步的執行追蹤紀錄，讓您可以確切看到正在編譯哪個 C 檔案，以及正在模擬哪一個 Chipyard SoC 配置！
