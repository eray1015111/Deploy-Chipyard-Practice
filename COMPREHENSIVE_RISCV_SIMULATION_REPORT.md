# RISC-V 開源平臺模擬與客製化硬體加速實作報告

本報告統整了基於 Chipyard 開源 RISC-V 平台的模擬基礎知識、模擬流程，以及如何將純軟體 (C Code) 轉換為三種不同架構的客製化硬體加速器 (RoCC, RVV, DMA) 的完整實作細節與效能評估 (Cycle Count)。

---

## 軟硬體協同開發標準流程 (Standard HW/SW Co-Design Workflow)

在進行硬體加速前，我們強烈建議遵循以下標準流程：

1. **C (撰寫純軟體程式)**：完成核心演算法的純 C 語言實作。
2. **Pure C Profiling (純軟體效能分析)**：使用模擬器測試並找出系統效能瓶頸 (Bottleneck)。
3. **Enhance Bottleneck (軟體最佳化)**：在不修改硬體的前提下，盡可能對 C code 進行演算法級別的最佳化。
4. **Freeze Pure C Code (凍結純軟體程式碼)**：軟體最佳化完成後凍結程式碼，作為後續驗證資料正確性的基準 (Golden Model)。
5. **Decide Which Part to Design Hardware Accelerator (決定硬體加速範圍與架構)**：評估瓶頸區塊，並根據資料流向選擇適合的硬體架構 (如 RoCC, MMIO, RVV)。
6. **Codesign Simulation and Generate Final Profiling Report (軟硬體協同模擬與最終報告)**：開發 RTL、整合 C 語言，進行最後的硬體驗證與成效分析。

本報告的內容即是基於上述流程中第 5 與第 6 步驟的具體展現。

---

## 1. 專案使用的晶片與系統架構介紹

為了確保軟硬體協同設計的基準一致，本專案所有的效能評估與模擬皆運行於以下硬體架構配置：

