// #include "pico/stdlib.h"
// #include <hardware/gpio.h>
// #include <stdio.h>
// #include "queue.h"

// // Global column variable
// int col = -1;
// char key = '\0';

// // Global key state
// static bool state[16]; // Are keys pressed/released

// // Keymap for the keypad
// const char keymap[17] = "DCBA#9630852*741";

// void keypad_drive_column();
// void keypad_isr();
// extern void key_push(uint16_t key_event);

// /********************************************************* */
// // Implement the functions below.

// void keypad_init_pins()
// {
//     for (int i = 2; i < 6; i++)
//     {
//         uint32_t mask = 1u << (i & 0x1fu);
//         sio_hw->gpio_oe_clr = mask;
//         sio_hw->gpio_clr = mask;
//         // Set new values for a sub-set of the bits in a HW register
//         hw_write_masked(&pads_bank0_hw->io[i],
//                         PADS_BANK0_GPIO0_IE_BITS,
//                         PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
//         io_bank0_hw->io[i].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
//         // Atomically clear the specified bits to 0 in a HW register
//         hw_clear_bits(&pads_bank0_hw->io[i], PADS_BANK0_GPIO0_ISO_BITS);
//     }

//     for (int i = 6; i < 10; i++)
//     {
//         uint32_t mask = 1u << (i & 0x1fu);
//         sio_hw->gpio_oe_set = mask;
//         sio_hw->gpio_clr = mask;
//         // GPIO_SET_FUNCTION
//         // Set new values for a sub-set of the bits in a HW register
//         hw_write_masked(&pads_bank0_hw->io[i],
//                         PADS_BANK0_GPIO0_IE_BITS,
//                         PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
//         io_bank0_hw->io[i].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
//         // Atomically clear the specified bits to 0 in a HW register
//         hw_clear_bits(&pads_bank0_hw->io[i], PADS_BANK0_GPIO0_ISO_BITS);
//     }
// }

// // void keypad_init_timer()
// // {
// //     // ==========================================
// //     // KEYPAD STEP 1: DRIVE COLUMN (Using Alarm 2)
// //     // ==========================================
// //     uint32_t delay_us_alarm2 = 1 * 1000000; // 1 second

// //     // Clear any leftover interrupt flags on Alarm 2
// //     timer_hw->intr = 1u << 2;

// //     // Enable the interrupt for hardware Alarm 2
// //     hw_set_bits(&timer_hw->inte, 1u << 2);

// //     // Set irq handler for Alarm 2 vector
// //     irq_set_exclusive_handler(TIMER0_IRQ_2, keypad_drive_column);
// //     irq_set_enabled(TIMER0_IRQ_2, true);

// //     // Set the target time execution
// //     uint64_t target_alarm2 = timer_hw->timerawl + delay_us_alarm2;
// //     timer_hw->alarm[2] = (uint32_t)target_alarm2;

// //     // ==========================================
// //     // KEYPAD STEP 2: READ ROW ISR (Using Alarm 3)
// //     // ==========================================
// //     uint32_t delay_us_alarm3 = 1.1 * 1000000; // 1.1 seconds

// //     // Clear any leftover interrupt flags on Alarm 3
// //     timer_hw->intr = 1u << 3;

// //     // Enable the interrupt for hardware Alarm 3
// //     hw_set_bits(&timer_hw->inte, 1u << 3);

// //     // Set irq handler for Alarm 3 vector
// //     irq_set_exclusive_handler(TIMER0_IRQ_3, keypad_isr);
// //     irq_set_enabled(TIMER0_IRQ_3, true);

// //     // Set the target time execution
// //     uint64_t target_alarm3 = timer_hw->timerawl + delay_us_alarm3;
// //     timer_hw->alarm[3] = (uint32_t)target_alarm3;
// // }

// // Track our dynamically allocated alarm IDs globally
// static int keypad_alarm_drive_id = -1;
// static int keypad_alarm_read_id  = -1;

// void keypad_init_timer()
// {
//     // 1. Claim two unused hardware alarms dynamically from the SDK pool
//     keypad_alarm_drive_id = hardware_alarm_claim_unused(true); // Throws panic if none are left
//     keypad_alarm_read_id  = hardware_alarm_claim_unused(true);

//     // Calculate their respective IRQ numbers based on the claimed slots
//     uint irq_drive = TIMER0_IRQ_0 + keypad_alarm_drive_id;
//     uint irq_read  = TIMER0_IRQ_0 + keypad_alarm_read_id;

//     // ==========================================
//     // KEYPAD STEP 1: DRIVE COLUMN
//     // ==========================================
//     uint32_t delay_us_alarm_drive = 1 * 1000000; // 1 second

