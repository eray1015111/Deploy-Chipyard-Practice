# RISC-V LFSR Architecture Report

## Scope

This project compares several ways to run the same 32-bit LFSR workload on a Chipyard-based RISC-V system. The workload is intentionally small, which makes it useful for checking hardware/software interfaces, cycle counting, and accelerator integration.

## Golden Model

All implementations should match this C model:

```c
uint32_t lfsr_step(uint32_t state)
{
    return (state & 1U) ? ((state >> 1) ^ 0x80200003U) : (state >> 1);
}
```

The seed `0x12345678` and the same step count are used across the demos unless a test explicitly states otherwise.

## Architecture Summary

| Architecture | Interface | Strength | Limitation |
| --- | --- | --- | --- |
| Pure C | Rocket core only | Simple golden model | Slowest path for repeated scalar work |
| MMIO LFSR | TileLink register block | Easy to integrate and debug | Polling and MMIO traffic add overhead |
| RoCC | Custom instruction | Low-latency scalar accelerator path | Requires custom opcode support and tighter CPU coupling |
| RVV | Vector intrinsics | Processes many independent streams in parallel | Not a custom LFSR RTL block; speedup depends on vector length and data layout |
| DMA LFSR | TileLink master | Demonstrates memory-side accelerator flow | More complex control path and cache/coherency considerations |

## LFSR Correctness Check

The reviewed implementations now use the same transition rule:

- Pure C: `lfsr_step()` in each demo.
- MMIO blackbox: `LFSRMMIOBlackBox.v` uses `(lfsr_result >> 1) ^ 32'h80200003` when bit 0 is set.
- RoCC: `LFSRRoCC.scala` uses the same conditional right-shift/XOR operation.
- DMA: `LFSRDMA.scala` uses the same conditional right-shift/XOR operation before writing the result back to memory.
- RVV: `rvv_test.c` computes the same operation lane-wise with vector intrinsics.

The polynomial is therefore consistent across the software model, MMIO accelerator, RoCC accelerator, RVV vector implementation, and DMA accelerator.

## Project Assessment

The project is a good educational scaffold for comparing RISC-V accelerator attachment styles. It covers the main categories students usually need to understand: software baseline, MMIO peripheral, tightly coupled RoCC, vector execution, and a TileLink master/DMA-style peripheral.

The code is not yet a production-quality accelerator package. The main gaps are automated tests, CI, version pinning beyond Chipyard checkout guidance, and stronger documentation of the bus protocol assumptions. The DMA path in particular needs careful validation in a full Chipyard simulation because memory ordering and cache visibility can affect real systems.

## Recommended Next Steps

1. Add a small host-side reference script that prints expected LFSR values for fixed seeds and step counts.
2. Add simulation checks that fail automatically on any mismatch.
3. Record cycle counts in machine-readable logs.
4. Document MMIO register maps and RoCC instruction encoding in one place.
5. Add waveform examples for one passing MMIO run and one DMA run.
