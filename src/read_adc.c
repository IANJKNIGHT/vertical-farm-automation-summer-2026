#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

#define NUM_SAMPLES 1000
#define MASTER_BUFFER_SIZE (NUM_SAMPLES * 2) // 2000 elements total
// extern uint16_t adc_fan_speed_control[NUM_SAMPLES];
// extern uint16_t adc_fan_speed_control2[NUM_SAMPLES];
// extern volatile bool master_buffer_ready;
// extern int dma_master_chan;
// extern bool buffer1_full;
// extern bool buffer2_full;
// volatile uint16_t* completed_raw_buffer = NULL;

// extern int dma_fan_speed_chan;
// extern uint16_t master_buffer1[MASTER_BUFFER_SIZE];
// extern uint16_t master_buffer2[MASTER_BUFFER_SIZE];
// extern int current_buffer;
// void combined_dma_handler();

// void dma_fanspeed_handler_logic()
// {
//     if (current_buffer == 1)
//     {
//         dma_channel_set_write_addr(dma_fan_speed_chan, adc_fan_speed_control2, true);
//         current_buffer = 2;
//         buffer1_full = true;
//     }
//     else if (current_buffer == 2)
//     {
//         dma_channel_set_write_addr(dma_fan_speed_chan, adc_fan_speed_control, true);
//         current_buffer = 1;
//         buffer2_full = true;
//     }
// }

// void init_adc()
// {
//     // fill in
//     adc_init();
//     // gpio_init();
//     // uint32_t mask = 1u << (45 & 0x1fu);
//     // sio_hw->gpio_hi_oe_set = mask;
//     // sio_hw->gpio_clr = mask;
//     // GPIO_SET_FUNCTION
//     // Set new values for a sub-set of the bits in a HW register
//     int GPIO_NUM = 45;
//     hw_write_masked(&pads_bank0_hw->io[GPIO_NUM],
//                     PADS_BANK0_GPIO0_IE_BITS,
//                     PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
//     io_bank0_hw->io[GPIO_NUM].ctrl = GPIO_FUNC_NULL << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
//     pads_bank0_hw->io[GPIO_NUM] = 1u << 7;
//     // io_bank0_hw->io[45].ctrl =
//     // Atomically clear the specified bits to 0 in a HW register
//     hw_clear_bits(&pads_bank0_hw->io[GPIO_NUM], PADS_BANK0_GPIO0_ISO_BITS);
//     // adc_select_input(5);
//     // adc_get_selected_input();
//     // AINSEL: Select analog mux input. Updated automatically in round-robin mode. (Selected GPIO 26)
//     adc_set_clkdiv(0); // Set the ADC clock divider to 0 for maximum speed (48MHz)
    

//     // EN: Power on ADC and enable its clock. 1 - enabled. 0 - disabled
//     adc_hw->cs |= 1 << 0;
// }

// // void init_adc_combined_freerun() {

// //     adc_init();
// //     // 1. Initialize Pad/IO controls for GPIO 45 (Ch 5)
// //     int GPIO_45 = 45;
// //     hw_write_masked(&pads_bank0_hw->io[GPIO_45], PADS_BANK0_GPIO0_IE_BITS, PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
// //     io_bank0_hw->io[GPIO_45].ctrl = GPIO_FUNC_NULL << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
// //     hw_clear_bits(&pads_bank0_hw->io[GPIO_45], PADS_BANK0_GPIO0_ISO_BITS);

// //     // 2. Initialize Pad/IO controls for GPIO 42 (Ch 2)
// //     int GPIO_42 = 42;
// //     hw_write_masked(&pads_bank0_hw->io[GPIO_42], PADS_BANK0_GPIO0_IE_BITS, PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
// //     io_bank0_hw->io[GPIO_42].ctrl = GPIO_FUNC_NULL << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
// //     hw_clear_bits(&pads_bank0_hw->io[GPIO_42], PADS_BANK0_GPIO0_ISO_BITS);

