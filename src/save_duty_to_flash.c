// #include <stdio.h>
// #include <string.h>
// #include "pico/stdlib.h"
// #include "hardware/flash.h"
// #include "hardware/sync.h"

// // Assuming standard 4MB Flash setup: target the absolute last 4KB sector
// #define FLASH_TARGET_OFFSET (4 * 1024 * 1024 - FLASH_SECTOR_SIZE)

// // 1. Structure matching exactly all properties to maintain state recovery
// typedef struct {
//     uint32_t magic_number;       // Signature validation tag (0xDEADBEEF)
//     uint32_t set_duty;           // Fan duty cycle
//     int32_t  remaining_days;
//     int32_t  remaining_hours;
//     int32_t  remaining_minutes;
//     int32_t  remaining_seconds;
//     uint8_t  padding[232];       // Pads struct to exactly 256 bytes (1 Page)
// } nv_recovery_t;

// // 2. High-Level flash-writing procedure utilizing safe SDK abstraction
// void save_state_to_flash(uint32_t duty, int days, int hours, int mins, int secs) {
//     nv_recovery_t state;
//     state.magic_number      = 0xDEADBEEF;
//     state.set_duty          = duty;
//     state.remaining_days    = days;
//     state.remaining_hours   = hours;
//     state.remaining_minutes = mins;
//     state.remaining_seconds = secs;

//     // Local RAM buffer alignment configuration
//     uint8_t page_buffer[FLASH_PAGE_SIZE];
//     memcpy(page_buffer, &state, sizeof(nv_recovery_t));

//     // CRITICAL STAGE: Disable interrupts to prevent XIP asset collisions
//     uint32_t ints = save_and_disable_interrupts();

//     // Erase the target 4KB sector (mandatory before programming new properties)
//     flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

//     // Commit the 256-byte data page directly into flash
//     flash_range_program(FLASH_TARGET_OFFSET, page_buffer, FLASH_PAGE_SIZE);

//     // Safely restore global interrupt operations
//     restore_interrupts(ints);
// }

// // 3. High-Level flash reading procedure (Uses XIP Memory Mapping)
// bool load_state_from_flash(nv_recovery_t *state_out) {
//     // XIP maps external flash directly to memory space starting at XIP_BASE (0x10000000)
//     const nv_recovery_t *flash_data = (const nv_recovery_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    
//     // Direct memory copy via pointer mapping 
//     memcpy(state_out, flash_data, sizeof(nv_recovery_t));
    
//     // Return true if data is validated, false if it's unprogrammed/blank flash
//     return (state_out->magic_number == 0xDEADBEEF);
// }