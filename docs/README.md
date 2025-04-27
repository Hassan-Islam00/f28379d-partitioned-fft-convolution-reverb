# Real‑Time DSP Convolution Reverb on TI TMS320F28379D
_Uniform‑Partitioned FFT • 48 kHz • 512‑Sample Blocks_

## Overview
This repository contains source code that implements a **real‑time impulse‑response (IR) reverb** on Texas Instruments’ C2000 **TMS320F28379D** microcontroller.  

Key elements:

* **Uniform‑partitioned FFT convolution** (12 × 512‑sample partitions)
* **Ping‑pong DMA buffering** for uninterrupted audio capture and playback
* **TI‑RTOS / SYS/BIOS** for scheduling
* TI’s hardware‑accelerated **C28x FPU** FFT library (`CFFT_f32()`)

The algorithm processes audio at 48 kHz and occupies approximately **42 % of the CPU**.

---

## Features
* Real‑time IR convolution with ~128 ms total IR length (6144 samples)
* Deterministic 512‑sample (≈ 10.7 ms) latency
* Runs under **TI‑RTOS / SYS/BIOS** with separate Hwi, Swi, and Task threads
* MATLAB to partition arbitrary IRs into frequency‑domain blocks

---

## Hardware & Software
| Component | Version / Notes |
|-----------|-----------------|
| MCU | **TMS320F28379D** (LAUNCHXL‑F28379D) |
| Audio codec | On‑board **TLV320AIC3254** @ 48 kHz, 16‑bit |
| IDE | Code Composer Studio **12.0** or newer |
| SDK | C2000Ware **4.x** |
| RTOS | **TI‑RTOS / SYS/BIOS** 6.x (bundled with C2000Ware) |
| Compiler | TI C28x **v22.x** |
| MATLAB | R2024b (for IR preprocessing) |

---

## Algorithm Summary
### 1 Ping‑Pong Buffering
The ADC DMA fills _Buffer A_ while the CPU processes _Buffer B_; the roles swap every 512 samples.

### 2 Uniform Partitioning
The IR is pre‑split into **P = 12** equal partitions of 512 samples. Each partition is zero‑padded to 1024 samples and stored as a complex spectrum

### 3 Per‑Block Processing
```text
1. Pad current 512‑sample block to 1024 samples
2. X[k]  = FFT( x[n] )
3. Y[k]  = Σₚ X_{p}[k] × IR_FFT[p][k]
4. y[n]  = IFFT( Y[k] )            ← overlap‑add to output buffer
```

A good source explaining the algorithm used 
<https://thewolfsound.com/fast-convolution-fft-based-overlap-add-overlap-save-partitioned/>

### 4 Performance Metrics
| Metric | Value |
|--------|-------|
| Sample rate | 48 kHz stereo |
| Block size | 512 samples |
| FFT size | 1024 points |
| IR length | 6144 samples |
| Latency | ≈ 10.7 ms |
| CPU load | 42.58 % (measured) |
| SRAM usage | ~96 kB |


## Memory Limitations & Extended IR Length
The F28379D provides constrains the maximum IR length to **≈ 128 ms at 48 kHz**.

For longer reverbs (e.g., concert‑hall tails > 1 s) the same algorithm has been ported to an **STM32H7** platform with external SDRAM and dual‑bank DTCM, enabling even longer IR lengths and even lower latency performance. See the repository:

<https://github.com/Hassan-Islam00/stm32h750-partitioned-fft-convolution-reverb>

---