// //     // 3. Configure the ADC FIFO
// //     adc_fifo_setup(
// //         true,  // Write each result to the FIFO
// //         true,  // Enable DMA data request (DREQ)
// //         1,     // DREQ asserted when at least 1 sample present
// //         false, // No ERR bit
// //         false  // No byte shift
// //     );

// //     // 4. Enable Round Robin for Channel 2 and Channel 5
// //     // Binary: 0010 0100 -> Enables Channel 5 and Channel 2
// //     adc_set_round_robin(0x24); 

// //     // Set the starting channel to Ch 2
// //     adc_select_input(2);

// //     adc_set_clkdiv(96);

// //     // 5. Start free-running conversion
// //     adc_hw->cs |= 1 << 3; // Set START_MANY bit to enable free-running mode
// //     adc_hw->cs |= 1 << 0; // Set EN bit to power on ADC
// // }

int get_max_adc_value(uint16_t *adc_result)
{
    int max = 0;
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        if (adc_result[i] > max)
        {
            max = adc_result[i];
        }
    }
    return max;
}

int get_min_adc_value(uint16_t *adc_result)
{
    int min = 4095;
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        if (adc_result[i] < min)
        {
            min = adc_result[i];
        }
    }
    return min;
}


// void master_dma_handler() {
//     // Clear the interrupt flag
//     dma_hw->ints0 = (1u << dma_master_chan);

//     // Switch destinations using Ping-Pong logic
//     if (current_buffer == 1) {
//         dma_channel_set_write_addr(dma_master_chan, master_buffer2, true);
//         current_buffer = 2;
//         completed_raw_buffer = master_buffer1; // Buffer 1 is ready for processing
//     } else {
//         dma_channel_set_write_addr(dma_master_chan, master_buffer1, true);
//         current_buffer = 1;
//         completed_raw_buffer = master_buffer2; // Buffer 2 is ready for processing
//     }
    
//     // Signal to main loop that processing can begin
//     master_buffer_ready = true;
// }

// void init_adc_combined_freerun() {
//     adc_init();
    
//     // 1. Initialize Pad/IO controls for GPIO 45 (Ch 5)
//     int GPIO_45 = 45;
//     hw_write_masked(&pads_bank0_hw->io[GPIO_45], PADS_BANK0_GPIO0_IE_BITS, PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
//     io_bank0_hw->io[GPIO_45].ctrl = GPIO_FUNC_NULL << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
//     hw_clear_bits(&pads_bank0_hw->io[GPIO_45], PADS_BANK0_GPIO0_ISO_BITS);

//     // 2. Initialize Pad/IO controls for GPIO 42 (Ch 2)
//     int GPIO_42 = 42;
//     hw_write_masked(&pads_bank0_hw->io[GPIO_42], PADS_BANK0_GPIO0_IE_BITS, PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
//     io_bank0_hw->io[GPIO_42].ctrl = GPIO_FUNC_NULL << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
//     hw_clear_bits(&pads_bank0_hw->io[GPIO_42], PADS_BANK0_GPIO0_ISO_BITS);

//     // 3. Configure the ADC FIFO
//     adc_fifo_setup(
//         true,  // Write each result to the FIFO
//         true,  // Enable DMA data request (DREQ)
//         1,     // DREQ asserted when at least 1 sample present
//         false, // No ERR bit
//         false  // No byte shift
//     );

//     // 4. Enable Round Robin for Channel 2 and Channel 5
//     adc_set_round_robin(0x24); 

//     // Set the starting channel to Ch 2
//     adc_select_input(2);
//     adc_set_clkdiv(96);

//     // === FIX 2: DRAINING THE HARDWARE FIFO BEFORE STARTING ===
//     // If a stray sample sits in the FIFO here, Ch 2 and Ch 5 will be permanently inverted!
//     adc_run(false);
//     while (!adc_fifo_is_empty()) {
//         adc_fifo_get();
//     }

//     // 5. Start free-running conversion
//     adc_hw->cs |= 1 << 0; // Set EN bit to power on ADC
//     adc_hw->cs |= 1 << 3; // Set START_MANY bit to enable free-running mode
// }

