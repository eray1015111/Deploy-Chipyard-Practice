# RISC-V 模擬與客製化硬體加速報告

說明基於 Chipyard 平台的模擬流程，及將軟體演算法轉換為客製化硬體加速器 (RoCC, RVV, DMA) 的實作與評估。

---

## 軟硬體協同開發標準流程 (HW/SW Co-Design Workflow)

標準流程：
1. **C 語言實作**：完成演算法之軟體實作。
2. **純軟體效能分析**：透過模擬器辨識系統效能瓶頸。
3. **軟體最佳化**：針對瓶頸區段進行演算法層級最佳化。
4. **凍結軟體程式碼**：作為後續硬體驗證基準 (Golden Model)。
5. **評估硬體架構**：依據資料流向選擇適當之硬體架構 (RoCC, MMIO, RVV)。
6. **軟硬體協同模擬**：開發 RTL、整合 C 語言程式，進行驗證與效能分析。

本報告聚焦於上述流程之第 5 與第 6 步驟。

---

## 1. 系統架構配置

本專案效能評估與模擬之硬體配置：

* **中央處理器 (CPU)**：Rocket Core (64-bit, In-order, Scalar, RV64IMAFDC, Single-issue)。
* **快取系統 (Cache)**：
  * L1 I-Cache：32 KB
  * L1 D-Cache：32 KB
* **系統匯流排 (System Bus)**：TileLink。
* **向量單元 (Vector Unit)** (僅在 RVV 測試啟用)：支援標準 RISC-V 向量擴充指令集。
* **主記憶體 (Main Memory)**：透過 TileLink 連接之模擬 DRAM。

**【Rocket Chip 系統架構總覽：標示三種硬體設計位置與傳輸介面】**
```text
  +-----------------------------------------------------------------------+
  |                        Rocket Chip SoC                                |
  |                                                                       |
  |   +---------------------------------------------------------------+   |
  |   |                       Rocket Core (CPU)                       |   |
  |   |                                                               |   |
  |   |             +-----------------------------------+             |   |
  |   |             |     ALU / Scalar Pipeline         |             |   |
  |   |             |     (C Code 執行與指令分派)       |             |   |
  |   |             +-----------------------------------+             |   |
  |   |               ^               |               ^               |   |
  |   | (Vector Inst/ |               | (L1 Mem Req)  | (RoCC Cmd/    |   |
  |   |  Scalar Regs) |               |               |  rs1,rs2,rd)  |   |
  |   |               v               v               v               |   |
  |   |  +---------------+     +---------------+     +-------------+  |   |
  |   |  | Vector Unit   |     |  L1 D-Cache   |     | RoCC Coproc |  |   |
  |   |  | (設計 B:RVV)  |     |  (資料快取)   |     | (設計 A:RoCC|  |   |
  |   |  +---------------+     +---------------+     +-------------+  |   |
  |   |          |                    |                     |         |   |
  |   |          | (L1 Mem I/F)       | (TileLink Cache)    | (L1 I/F)|   |
  |   |   +----------|--------------------|---------------------|---------+   |
  |              v                    v                     v             |
  |      ========|====================|=====================|=======      |
  |                               TileLink Bus                            |
  |      ========|====================|=====================|=======      |
  |              |                    |                     |             |
  | (TileLink    |            (TileLink MMIO)               | (TileLink   |
  |  Mem Access) v                    v                     v  DMA Access)|
  |      +---------------+    +---------------+     +---------------+     |
  |      |  L2 Cache /   |    |  UART/GPIO    |     |  DMA Accel.   |     |
  |      |  Mem Ctrl     |    | (Peripheral)  |     | (設計 C:DMA)  |     |
  |      +---------------+    +---------------+     +---------------+     |
  +--------------|------------------------------------------|-------------+
                 | (DRAM I/F)                               |
                 v                                          v
         +----------------------------------------------------------+
         |                 Simulated Main Memory (DRAM)             |
         +----------------------------------------------------------+
```
*(註：Vector Unit 與 RoCC Coprocessor 無直接資料傳輸線路，需透過 CPU 暫存器或快取/記憶體中轉。)*

---

## 2. 實作說明與程式碼範例

本專案建構軟硬體協同開發驗證環境。以下展示純軟體基準與三種硬體加速器之實作架構。

### 階段一：Pure C Code (純軟體基準)
演算法於 CPU 內以純軟體執行，用於確立正確性 (Golden Model) 及辨識效能瓶頸。

