// tftspi.c
#include "tftspi.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

// Wait for SPI to be ready
static void wait_spi_ready(spi_inst_t* spi) {
    while (spi_get_hw(spi)->sr & SPI_SSPSR_BSY_BITS) {
        tight_loop_contents();
    }
}

// Initialize TFT SPI pins and settings
void init_tft_pins() {
    // Initialize GPIO pins
    int gpio_list[] = {SPI_DISP_SCK, SPI_DISP_TX, TFT_DC, TFT_RST};
    for (int i = 0; i < 4; i++) {
        gpio_init(gpio_list[i]);
        gpio_set_dir(gpio_list[i], GPIO_OUT);
        gpio_put(gpio_list[i], 0);
    }

    // Set SPI function for SCK and TX
    gpio_set_function(SPI_DISP_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_DISP_TX, GPIO_FUNC_SPI);

    // Initialize CS pin (active low)
    gpio_init(SPI_DISP_CSn);
    gpio_set_dir(SPI_DISP_CSn, GPIO_OUT);
    gpio_put(SPI_DISP_CSn, 1);

    // Set SPI clock speed (10 MHz for ST7789)
    spi_init(spi0, 10 * 1000 * 1000);
    spi_set_baudrate(spi0, 10 * 1000 * 1000);

    // Set SPI format (8-bit, mode 0, MSB first)
    spi_set_format(spi0, 8, 0, 0, SPI_MSB_FIRST);
}

// Send a command to the TFT display
void send_tft_cmd(spi_inst_t* spi, uint8_t cmd) {
    // Set DC pin to 0 (command mode)
    gpio_put(TFT_DC, 0);

    // Set CS pin to 0 (enable)
    gpio_put(SPI_DISP_CSn, 0);

    // Send command
    spi_write_blocking(spi, &cmd, 1);

    // Wait for SPI to be ready
    wait_spi_ready(spi);

    // Set CS pin to 1 (disable)
    gpio_put(SPI_DISP_CSn, 1);
}

// Send data to the TFT display
void send_tft_data(spi_inst_t* spi, uint8_t data) {
    // Set DC pin to 1 (data mode)
    gpio_put(TFT_DC, 1);

    // Set CS pin to 0 (enable)
    gpio_put(SPI_DISP_CSn, 0);

    // Send data
    spi_write_blocking(spi, &data, 1);

    // Wait for SPI to be ready
    wait_spi_ready(spi);

    // Set CS pin to 1 (disable)
    gpio_put(SPI_DISP_CSn, 1);
}

// Reset the TFT display
static void tft_reset() {
    gpio_put(TFT_RST, 0);
    sleep_ms(10);
    gpio_put(TFT_RST, 1);
    sleep_ms(100);
}

// Initialize the TFT display (ST7789)
void tft_init() {
    // Reset the display
    tft_reset();

    // Send initialization commands
    send_tft_cmd(spi0, 0x11); // Exit sleep mode
    sleep_ms(120);

    send_tft_cmd(spi0, 0x36); // Memory data access control
    send_tft_data(spi0, 0x00); // 0x00 for portrait mode

    send_tft_cmd(spi0, 0x3A); // Interface pixel format
    send_tft_data(spi0, 0x55); // 16-bit color (RGB565)

    send_tft_cmd(spi0, 0x21); // Display inversion on
    send_tft_cmd(spi0, 0x29); // Display on
}

// Clear the screen with a color
void tft_clear(uint16_t color) {
    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;

    send_tft_cmd(spi0, 0x2A); // Column address set
    send_tft_data(spi0, 0x00);
    send_tft_data(spi0, 0x00);
    send_tft_data(spi0, 0x00);
    send_tft_data(spi0, 239); // 240 pixels wide

    send_tft_cmd(spi0, 0x2B); // Row address set
    send_tft_data(spi0, 0x00);
    send_tft_data(spi0, 0x00);
    send_tft_data(spi0, 0x01);
    send_tft_data(spi0, 0x3F); // 320 pixels tall

    send_tft_cmd(spi0, 0x2C); // Memory write

    for (int i = 0; i < 240 * 320; i++) {
        send_tft_data(spi0, hi);
        send_tft_data(spi0, lo);
    }
}





// Display a string on the first line (simplified)
void tft_display1(const char *str, uint16_t color) {
    // Simple implementation: Draw each character as a rectangle
    // For a full implementation, use a font library like Adafruit GFX
    uint16_t x = 10;
    uint16_t y = 10;

    while (*str != '\0') {
        // Draw a rectangle for each character (simplified)
        tft_draw_rect(x, y, 8, 16, color, true);
        x += 10;
        str++;
    }
}

// Display a string on the second line (simplified)
void tft_display2(const char *str, uint16_t color) {
    // Simple implementation: Draw each character as a rectangle
    uint16_t x = 10;
    uint16_t y = 30;

    while (*str != '\0') {
        // Draw a rectangle for each character (simplified)
        tft_draw_rect(x, y, 8, 16, color, true);
        x += 10;
        str++;
    }
}

// Display a string on the second line (simplified)
void tft_display3(const char *str, uint16_t color) {
    // Simple implementation: Draw each character as a rectangle
    uint16_t x = 10;
    uint16_t y = 50;

    while (*str != '\0') {
        // Draw a rectangle for each character (simplified)
        tft_draw_rect(x, y, 8, 16, color, true);
        x += 10;
        str++;
    }
}