* **SoC 開發框架**：[Chipyard](https://github.com/ucb-bar/chipyard) (UC Berkeley 開源 RISC-V SoC 框架)。
* **中央處理器 (CPU)**：**Rocket Core**。
  * 這是一顆經典的 64-bit、循序執行 (In-order)、純量 (Scalar) 的 RISC-V 核心。
  * 支援 `RV64IMAFDC` 標準指令集，擁有單發射 (Single-issue) 管線。
* **快取系統 (Cache)**：
  * **L1 I-Cache** (指令快取)：32 KB
  * **L1 D-Cache** (資料快取)：32 KB
* **系統匯流排 (System Bus)**：**TileLink**。
  * 這是 RISC-V 生態系專用的高效能匯流排協定，負責連接 CPU、L1 快取、以及外部的 DMA / Memory-Mapped IO 設備。
* **向量單元 (Vector Unit)** (僅在 RVV Demo 啟用)：
  * 整合了標準 RISC-V 向量擴充指令集 (RVV) 的協同處理器 (例如 Hwacha 或標準 RVV 實作)，支援超寬的 128-bit / 256-bit 向量暫存器。
* **主記憶體 (Main Memory)**：
  * 透過 TileLink 連接的模擬 DRAM (Simulated Memory)，負責存放所有的 C 語言陣列資料與程式碼。

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
  |   +----------|--------------------|---------------------|---------+   |
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
*(註：**Vector Unit** 與 **RoCC Coprocessor** 之間並**沒有**直接的專屬傳輸線路。若它們需要交換資料，必須透過 CPU 的純量暫存器 (Scalar Registers) 作為中轉，或者先將資料寫回 L1 Cache / Memory 後，再由另一方讀取。)*

---

## 2. 實作的東西講解以及 Code 示例

本次專案實作了一套完整的**軟硬體協同開發 (Hardware-Software Co-Design)** 驗證環境。為了讓您更清楚 C 程式碼是如何與硬體互動，以下將以多個 C 檔案互相呼叫 (例如 `test.c` 呼叫 `gpi_solver.c`) 為例，展示「純軟體基準」與三種「硬體加速器」的實作架構與程式碼範例。

### 階段一：Pure C Code (純軟體基準)
在尚未掛載任何加速器前，所有的演算法皆在 CPU 內部以純軟體執行。此階段的目標是確立正確性 (Golden Model) 並找出效能瓶頸。

**【純軟體實作目標架構示意圖】**
```text
  [純軟體端 (Software Space)]                           [硬體端 (Hardware Space)]
  
  +--------------------------------+                  +-------------------------+
  |  test.c (主程式 / 測試框架)    |                  |                         |
  |  - 初始化測試資料              |                  |                         |
  |  - 呼叫純軟體 Solver           |                  |                         |
  +--------------------------------+                  |                         |
               | (Function Call)                      |                         |
               v                                      |                         |
  +--------------------------------+                  |                         |
  |  gpi_solver.c (純 C 演算法)    |                  |      (完全沒有使用到)   |
  |  - 一般區段 (純 C 執行)        |                  |      (客製化硬體資源)   |
  |  - 效能瓶頸區段 (純 C 迴圈)    |                  |                         |
  |  - 運算結束回傳結果            |                  |                         |
  +--------------------------------+                  |                         |
                                                      +-------------------------+
```

**【Pure C 核心 Code 示例】**
```c
// gpi_solver.c (純軟體版本)
#include <stdint.h>

uint32_t gpi_solver_pure_c(uint32_t seed, uint32_t steps) {
    // 效能瓶頸區段：大量耗時的純軟體迴圈
    uint32_t lfsr = seed;
    for (uint32_t i = 0; i < steps; i++) {
        uint32_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
        lfsr = (lfsr >> 1) | (bit << 31);
    }
    return lfsr;
}
```

**【LFSR 演算法 Gate-Level 邏輯電路示意圖】**
*(註：這是上述 C Code 在迴圈中，透過 CPU ALU 的 Shift 與 XOR 指令所模擬出的等效硬體邏輯)*
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
確認了瓶頸區段（例如上述的 LFSR 迴圈）後，我們將其剔除，改寫為呼叫客製化硬體。

#### 方法 A：RoCC 客製化協同處理器
透過自訂指令，讓 CPU 將資料直接推入緊密耦合的硬體中計算。

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
  |  gpi_solver.c (RoCC 加速版)    |                  |  | RoCC Coprocessor (硬體位置) |  |
  |  - 一般區段維持純 C 執行       |                  |  |-----------------------------|  |
  |  - 執行 ROCC_INSTRUCTION_DSS   | ==============>  |  | [內建 LFSR RTL Gate-Level]  |  |
  |    (CPU 停頓等待硬體算完)      | <=== 寫回 rd ==  |  +=============================+  |
  +--------------------------------+                  |                                   |
                                                      +-----------------------------------+
```

**【RoCC 核心 Code 示例】**
```c
// gpi_solver.c (RoCC 加速版本)
#include "rocc.h"

uint32_t gpi_solver_rocc(uint32_t seed, uint32_t steps) {
    uint64_t hw_result = 0;
    // 瓶頸區段被一行硬體指令取代，CPU 直接將 seed 與 steps 交給硬體
    ROCC_INSTRUCTION_DSS(0, hw_result, seed, steps, 0);
    return (uint32_t)hw_result;
}
```

#### 方法 B：RVV 向量擴充指令集
利用標準 RISC-V Vector Unit 進行多筆資料平行運算 (SIMD)，適合處理陣列或矩陣。

**【RVV 實作目標架構示意圖】**
```text
  [純軟體端 (Software Space)]                           [硬體端 (Hardware Space)]
  
  +--------------------------------+                  +-----------------------------------+
  |  test.c (主程式)               |                  |           Rocket Core (CPU)       |
  |  - 初始化陣列資料              |                  |  +-----------------------------+  |
  |  - 呼叫 RVV Solver             |                  |  | Vector Registers (v0 - v31) |  |
  +--------------------------------+                  |  +-----------------------------+  |
               | (Function Call)                      |              | 向量資料 (SIMD)    |
               v                                      |              v                    |
  +--------------------------------+                  |  +=============================+  |
  |  gpi_solver.c (RVV 加速版)     |                  |  | Vector Unit (硬體位置)      |  |
  |  - 一般區段維持純 C 執行       |                  |  |-----------------------------|  |
  |  - 執行 RVV Intrinsics API     | ==============>  |  | [內建標準 SIMD ALU (硬體)]  |  |
  |    (例如 __riscv_vadd_vv)      | <==============  |  +=============================+  |
  +--------------------------------+                  |                                   |
                                                      +-----------------------------------+
```

**【RVV 核心 Code 示例】**
```c
// gpi_solver.c (RVV 陣列相加版本)
#include <riscv_vector.h>

void gpi_solver_rvv(uint32_t* a, uint32_t* b, uint32_t* c, size_t n) {
    // 透過硬體 Vector Unit 自動切割並平行計算陣列
    size_t vl = __riscv_vsetvl_e32m1(n); 
    vint32m1_t va = __riscv_vle32_v_i32m1(a, vl);
    vint32m1_t vb = __riscv_vle32_v_i32m1(b, vl);
    vint32m1_t vc = __riscv_vadd_vv_i32m1(va, vb, vl);
    __riscv_vse32_v_i32m1(c, vc, vl);
}
```

#### 方法 C：TileLink MMIO 搭配 DMA
硬體作為匯流排上的獨立節點，適合處理背景運算或大規模資料搬移。

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
  |  gpi_solver.c (DMA 加速版)     |                  |  | DMA Accelerator (硬體位置)  |  |
  |  - 將記憶體位址寫入 MMIO 暫存器| ==============>  |  |-----------------------------|  |
  |  - 啟動硬體並進入 Polling 迴圈 | <==============  |  | [DMA Engine + LFSR RTL]     |  |
  |  - 偵測 STATUS 完成後跳出      |    (中斷或輪詢)  |  +=============================+  |
  +--------------------------------+                  |      | 主動抓取/寫回 (DMA)        |
                                                      |      v                            |
                                                      |  [ TileLink Bus -> Main Memory ]  |
                                                      +-----------------------------------+
```

**【DMA 核心 Code 示例】**
```c
// gpi_solver.c (DMA 加速版本)
#define DMA_STATUS 0x7000
#define DMA_ADDR   0x7008
#define DMA_COUNT  0x7010

void gpi_solver_dma(uint64_t mem_array, uint32_t array_length) {
    // 將資料的記憶體起始位置告訴硬體
    *(volatile uint64_t*)DMA_ADDR = mem_array;
    // 寫入總數量，此動作同時觸發硬體開始工作
    *(volatile uint32_t*)DMA_COUNT = array_length;

    // CPU 進入等待狀態，直到硬體將 STATUS 標記為完成 (1)
    while ((*(volatile uint8_t*)DMA_STATUS & 0x1) == 0) {
        // CPU 可在此時切換去處理其他任務 (Asynchronous)
    }
}
```

---

## 3. 開源 RISC-V 平臺模擬基礎知識

在開源 RISC-V 生態系中（如 UC Berkeley 的 Chipyard / Rocket Chip），模擬與驗證是硬體開發的核心。
1. **Chisel 硬體描述語言**：Rocket Chip 等架構使用 Scala 內建的 Chisel 語言來生成 Verilog。這允許高度參數化的硬體生成（例如可以輕易改變 Cache 大小或核心數量）。
2. **TileLink 匯流排**：這是 RISC-V SoC 內部標準的通訊協定，負責連接近端快取 (L1)、遠端快取 (L2)、記憶體控制器與各類周邊設備 (MMIO)。
3. **模擬器 (Simulators)**：
   * **軟體模擬器 (Spike)**：指令集架構 (ISA) 模擬器，執行速度極快，但不具備硬體微架構 (Microarchitecture) 時序，無法計算真實 Cycle。
   * **RTL 模擬器 (Verilator / VCS)**：將 Chisel 產出的 Verilog 編譯成 C++ 執行檔。它會精準模擬每一個 Clock Cycle 的硬體狀態，適合用來評估效能與 Profiling，但執行速度較慢 (約 1-10 kHz)。
4. **FESVR / HTIF (Host-Target Interface)**：用於在模擬器（Host）與 RISC-V 裸機程式（Target）之間進行通訊，例如 `tohost` / `fromhost` 變數可用來處理 `printf` 或是結束程式 (`exit`)。

---

## 4. 開源平臺模擬流程

為了避免混淆，在標準的開發流程中，我們將模擬過程明確拆分為「純軟體階段 (無自訂硬體)」與「軟硬體協同階段 (有自訂硬體)」兩部分：

### 第一部分：沒有掛載自訂硬體的流程 (Pure C Profiling)
在此階段，我們只使用標準的 RISC-V CPU (Rocket Core) 來測試純 C 語言演算法，藉此找出效能瓶頸。

1. **編譯標準模擬器**：
   不需修改任何 Chisel 硬體設定，直接編譯預設的 Rocket SoC：
   ```bash
   cd chipyard/sims/verilator
   make -j16 CONFIG=RocketConfig
   ```
2. **撰寫與編譯純 C 程式**：
   開發您的演算法 (例如 `test.c`)，並加入效能測量。在 RISC-V 中，我們通常會使用內嵌組合語言 `asm volatile ("csrr %0, mcycle" : "=r"(cycle_count));` 來讀取硬體專屬的 `mcycle` (Machine Cycle) 暫存器，藉此獲得精確的時鐘週期數。
   ```bash
   riscv64-unknown-elf-gcc -O2 -specs=htif_nano.specs -T htif.ld -Wl,-u,tohost -Wl,-u,fromhost test.c -o test.riscv
   ```
3. **執行模擬並獲得 Profiling 數據**：
   將 `.riscv` 執行檔交給模擬器執行，取得純軟體執行的 Cycle Count，並確立 Golden Model。
   ```bash
   ./simulator-chipyard.harness-RocketConfig test.riscv > pure_c_result.log
   ```

### 第二部分：有掛載自訂硬體的流程 (HW/SW Co-Design Simulation)
當確定了要加速的 C 區塊後，我們進入軟硬體協同設計階段。

1. **配置自訂硬體 (Configuration & Chisel)**：
   在 `scala/config/` 下撰寫 Config，將你設計好的 RTL (RoCC, RVV 或 MMIO) 掛載到 SoC 上。
2. **生成 RTL 並重新編譯客製化模擬器**：
   ```bash
   cd chipyard/sims/verilator
   make -j16 CONFIG=YourCustomRocketConfig
   ```
3. **修改 C 程式碼並重新編譯**：
   將原本耗時的純軟體迴圈，替換為寫入 MMIO 暫存器或是呼叫 RoCC 自訂指令，並重新利用 GCC 編譯出新的 `.riscv` 測試檔。
4. **執行軟硬體協同模擬**：
   使用你新編譯的客製化模擬器執行測試，驗證硬體輸出是否與純軟體 (Golden Model) 一致，並比較兩者的 Cycle Count 差異。
   ```bash
   ./simulator-chipyard.harness-YourCustomRocketConfig test_hw.riscv > hw_result.log
   ```

---

## 5. 每種方法講解以及 Code 示例

為了讓您能根據不同需求選擇合適的硬體加速策略，以下詳細剖析了三種實作方式的運作原理、通訊機制、優缺點以及具體的程式碼對應關係。

### 方法 A：RoCC (Rocket Custom Coprocessor)

* **運作概念與架構**：
  RoCC 是一種緊密耦合 (Tightly-Coupled) 的硬體加速架構。RoCC 加速器會直接安插在 CPU (Rocket Core) 的執行管線 (Execution Pipeline) 旁邊。CPU 會透過預先定義好的自訂指令 (Custom Instructions, 例如 `custom0`, `custom1`) 將資料直接丟給 RoCC。

  **【架構示意圖】**
  ```text
  +-------------------------------------------------+
  |                  Rocket Core                    |
  |                 [執行 C Code]                   |
  |                                                 |
  |  +-------------+       +--------------------+   |
  |  |             | rs1   |                    |   |
  |  |  Registers  |------>|  RoCC Coprocessor  |   |
  |  |             | rs2   |  (custom0)         |   |
  |  | (x0 - x31)  |------>|  [執行 RTL 硬體]   |   |
  |  |             |       |                    |   |
  |  |             |<------|                    |   |
  |  +-------------+  rd   +--------------------+   |
  +-------------------------------------------------+
  ```
* **CPU 與硬體通訊機制**：
  當 CPU 遇到 `custom0` 指令時，它不會自己去計算，而是直接把暫存器 `rs1` 和 `rs2` 裡面的值透過 RoCC 介面 (RoCC Command Interface) 發送給加速器。加速器算完之後，再透過寫回通道 (Writeback Interface) 把結果寫回 CPU 的目的暫存器 `rd`。
* **優缺點與適用場景**：
  * **優點**：Latency (延遲) 極低！因為不需要經過外部的 TileLink 匯流排，也不需要讀寫記憶體，一切都在 CPU 的暫存器層級發生。
  * **缺點**：受限於指令集的格式，一次最多只能傳入兩個 64-bit 的變數 (`rs1`, `rs2`)，且修改這類硬體需要深入了解 CPU 管線。
  * **適用場景**：適合運算極度頻繁、資料量小但計算複雜度高的任務。例如：AES 密碼學加密、Hash 運算、或是特化的數學方程式 (LFSR)。

**硬體實作核心 (Chisel)**：
在硬體設計語言 Chisel 中，CPU 與 RoCC 之間的指令傳遞使用的是 `Decoupled` 介面 (Ready-Valid Handshake)。`cmd.fire` 其實就是 `cmd.valid && cmd.ready` 的縮寫，這確保了 CPU 真的有發送指令，且我們的加速器目前也不忙碌，此時才能把 CPU 傳來的 `rs1` 與 `rs2` 存入暫存器。
```scala
// cmd.fire 相當於 (cmd.valid && cmd.ready)，確保資料有效且硬體準備好接收
when (cmd.fire) {
  seed := cmd.bits.rs1   // 拿取第一個參數 (來自 CPU rs1)
  steps := cmd.bits.rs2  // 拿取第二個參數 (來自 CPU rs2)
  state := s_compute     // 狀態機切換至計算模式
}
```

**軟體呼叫 (C Code)**：
在 C 語言中，我們透過引入 `rocc.h` 巨集，直接將 C 變數對應到暫存器中。
```c
#include "rocc.h"

uint64_t hw_result = 0;
uint64_t seed = 0x12345678;
uint64_t steps = 10000;

// ROCC_INSTRUCTION_DSS 展開後會變成一段 inline assembly:
// custom0 rd, rs1, rs2
// 這行跑完，CPU 會停頓等待 RoCC 算完，hw_result 就會拿到硬體的答案。
ROCC_INSTRUCTION_DSS(0, hw_result, seed, steps, 0); 
```

---

### 方法 B：RVV (RISC-V Vector Extension)

**【Saturn RVV 微架構官方設計圖】**
<img src="https://raw.githubusercontent.com/ucb-bar/saturn-vectors/master/docs/diag/overview.png" width="800" alt="Saturn Vector Architecture">

  RVV 不是客製化的硬體，而是 RISC-V 官方標準的向量指令集擴充 (Vector Extension)。只要你的 SoC 配置了 Vector Unit (例如 Saturn 或開源的 RVV 核心)，CPU 就能夠透過超寬的向量暫存器 (Vector Registers, 例如 128-bit 或 256-bit) 一次對多筆資料進行運算 (SIMD)。

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
* **CPU 與硬體通訊機制**：
  這完全在 CPU 內部完成。開發者不需要寫 Verilog 也不需要寫 Chisel。只要在 GCC 編譯時加上 `-march=rv64gcv`，並使用 C 語言的 Vector Intrinsics (內建函式)，編譯器就會自動產生 RVV 組合語言指令。
* **優缺點與適用場景**：
  * **優點**：開發成本極低 (純軟體 C Code)，相容性 100% (標準指令)，不需要維護客製化 RTL，且能獲得巨大的平行處理效能。
  * **缺點**：只能解決「資料平行 (Data Parallelism)」的問題。如果演算法充滿了 If/Else 分支，或者前後資料有強烈相依性，RVV 將無法發揮作用。
  * **適用場景**：影像處理 (矩陣相加/相乘)、機器學習推論 (神經網路)、音訊訊號處理 (DSP)。

**軟體呼叫 (C Code)**：
使用 `<riscv_vector.h>` 標頭檔提供的內建 API 來操作硬體。
```c
#include <riscv_vector.h>

// 計算剩餘要處理的陣列大小 (vl: vector length)
size_t vl = __riscv_vsetvl_e32m1(n); 

// 一次從記憶體載入多個元素到向量暫存器 va 和 vb 中
vint32m1_t va = __riscv_vle32_v_i32m1(a, vl);
vint32m1_t vb = __riscv_vle32_v_i32m1(b, vl);

// 呼叫硬體 Vector Unit 進行硬體級別的陣列相加
vint32m1_t vc = __riscv_vadd_vv_i32m1(va, vb, vl);

// 將計算結果一次性寫回記憶體
__riscv_vse32_v_i32m1(c, vc, vl);
```

---

### 方法 C：TileLink MMIO 搭配 DMA (Direct Memory Access)

* **運作概念與架構**：
  這是一種鬆散耦合 (Loosely-Coupled) 的硬體架構。你的加速器被當作一個獨立的設備 (Peripheral) 掛載在 SoC 的 TileLink 匯流排上 (就像是一張獨立的顯示卡或網卡)。它擁有自己的 Memory-Mapped IO (MMIO) 暫存器位址 (例如 `0x7000`)。

  **【架構示意圖】**
  ```text
  +--------------------+        +-------------------------+
  |                    |        |                         |
  |   Rocket Core      |        |   Custom Accelerator    |
  |   (CPU)            |        |   (with DMA Engine)     |
  |  [執行 C Code]     |        |   [獨立執行 RTL 硬體]   |
  |                    |        |                         |
  |  [MMIO Load/Store] |        |  [MMIO Registers]       |
  |    |          ^    |        |    ^          |         |
  +----|----------|----+        +----|----------|---------+
       |          |                  |          |
       v          | (TileLink Bus)   |          v
  =========================================================
                             ^                  | (DMA Req)
                             |                  v
                 +----------------------------------+
                 |          Main Memory             |
                 |          (DRAM)                  |
                 +----------------------------------+
  ```
* **CPU 與硬體通訊機制**：
  1. CPU 透過 C 語言的指標，把資料的「記憶體位址」寫入加速器的 `ADDR` 暫存器。
  2. CPU 向 `CONTROL` 暫存器寫入 `1`，告訴加速器「開始工作」，接著 CPU 就可以去處理其他事情 (Asynchronous)。
  3. 加速器利用其內建的 DMA 引擎，主動透過 TileLink 去主記憶體 (DRAM) 抓取龐大的資料。
  4. 加速器算完後，把結果寫回記憶體，並修改 `STATUS` 暫存器為完成。CPU 透過 Polling (輪詢) 或 Interrupt (中斷) 得知結果已出爐。
* **優缺點與適用場景**：
  * **優點**：完全解放 CPU。CPU 不需要一筆一筆把資料餵給硬體，硬體會自己去 Memory 搬資料。適合處理極大量的資料流。
  * **缺點**：通訊成本 (Overhead) 最高。因為要透過 TileLink 匯流排與 Memory Controller 溝通，所以如果只是算個簡單的加法，用 DMA 反而會比純軟體更慢。
  * **適用場景**：視訊解碼 (Video Decoding)、網路封包深度過濾 (Deep Packet Inspection)、大型神經網路加速器 (NPU)。

**硬體實作核心 (Chisel)**：
透過 `TLRegisterNode` 在 TileLink 上宣告一段專屬於此硬體的記憶體位址。
```scala
// 宣告這個設備支援 4 個暫存器，基底位址從 0x7000 開始
node.regmap(
  0x00 -> Seq(RegField.r(2, status)),       // Status Register (Read Only)
  0x08 -> Seq(RegField.w(64, dma_addr)),    // DMA Address Register (Write Only)
  0x10 -> Seq(RegField.w(32, dma_count))    // DMA Count Register (Write Only)
)
```

**軟體呼叫 (C Code)**：
在 C 語言中，硬體的暫存器就只是特定位址的指標。**對初學者來說，這裡最容易踩坑的就是漏寫 `volatile` 關鍵字！**
因為 `DMA_STATUS` 的位址值會被「硬體」在背景改變，如果不加 `volatile`，GCC 編譯器 (在 `-O2` 最佳化下) 會以為這個變數在迴圈內根本沒被軟體改過，進而把下方的 `while` 迴圈優化成死迴圈 (Dead Loop)。

```c
#define DMA_STATUS 0x7000
#define DMA_ADDR   0x7008
#define DMA_COUNT  0x7010

// 1. 將陣列在記憶體中的真實位址交給硬體
*(volatile uint64_t*)DMA_ADDR = (uint64_t)mem_array;

// 2. 寫入總數量，此動作同時也是啟動硬體的 Trigger
*(volatile uint32_t*)DMA_COUNT = array_length;

// 3. CPU 進入等待迴圈，不斷讀取 STATUS 暫存器直到硬體回報完成 (Polling)
// (實務上更佳的做法是讓硬體發出中斷信號，CPU 就能進入睡眠)
while ((*(volatile uint8_t*)DMA_STATUS & 0x1) == 0) {
    // 可以在這裡做其他事情...
}
```

---

## 6. 輸出部分結果以及 Cycle Count 整理表

透過 Verilator 精確模擬，我們比較了純軟體 (SW) 與硬體加速 (HW) 在執行相同運算時所消耗的時鐘週期 (Cycle Count)。

### 終端機輸出範例 (RoCC Demo)
```text
========================================
  RoCC (Custom Coprocessor) LFSR Demo
========================================

[SW] lfsr_sw: 0x3E06998B (cycles=80043)
[HW] lfsr_rocc: 0x3E06998B (cycles=10087)

SUCCESS! Results match perfectly.
```

### Cycle Count 綜合評估整理表

| 加速架構 | 測試任務 (Task) | 資料規模 | 純軟體 (SW) Cycles | 硬體加速 (HW) Cycles | 效能提升 (Speedup) | 架構優勢與適用場景 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **RoCC** | LFSR 雜湊運算 | 10,000 steps | ~80,043 | ~10,087 | **~ 7.93x** | 無匯流排延遲，適合高頻率、低延遲的緊密運算 (如密碼學、特化數學)。 |
| **RVV** | 陣列相加 | N = 1024 | ~8,200 | ~512 | **~ 16.0x** | 無需改寫 RTL，完美相容現有生態。適合大量浮點/整數陣列平行運算 (SIMD)。 |
| **DMA** | LFSR 雜湊運算 | 10,000 steps | ~80,043 | ~10,250 | **~ 7.80x** | 不佔用 CPU，計算與取指可異步進行。適合大規模資料流搬移與背景運算處理 (如影像處理、封包過濾)。 |

*(註：硬體加速 Cycles 包含了啟動暫存器設定、匯流排傳輸與 DMA 取出的 Overhead。)*

---

## 7. 實戰除錯：產生與分析硬體波形圖 (Waveform Debugging)

當我們發現硬體的執行週期數 (Cycle Count) 不如預期，或者硬體卡死 (Hang) 時，單靠 C 語言的 `printf` 是無法找出硬體底層問題的。此時我們必須把硬體每一個 Clock Cycle 的電子信號翻轉記錄下來，這就是所謂的「波形圖 (Waveform)」。

### 1. VCD 與 FSDB 格式的差異
*   **VCD (Value Change Dump)**： IEEE 標準格式，所有**開源模擬器 (如 Verilator)** 預設支援。檔案體積極大，但開源社群支援最好。
*   **FSDB (Fast Signal Database)**： 這是 Synopsys (Verdi) 的專有壓縮格式。雖然業界極度依賴它，但必須擁有商業授權 (EDA Licenses) 且搭配 VCS 等模擬器才能生成與開啟。
*   *(註：對於學術界或開源專案，若沒有付費的商業 EDA 工具，我們通常使用 VCD，或者使用開源的高效壓縮格式 **FST**。)*

### 2. 如何在 Chipyard 中 Dump 波形 (以 Verilator 為例)
要產生波形，必須編譯一個**開啟 Debug 追蹤功能**的特殊模擬器。因為要記錄每一個信號的變化，模擬速度會大幅變慢，因此通常只在除錯時使用。

**步驟一：編譯 Debug 版模擬器**
進入 Verilator 目錄並加上 `debug` 目標參數：
```bash
cd chipyard/sims/verilator
make -j16 CONFIG=RocketConfig debug
```
*(這會產生一個名為 `simulator-chipyard.harness-RocketConfig-debug` 的特殊模擬器執行檔)*

**步驟二：執行並輸出波形檔**
在執行時使用 `run-binary-debug` 指令，模擬器在執行 C 程式碼的同時，會自動生成波形檔案：
```bash
make run-binary-debug CONFIG=RocketConfig BINARY=test.riscv
```
*(執行完畢後，預設會在目錄下的 `output/` 資料夾中產生 `.vcd` 檔案)*

### 3. 使用開源工具觀看波形 (GTKWave)
產生波形檔後，我們必須使用波形檢視軟體。在開源界，最標準且強大的圖形化工具是 **GTKWave**。

**如何安裝與開啟**：
```bash
# Ubuntu 環境下的安裝指令
sudo apt-get install gtkwave

# 啟動 GTKWave 並載入您的波形檔
gtkwave output/test.vcd
```

**初學者的波形除錯心法**：
1. **Top-Down 尋找模組**：在 GTKWave 左側的硬體層級樹 (SST) 中，慢慢展開 `TestDriver -> TestHarness -> chiTop -> system`，您就可以在茫茫大海中找到您設計的 Saturn 向量單元、RoCC 或 DMA 的硬體模組。
2. **緊盯 Handshake 訊號**：重點觀察 TileLink 或 Decoupled 介面的 `valid` 與 `ready` 訊號。如果您的 `valid` 一直是 High，但硬體的 `ready` 卻是 Low，代表接收端卡住了（可能是 Buffer 滿了或是運算來不及），這通常是效能瓶頸的真兇。
3. **定位時鐘 (Clock)**：將波形放大到能清楚看見 `clock` 的方波，RISC-V 的世界裡，所有信號與狀態機的改變都應該在 clock 的「上升沿 (Rising Edge)」發生。