//     // Clear any leftover interrupt flags on our assigned slot
//     timer_hw->intr = 1u << keypad_alarm_drive_id;

//     // Enable the interrupt configuration for our hardware Alarm slot
//     hw_set_bits(&timer_hw->inte, 1u << keypad_alarm_drive_id);

//     // Bind our custom handler exclusively to this safe vector slot
//     irq_set_exclusive_handler(irq_drive, keypad_drive_column);
//     irq_set_enabled(irq_drive, true);

//     // Commit target execution timestamp
//     uint64_t target_drive = timer_hw->timerawl + delay_us_alarm_drive;
//     timer_hw->alarm[keypad_alarm_drive_id] = (uint32_t)target_drive;

//     // ==========================================
//     // KEYPAD STEP 2: READ ROW ISR
//     // ==========================================
//     uint32_t delay_us_alarm_read = 1.1 * 1000000; // 1.1 seconds

//     // Clear any leftover interrupt flags on our assigned slot
//     timer_hw->intr = 1u << keypad_alarm_read_id;

//     // Enable the interrupt configuration for our hardware Alarm slot
//     hw_set_bits(&timer_hw->inte, 1u << keypad_alarm_read_id);

//     // Bind our custom handler exclusively to this safe vector slot
//     irq_set_exclusive_handler(irq_read, keypad_isr);
//     irq_set_enabled(irq_read, true);

//     // Commit target execution timestamp
//     uint64_t target_read = timer_hw->timerawl + delay_us_alarm_read;
//     timer_hw->alarm[keypad_alarm_read_id] = (uint32_t)target_read;
// }

// void keypad_drive_column()
// {
//     // Dynamically clear the correct interrupt bit flag
//     hw_clear_bits(&timer_hw->intr, 1u << keypad_alarm_drive_id);

//     // Cycle the column lines
//     col++;
//     col = (col) % 4;

//     int new_col = 6 + col;
//     sio_hw->gpio_clr = 15 << 6;

//     uint16_t out = sio_hw->gpio_out;
//     uint16_t check_on = ((15 << 6) & out);
//     sio_hw->gpio_togl = check_on ^ (1 << new_col);

//     // Reschedule utilizing the dynamic hardware block index
//     uint32_t delay_us_alarm2 = 25 * 1000;
//     uint64_t target_alarm2 = timer_hw->timerawl + delay_us_alarm2;
//     timer_hw->alarm[keypad_alarm_drive_id] = (uint32_t)target_alarm2;
// }

// uint8_t keypad_read_rows()
// {
//     uint8_t rows = ((sio_hw->gpio_in & (1 << 5)) | (sio_hw->gpio_in & (1 << 4)) | (sio_hw->gpio_in & (1 << 3)) | sio_hw->gpio_in & (1 << 2)) >> 2;
//     return rows;
// }

// void ascii_to_bin(int set_bit, char key)
// {
//     uint16_t bin = set_bit << 8;
//     uint16_t bin_val = bin | key;
//     key_push(bin_val);
// }

// void keypad_isr()
// {
//     // Dynamically clear the correct interrupt bit flag
//     hw_clear_bits(&timer_hw->intr, 1u << keypad_alarm_read_id);

//     uint8_t row = keypad_read_rows();

//     for (int i = 2; i <= 5; i++)
//     {
//         int row_pin = i - 2;
//         int index = (col * 4) + row_pin;
//         key = keymap[index];

//         if ((row & (1 << row_pin)) && (state[index] == 0))
//         {
//             state[index] = 1;
//             ascii_to_bin(1, key);
//         }
//         else if (!(row & (1 << row_pin)) && (state[index] == 1))
//         {
//             state[index] = 0;
//             ascii_to_bin(0, key);
//         }
//     }

//     // Reschedule utilizing the dynamic hardware block index
//     uint32_t delay_us_alarm3 = 25 * 1000;
//     uint64_t target_alarm3 = timer_hw->timerawl + delay_us_alarm3;
//     timer_hw->alarm[keypad_alarm_read_id] = (uint32_t)target_alarm3;
// }

// void keypad_drive_column()
// {
//     // acknowledge the interrupt for ALARM0 on TIMER0
//     // uint32_t result =
//     // timer0_hw->intr =
//     hw_clear_bits(&timer_hw->intr, 1u << 2);
//     // set the columns from 6-9
//     col++;
//     col = (col) % 4;

//     int new_col = 6 + col;
//     // int new_col_behind = (new_col == 6) ? 9 : new_col - 1;
//     uint32_t mask = (1 << new_col);

//     // clear completly
//     sio_hw->gpio_clr = 15 << 6;
//     // // // set the bit of the mask that I think needs to be set
//     // sio_hw->gpio_out = mask;
//     uint16_t out = sio_hw->gpio_out;
//     uint16_t check_on = ((15 << 6) & out);
//     sio_hw->gpio_togl = check_on ^ mask;