**【純軟體實作目標架構示意圖】**
```text
  [純軟體端 (Software Space)]                           [硬體端 (Hardware Space)]
  
  +--------------------------------+                  +-------------------------+
  |  pure_c_test.c (主程式 / 測試框架) |                  |                         |
  |  - 初始化測試資料              |                  |                         |
  |  - 呼叫純軟體 Solver           |                  |                         |
  +--------------------------------+                  |                         |
               | (Function Call)                      |                         |
               v                                      |                         |
  +--------------------------------+                  |                         |
  |  lfsr_sw (純 C 演算法函式)     |                  |      (完全沒有使用到)   |
  |  - 一般區段 (純 C 執行)        |                  |      (客製化硬體資源)   |
  |  - 效能瓶頸區段 (純 C 迴圈)    |                  |                         |
  |  - 運算結束回傳結果            |                  |                         |
  +--------------------------------+                  |                         |
                                                      +-------------------------+
```

**【Pure C 核心 Code 示例】**
```c
// pure_c_test.c (純軟體版本)
#include <stdint.h>

uint32_t lfsr_sw(uint32_t seed, uint32_t steps) {
    // 效能瓶頸區段：大量耗時的純軟體迴圈
    uint32_t lfsr = seed;
    for (uint32_t i = 0; i < steps; i++) {
        if (lfsr & 1) lfsr = (lfsr >> 1) ^ 0x80200003;
        else          lfsr = (lfsr >> 1);
    }
    return lfsr;
}
```

**【LFSR 演算法 Gate-Level 邏輯電路示意圖】**
*(註：此為上述 C Code 迴圈之等效硬體邏輯)*
```text
                  +-----+   +-----+   +-----+         +-----+
        Feedback  | DFF |   | DFF |   | DFF |         | DFF |
       +--------->| [31]|-->| [30]|-->| [29]|-->...-->| [0] |
       |          +-----+   +-----+   +-----+         +-----+
       |                                                 |
       |                   +-------+       +-------+     |
       +-------------------|  XOR  |<------|  XOR  |<----+ (bit 0)
                           +-------+       +-------+
                               ^               ^
                               |               |
                            (bit 5)    +-------+
                                       |  XOR  |<------- (bit 2)
                                       +-------+
                                           ^
                                           |
                                        (bit 3)
```

---

### 階段二：硬體加速替換 (RoCC, RVV, DMA)
將效能瓶頸區段改以硬體加速執行。

#### 方案 A：RoCC 客製化協同處理器
透過自訂指令將資料傳遞至緊密耦合之硬體進行運算。

**【RoCC 實作目標架構示意圖】**
```text
  [純軟體端 (Software Space)]                           [硬體端 (Hardware Space)]
  
  +--------------------------------+                  +-----------------------------------+
  |  test.c (主程式)               |                  |           Rocket Core (CPU)       |
  |  - 初始化測試資料              |                  |  +-----------------------------+  |
  |  - 呼叫 RoCC Solver            |                  |  | CPU Registers (x0 - x31)    |  |
  +--------------------------------+                  |  +-----------------------------+  |
               | (Function Call)                      |              | rs1, rs2 (指令傳遞)|
               v                                      |              v                    |
  +--------------------------------+                  |  +=============================+  |
  |  lfsr_rocc_test.c (RoCC加速版) |                  |  | RoCC Coprocessor (硬體位置) |  |
  |  - 一般區段維持純 C 執行       |                  |  |-----------------------------|  |
  |  - 執行 ROCC_INSTRUCTION_DSS   | ==============>  |  | [內建 LFSR RTL Gate-Level]  |  |
  |    (CPU 停頓等待硬體算完)      | <=== 寫回 rd ==  |  +=============================+  |
  +--------------------------------+                  |                                   |
                                                      +-----------------------------------+
```

**【RoCC 核心 Code 示例】**
```c
// lfsr_rocc_test.c (RoCC 加速版本)
#include "rocc.h"

uint32_t lfsr_rocc(uint32_t seed, uint32_t steps) {
    uint64_t hw_result = 0;
    // 瓶頸區段替換為硬體指令
    ROCC_INSTRUCTION_DSS(0, hw_result, seed, steps, 0);
    return (uint32_t)hw_result;
}
```

#### 方案 B：RVV 向量擴充指令集

**【Saturn RVV 微架構官方設計圖】**
<img src="https://raw.githubusercontent.com/ucb-bar/saturn-vectors/master/docs/diag/overview.png" width="800" alt="Saturn Vector Architecture">