// void init_master_dma() {
//     // Force reset state tracking variable
//     current_buffer = 1;
//     completed_raw_buffer = NULL;

//     dma_channel_config c = dma_channel_get_default_config(dma_master_chan);
//     channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
//     channel_config_set_read_increment(&c, false);
//     channel_config_set_write_increment(&c, true);
//     channel_config_set_dreq(&c, DREQ_ADC);

//     dma_channel_configure(
//         dma_master_chan,
//         &c,
//         master_buffer1,         // Initial Destination
//         &adc_hw->fifo,          // Source
//         MASTER_BUFFER_SIZE,     // Transfer 2000 samples (1000 pairs)
//         true                    // Start immediately
//     );

//     // Bind handler exclusively to DMA_IRQ_0
//     dma_channel_set_irq0_enabled(dma_master_chan, true);
//     irq_set_exclusive_handler(DMA_IRQ_0, master_dma_handler);
//     irq_set_enabled(DMA_IRQ_0, true);
// }

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

#define NUM_SAMPLES 1000
#define MASTER_BUFFER_SIZE (NUM_SAMPLES * 2)

extern uint16_t master_buffer1[MASTER_BUFFER_SIZE];
extern uint16_t master_buffer2[MASTER_BUFFER_SIZE];
extern int dma_master_chan;
extern int current_buffer;
extern volatile bool master_buffer_ready;
extern volatile uint16_t* completed_raw_buffer;

void master_dma_handler() {
    // Clear the interrupt flag
    dma_hw->ints0 = (1u << dma_master_chan);

    // Switch destinations using Ping-Pong logic
    if (current_buffer == 1) {
        dma_channel_set_write_addr(dma_master_chan, master_buffer2, true);
        current_buffer = 2;
        completed_raw_buffer = master_buffer1; 
    } else {
        dma_channel_set_write_addr(dma_master_chan, master_buffer1, true);
        current_buffer = 1;
        completed_raw_buffer = master_buffer2; 
    }
    
    master_buffer_ready = true;
}

void init_adc_combined_freerun() {
    adc_init();

    // 1. Safe analog pin configurations using Pico SDK standard
    adc_gpio_init(42); // Channel 2
    adc_gpio_init(45); // Channel 5

    // 2. Configure the ADC FIFO
    adc_fifo_setup(
        true,  // Write each result to the FIFO
        true,  // Enable DMA data request (DREQ)
        1,     // DREQ asserted when at least 1 sample present
        false, // No ERR bit
        false  // No byte shift
    );

    // 3. Set standard clock divider (48MHz / 96 = 500ksps)
    adc_set_clkdiv(96);
}

void start_synchronized_adc_dma() {
    // Step 1: Explicitly shut down the ADC free-running clock
    adc_run(false);
    adc_set_round_robin(0);

    // Step 2: Purge the hardware FIFO completely until it's dead empty
    while (!adc_fifo_is_empty()) {
        adc_fifo_get();
    }

    // Step 3: Reset tracking variables to baseline states
    current_buffer = 1;
    completed_raw_buffer = NULL;
    master_buffer_ready = false;

    // Step 4: Configure the Master DMA channel configuration
    dma_channel_config c = dma_channel_get_default_config(dma_master_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, DREQ_ADC);

    // Configure and prime the DMA channel (Set to trigger but wait for DREQ)
    dma_channel_configure(
        dma_master_chan,
        &c,
        master_buffer1,         // Initial Destination
        &adc_hw->fifo,          // Source
        MASTER_BUFFER_SIZE,     // Transfer 2000 samples
        true                    // Enable the channel structure
    );

    // Set up our exclusive IRQ handler structure
    dma_channel_set_irq0_enabled(dma_master_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, master_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Step 5: Engage Round-Robin and line up the starting channel
    adc_set_round_robin(0x24); // Channels 2 and 5
    adc_select_input(2);       // Force layout lock to Channel 2 first

    // Step 6: Turn on the master ADC engine clock
    adc_run(true);
}