#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

#define NUM_SAMPLES 1000
extern uint16_t adc_duty_cycle_result[NUM_SAMPLES];
extern uint16_t adc_duty_cycle_result2[NUM_SAMPLES];
extern int current_buffer;
extern bool buffer1_read_full;
extern bool buffer2_read_full;
extern int dma_read_fan_speed_chan;

void combined_dma_handler();

void init_adc_read_pwm_freerun()
{
    // fill in
    int GPIO_NUM = 42;
    hw_write_masked(&pads_bank0_hw->io[GPIO_NUM],
                    PADS_BANK0_GPIO0_IE_BITS,
                    PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
    io_bank0_hw->io[GPIO_NUM].ctrl = GPIO_FUNC_NULL << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
    pads_bank0_hw->io[GPIO_NUM] = 1u << 7;
    // io_bank0_hw->io[45].ctrl =
    // Atomically clear the specified bits to 0 in a HW register
    hw_clear_bits(&pads_bank0_hw->io[GPIO_NUM], PADS_BANK0_GPIO0_ISO_BITS);
    // adc_select_input(5);
    // adc_get_selected_input();
    // AINSEL: Select analog mux input. Updated automatically in round-robin mode. (Selected GPIO 26)

    adc_select_input(2);
    adc_fifo_setup(
        true,  // Write each result to the FIFO
        true,  // Enable DMA data request (DREQ)
        1,     // DREQ (and IRQ) asserted when at least 1 sample present
        false, // Don't ERR bit in FIFO
        false  // No byte shift, 8-bit results are right-aligned in FIFO
    );
    adc_hw->cs = 1 << 14 | 0 << 13 | 1 << 12;

    // START_MANY: Continuously perform conversions whilst this bit is 1. A new conversion will start immediately after the previous finishes.
    adc_hw->cs |= 1 << 3;

    // EN: Power on ADC and enable its clock. 1 - enabled. 0 - disabled
    adc_hw->cs |= 1 << 0;
}



void init_dma_for_read_fanspeed()
{
    dma_channel_config c = dma_channel_get_default_config(dma_read_fan_speed_chan);

    // set data size to half-word (16 bits) since our ADC samples are 12 bits and right-aligned in the FIFO
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);

    // increment adc result address (write) true and fifo (read) false
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);

    // Set the TREQ for this channel to DREQ_ADC, so that the DMA will wait for the ADC to have a sample ready before performing each transfer
    channel_config_set_dreq(&c, DREQ_ADC);

    dma_channel_configure(
        dma_read_fan_speed_chan, // Channel to be configured
        &c,                      // The configuration we just created
        adc_duty_cycle_result,   // Destination pointer (ADC result variable)
        &adc_hw->fifo,           // Source pointer (ADC FIFO)
        NUM_SAMPLES,             // Number of transfers
        true                     // Don't start yet
    );
    dma_channel_set_irq0_enabled(dma_read_fan_speed_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, combined_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}