#include "pico/stdlib.h"
#include <hardware/gpio.h>
#include <stdio.h>
#include "queue.h"

// Global column variable
int col = -1;
char key = '\0';

// Global key state
static bool state[16]; // Are keys pressed/released

// Keymap for the keypad
const char keymap[17] = "DCBA#9630852*741";

void keypad_drive_column();
void keypad_isr();
extern void key_push(uint16_t key_event);

/********************************************************* */
// Implement the functions below.

void keypad_init_pins()
{
    for (int i = 2; i < 6; i++)
    {
        uint32_t mask = 1u << (i & 0x1fu);
        sio_hw->gpio_oe_clr = mask;
        sio_hw->gpio_clr = mask;
        // Set new values for a sub-set of the bits in a HW register
        hw_write_masked(&pads_bank0_hw->io[i],
                        PADS_BANK0_GPIO0_IE_BITS,
                        PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
        io_bank0_hw->io[i].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
        // Atomically clear the specified bits to 0 in a HW register
        hw_clear_bits(&pads_bank0_hw->io[i], PADS_BANK0_GPIO0_ISO_BITS);
    }

    for (int i = 6; i < 10; i++)
    {
        uint32_t mask = 1u << (i & 0x1fu);
        sio_hw->gpio_oe_set = mask;
        sio_hw->gpio_clr = mask;
        // GPIO_SET_FUNCTION
        // Set new values for a sub-set of the bits in a HW register
        hw_write_masked(&pads_bank0_hw->io[i],
                        PADS_BANK0_GPIO0_IE_BITS,
                        PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
        io_bank0_hw->io[i].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
        // Atomically clear the specified bits to 0 in a HW register
        hw_clear_bits(&pads_bank0_hw->io[i], PADS_BANK0_GPIO0_ISO_BITS);
    }
}

void keypad_init_timer()
{
    // ==========================================
    // KEYPAD STEP 1: DRIVE COLUMN (Using Alarm 2)
    // ==========================================
    uint32_t delay_us_alarm2 = 1 * 1000000; // 1 second
    
    // Clear any leftover interrupt flags on Alarm 2
    timer_hw->intr = 1u << 2;
    
    // Enable the interrupt for hardware Alarm 2
    hw_set_bits(&timer_hw->inte, 1u << 2);
    
    // Set irq handler for Alarm 2 vector
    irq_set_exclusive_handler(TIMER0_IRQ_2, keypad_drive_column);
    irq_set_enabled(TIMER0_IRQ_2, true);
    
    // Set the target time execution
    uint64_t target_alarm2 = timer_hw->timerawl + delay_us_alarm2;
    timer_hw->alarm[2] = (uint32_t)target_alarm2;


    // ==========================================
    // KEYPAD STEP 2: READ ROW ISR (Using Alarm 3)
    // ==========================================
    uint32_t delay_us_alarm3 = 1.1 * 1000000; // 1.1 seconds
    
    // Clear any leftover interrupt flags on Alarm 3
    timer_hw->intr = 1u << 3;
    
    // Enable the interrupt for hardware Alarm 3
    hw_set_bits(&timer_hw->inte, 1u << 3);
    
    // Set irq handler for Alarm 3 vector
    irq_set_exclusive_handler(TIMER0_IRQ_3, keypad_isr);
    irq_set_enabled(TIMER0_IRQ_3, true);
    
    // Set the target time execution
    uint64_t target_alarm3 = timer_hw->timerawl + delay_us_alarm3;
    timer_hw->alarm[3] = (uint32_t)target_alarm3;
}

void keypad_drive_column()
{
    // acknowledge the interrupt for ALARM2 on TIMER0
    // uint32_t result =
    // timer0_hw->intr =
    hw_clear_bits(&timer_hw->intr, 1u << 2);
    // set the columns from 6-9
    col++;
    col = (col) % 4;

    int new_col = 6 + col;
    // int new_col_behind = (new_col == 6) ? 9 : new_col - 1;
    uint32_t mask = (1 << new_col);

    // clear completly
    sio_hw->gpio_clr = 15 << 6;
    // // // set the bit of the mask that I think needs to be set
    // sio_hw->gpio_out = mask;
    uint16_t out = sio_hw->gpio_out;
    uint16_t check_on = ((15 << 6) & out);
    sio_hw->gpio_togl = check_on ^ mask;

    // increment up to 3 then go back to 0

    uint32_t delay_us_alarm0 = 25 * 1000;
    uint64_t target_alarm0 = timer_hw->timerawl + delay_us_alarm0;
    timer_hw->alarm[2] = (uint32_t)target_alarm0;
}

uint8_t keypad_read_rows()
{
    uint8_t rows = ((sio_hw->gpio_in & (1 << 5)) | (sio_hw->gpio_in & (1 << 4)) | (sio_hw->gpio_in & (1 << 3)) | sio_hw->gpio_in & (1 << 2)) >> 2;
    return rows;
}

uint16_t ascii_to_bin(int set_bit, char key)
{
    uint16_t bin = set_bit << 8;
    uint16_t bin_val = bin | key;
    key_push(bin_val);
}

void keypad_isr()
{
    // acknowledge the interrupt for ALARM3 on TIMER0
    hw_clear_bits(&timer_hw->intr, 1u << 3);

    // Get the current row pin values by calling keypad_read_rows
    uint8_t row = keypad_read_rows();

    // For each of the row pins that are high, indicating that a button is pressed,
    // check if the corresponding bit in state is low

    // If it is low, then we have a new key press.

    for (int i = 2; i <= 5; i++)
    {
        // uint32_t result = gpio_get_irq_event_mask(i);
        int row_pin = i - 2;
        // const char keymap[17] = "DCBA#9630852*741";
        int index = (col * 4) + row_pin;
        key = keymap[index];

        if ((row & (1 << row_pin)) && (state[index] == 0))
        {
            // Uncomment for step 2

            state[index] = 1;
            // Pushes the event into kev
            ascii_to_bin(1, key);
        }
        else if (!(row & (1 << row_pin)) && (state[index] == 1))
        {
            state[index] = 0;

            // Pushes the event into kev
            ascii_to_bin(0, key);
        }
    }
    uint32_t delay_us_alarm0 = 25 * 1000;
    uint64_t target_alarm0 = timer_hw->timerawl + delay_us_alarm0;
    timer_hw->alarm[3] = (uint32_t)target_alarm0;
}