//     // increment up to 3 then go back to 0

//     uint32_t delay_us_alarm2 = 25 * 1000;
//     uint64_t target_alarm2 = timer_hw->timerawl + delay_us_alarm2;
//     timer_hw->alarm[2] = (uint32_t)target_alarm2;
// }

// void keypad_isr()
// {
//     // acknowledge the interrupt for ALARM1 on TIMER0
//     hw_clear_bits(&timer_hw->intr, 1u << 3);

//     // Get the current row pin values by calling keypad_read_rows
//     uint8_t row = keypad_read_rows();

//     // For each of the row pins that are high, indicating that a button is pressed,
//     // check if the corresponding bit in state is low

//     // If it is low, then we have a new key press.

//     for (int i = 2; i <= 5; i++)
//     {
//         // uint32_t result = gpio_get_irq_event_mask(i);
//         int row_pin = i - 2;
//         // const char keymap[17] = "DCBA#9630852*741";
//         int index = (col * 4) + row_pin;
//         key = keymap[index];

//         if ((row & (1 << row_pin)) && (state[index] == 0))
//         {
//             // Uncomment for step 2

//             state[index] = 1;
//             // Pushes the event into kev
//             ascii_to_bin(1, key);
//         }
//         else if (!(row & (1 << row_pin)) && (state[index] == 1))
//         {
//             state[index] = 0;

//             // Pushes the event into kev
//             ascii_to_bin(0, key);
//         }
//     }
//     uint32_t delay_us_alarm3 = 25 * 1000;
//     uint64_t target_alarm3 = timer_hw->timerawl + delay_us_alarm3;
//     timer_hw->alarm[3] = (uint32_t)target_alarm3;
// }

#include "pico/stdlib.h"
#include <hardware/gpio.h>
#include <stdio.h>
#include "queue.h"
#include "hardware/timer.h"

// Global column variable
int col = -1;
char key = '\0';

// Global key state
static bool state[16]; // Are keys pressed/released

// Keymap for the keypad
const char keymap[17] = "DCBA#9630852*741";

void keypad_drive_column();
void keypad_isr();
int drive_col_unused_alarm_id = -1;
int read_row_unused_alarm_id = -1;

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
    uint32_t delay_us_alarm0 = 1 * 1000000;
    drive_col_unused_alarm_id = hardware_alarm_claim_unused(true);
    // Enable the interrupt for our alarm (the timer outputs 4 alarm irqs)
    hw_set_bits(&timer_hw->inte, 1u << drive_col_unused_alarm_id);
    // Set irq handler for alarm irq
    irq_set_exclusive_handler(TIMER0_IRQ_0 + drive_col_unused_alarm_id, keypad_drive_column);
    // Enable the alarm irq
    irq_set_enabled(TIMER0_IRQ_0 + drive_col_unused_alarm_id, true);
    uint64_t target_alarm0 = timer_hw->timerawl + delay_us_alarm0;
    // Write the lower 32 bits of the target time to the alarm which
    timer_hw->alarm[drive_col_unused_alarm_id] = (uint32_t)target_alarm0;

    uint32_t delay_us_alarm1 = 1.1 * 1000000;
    read_row_unused_alarm_id = hardware_alarm_claim_unused(true);
    // Enable the interrupt for our alarm (the timer outputs 4 alarm irqs)
    hw_set_bits(&timer_hw->inte, 1u << read_row_unused_alarm_id);
    // Set irq handler for alarm irq
    irq_set_exclusive_handler(TIMER0_IRQ_0 + read_row_unused_alarm_id, keypad_isr);
    // Enable the alarm irq
    irq_set_enabled(TIMER0_IRQ_0 + read_row_unused_alarm_id, true);
    uint64_t target_alarm1 = timer_hw->timerawl + delay_us_alarm1;
    // Write the lower 32 bits of the target time to the alarm which
    timer_hw->alarm[read_row_unused_alarm_id] = (uint32_t)target_alarm1;
}

void keypad_drive_column()
{
    // acknowledge the interrupt for ALARM0 on TIMER0
    // uint32_t result =
    // timer0_hw->intr =
    hw_clear_bits(&timer_hw->intr, 1u << drive_col_unused_alarm_id);
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
    timer_hw->alarm[drive_col_unused_alarm_id] = (uint32_t)target_alarm0;
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
    // acknowledge the interrupt for ALARM1 on TIMER0
    hw_clear_bits(&timer_hw->intr, 1u << read_row_unused_alarm_id);

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
    timer_hw->alarm[read_row_unused_alarm_id] = (uint32_t)target_alarm0;
}