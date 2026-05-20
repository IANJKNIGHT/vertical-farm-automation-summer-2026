# Real-Time System Timer & ADC Driver (RP2350)

An interrupt-driven, bare-metal state machine built utilizing the Pico SDK. 

## Peripheral Architecture & Pin Mapping
* **Keypad Columns (Triggers):** GPIO 6 & GPIO 7 (Configured as Input, Interrupted on `GPIO_IRQ_EDGE_RISE`)
* **State Verification LEDs:** GPIO 22, 23, 24, 25 (Configured as SIO Outputs)
* **ADC Dual Channels:** Channel 2 (GPIO 42) & Channel 5 (GPIO 45)
* **DMA Channel:** Assigned dynamically via pool claiming (`dma_claim_unused_channel`)

## Critical Synchronization Rules

### 1. ADC & DMA Alignment
To prevent index shifting inside the multi-channel Ping-Pong raw data buffers, initialization must adhere to a strict sequence:
1. Halt the ADC engine via `adc_run(false)`.
2. Clear and exhaust the internal ADC hardware FIFO.
3. Completely initialize and arm the Master DMA channel.
4. Establish the round-robin channel mask and select the starting channel explicitly.
5. Fire `adc_run(true)` to release the system clock cleanly.

### 2. Single-Handler Blink System
LED blinking routines utilize a single, fixed ISR hook registered to `TIMER0_IRQ_0` (Alarm 0). Blinking frequencies and visual patterns are determined safely via an internal step state counter rather than vector table modifications or floating-point calculations to avoid core execution instability.
