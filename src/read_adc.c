#include "read_adc.h"


#define NUM_SAMPLES 1000
#define MASTER_BUFFER_SIZE (NUM_SAMPLES * 2) // 2000 elements total
extern uint16_t master_buffer1[MASTER_BUFFER_SIZE];
extern uint16_t master_buffer2[MASTER_BUFFER_SIZE];
extern int dma_master_chan;
extern int current_buffer;
extern volatile bool master_buffer_ready;
extern volatile uint16_t* completed_raw_buffer;


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