利用標準 RISC-V Vector Unit 進行多筆資料平行運算 (SIMD)。

**【架構示意圖】**
```text
  +-------------------------------------------------+
  |                  Rocket Core                    |
  |                 [執行 C Code]                   |
  |                                                 |
  |  +-------------+       +--------------------+   |
  |  |   Vector    |       |                    |   |
  |  |  Registers  |       |   Vector Unit      |   |
  |  |             |       |  [執行 RTL 硬體]   |   |
  |  | (v0 - v31)  |<=====>|                    |   |
  |  |             |       |                    |   |
  |  +-------------+       +--------------------+   |
  +-------------------------------------------------+
            |                     |
            v                     v
  +-------------------------------------------------+
  |               L1 D-Cache / Memory               |
  +-------------------------------------------------+
```
* **通訊機制**：由編譯器 (GCC `-march=rv64gcv`) 處理 Vector Intrinsics 轉換。
* **特性**：相容標準指令，無需客製化 RTL，適合處理資料平行 (Data Parallelism) 運算。

**軟體呼叫 (C Code)**：
```c
// rvv_test.c (RVV 陣列平行運算版本)
#include <riscv_vector.h>

void lfsr_hw_array(uint32_t* seeds_in, uint32_t* res, uint32_t steps, size_t n) {
    size_t vl;
    for (; n > 0; n -= vl, seeds_in += vl, res += vl) {
        vl = __riscv_vsetvl_e32m1(n); 
        vuint32m1_t vlfsr = __riscv_vle32_v_u32m1(seeds_in, vl);
        // ... 省略 LFSR 條件分支 Intrinsics ...
        __riscv_vse32_v_u32m1(res, vlfsr, vl);
    }
}
```

#### 方案 C：TileLink MMIO 搭配 DMA
硬體透過匯流排獨立運作，處理資料搬移。

**【DMA 實作目標架構示意圖】**
```text
  [純軟體端 (Software Space)]                           [硬體端 (Hardware Space)]
  
  +--------------------------------+                  +-----------------------------------+
  |  test.c (主程式)               |                  |           Rocket Core (CPU)       |
  |  - 準備位於 Memory 的巨量資料  |                  |  +-----------------------------+  |
  |  - 呼叫 DMA Solver             |                  |  | MMIO Load / Store (匯流排)  |  |
  +--------------------------------+                  |  +-----------------------------+  |
               | (Function Call)                      |              | 暫存器配置 (設定)  |
               v                                      |              v                    |
  +--------------------------------+                  |  +=============================+  |
  |  dma_test.c (DMA 加速版)       |                  |  | DMA Accelerator (硬體位置)  |  |
  |  - 將記憶體位址寫入 MMIO 暫存器| ==============>  |  |-----------------------------|  |
  |  - 啟動硬體並進入 Polling 迴圈 | <==============  |  | [DMA Engine + LFSR RTL]     |  |
  |  - 偵測 STATUS 完成後跳出      |    (中斷或輪詢)  |  +=============================+  |
  +--------------------------------+                  |      | 主動抓取/寫回 (DMA)        |
                                                      |      v                            |
                                                      |  [ TileLink Bus -> Main Memory ]  |
                                                      +-----------------------------------+
```
* **通訊機制**：CPU 透過寫入 MMIO 暫存器啟動加速器，加速器透過 DMA 讀寫記憶體。
* **特性**：完全卸載 CPU 負載，適合大規模資料流搬移，但具備較高匯流排延遲 (Overhead)。

**【DMA 核心 Code 示例】**
```c
// dma_test.c (DMA 加速版本)
#define DMA_STATUS 0x7000
#define DMA_ADDR   0x7008
#define DMA_COUNT  0x7010

void lfsr_dma(uint64_t mem_array, uint32_t array_length) {
    *(volatile uint64_t*)DMA_ADDR = mem_array;
    *(volatile uint32_t*)DMA_COUNT = array_length;

    while ((*(volatile uint8_t*)DMA_STATUS & 0x1) == 0) {
        // Polling 狀態暫存器
    }
}
```

---

## 3. RISC-V 模擬基礎知識

1. **Chisel 硬體描述語言**：基於 Scala，用於生成參數化之 Verilog。
2. **TileLink 匯流排**：RISC-V SoC 內部標準通訊協定。
3. **模擬器分類**：
   * **ISA 模擬器 (Spike)**：指令級模擬，無硬體時序。
   * **RTL 模擬器 (Verilator / VCS)**：Cycle-Accurate 模擬硬體狀態。
4. **FESVR / HTIF**：負責模擬器與裸機程式間通訊 (例如處理 `printf`)。

---

## 4. 模擬流程

模擬過程分為「純軟體階段」與「軟硬體協同階段」：

### 第一部分：純軟體模擬 (Pure C Profiling)
使用標準 RISC-V CPU 執行純 C 語言演算法。

1. **編譯標準模擬器**：
   ```bash
   cd chipyard/sims/verilator
   make -j16 CONFIG=RocketConfig
   ```
2. **編譯 C 程式**：
   使用 `mcycle` 暫存器測量時鐘週期。
   ```bash
   riscv64-unknown-elf-gcc -O2 -specs=htif_nano.specs -T htif.ld -Wl,-u,tohost -Wl,-u,fromhost test.c -o test.riscv
   ```
3. **執行模擬**：
   ```bash
   ./simulator-chipyard.harness-RocketConfig test.riscv > pure_c_result.log
   ```

### 第二部分：軟硬體協同模擬 (HW/SW Co-Design Simulation)
針對選定區塊進行硬體加速與模擬驗證。

1. **配置自訂硬體**：於 Chisel 原始碼中掛載 RTL 設計。
2. **編譯客製化模擬器**：
   ```bash
   cd chipyard/sims/verilator
   make -j16 CONFIG=YourCustomRocketConfig
   ```
3. **編譯硬體加速之 C 程式**：將迴圈替換為硬體操作指令。
4. **執行模擬**：驗證硬體輸出，並對比 Cycle Count。
   ```bash
   ./simulator-chipyard.harness-YourCustomRocketConfig test_hw.riscv > hw_result.log
   ```

---

## 5. 架構效能解析

### 深入解析：架構測資與 Cycle Count 標準差異 (Fairness & Evaluation)

在本專案比較不同架構時，常會有個疑問：「**為什麼不讓所有架構跑一模一樣的 256 個 Seed，這樣比較才公平？**」或者「**為什麼同樣的 Pure C Golden Model，在不同架構模擬時的 Cycle Count 會不一樣？**」

這主要受到硬體設計及微架構層面影響：

#### 1. 測資設定差異 (SIMD vs. ASIC)
硬體評估重點在於發揮特定設計之最佳場景：
* **純量專用加速器 (RoCC / DMA / MMIO)**：
  設計為「深度循序運算 (Deep Sequential)」。擅長單一資料之連續處理。若處理多組零散資料，CPU 與硬體通訊之延遲將抵銷硬體加速效益。
* **向量擴充處理器 (RVV)**：
  設計為「廣度平行運算 (Wide Parallel - SIMD)」。具備寬向量暫存器以平行處理陣列資料。若僅處理單一資料，多數運算單元將處於閒置，無法展現向量設計優勢。

因此，RVV 之總運算量較高，旨在展示平行處理陣列資料之效能潛力。

#### 2. Golden Cycle Count 差異 (微架構與 SoC 配置)
相同的 C 語言純軟體實作 (`lfsr_sw`) 在不同環境下之週期數浮動原因：
* **SoC 硬體配置 (Config)**：掛載不同硬體時，TileLink 匯流排節點與拓樸結構不同，影響記憶體存取延遲。
* **快取未命中與記憶體對齊 (Cache Miss & Alignment)**：二進位檔案大小變化改變指令在記憶體中之對齊，影響 L1 Instruction Cache 命中率及分支預測狀態。
* **RTL 模擬精度**：RTL 模擬器為 Cycle-Accurate，精確記錄管線停頓 (Pipeline Stall) 與快取延遲之影響。

---

## 6. 實戰除錯：產生與分析硬體波形圖 (Waveform Debugging)

若硬體執行週期異常或卡死，可輸出波形圖 (Waveform) 進行除錯。

### 1. 格式差異
* **VCD (Value Change Dump)**：開源模擬器預設支援，檔案較大。
* **FSDB (Fast Signal Database)**：商業壓縮格式 (需專屬 EDA 工具)。

### 2. 生成波形 (Verilator)
**編譯 Debug 版模擬器**：
```bash
cd chipyard/sims/verilator
make -j16 CONFIG=RocketConfig debug
```

**輸出波形檔**：
```bash
make run-binary-debug CONFIG=RocketConfig BINARY=test.riscv
```

### 3. 使用 GTKWave 檢視
```bash
# Ubuntu 安裝指令
sudo apt-get install gtkwave

# 開啟波形檔
gtkwave output/test.vcd
```
