#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <stdint.h>
#include <stddef.h>
#include "pico/stdlib.h" 
#include "hardware/spi.h"

// SPI Configuration
#define TFT_SPI_PORT spi0 // using SPI block 1 on the Pico
#define TFT_BAUDRATE 1000000 // data transfer speed 

// GPIO pin definitions
#define SPI_TFT_SCK  22
#define SPI_TFT_TX   19
#define SPI_TFT_CSN  21
#define SPI_TFT_DC   14
#define SPI_TFT_RST  13 

// colors
#define COLOR_BLACK  0x0000
#define COLOR_WHITE  0xFFFF
#define COLOR_RED    0xF800
#define COLOR_GREEN  0x07E0
#define COLOR_BLUE   0x001F

// functions
void display_init_spi(void); // configures the Pico's SPI hardware and sets GPIO pin directions
void write_command(uint8_t command); // sends a single 8-bit instruction to the screen (pulls DC pin low)
void write_data(const uint8_t *data, size_t len); // sends raw data/parameters to the screen (pulls DC pin high)
void tft_reset(void); // toggles the reset pin to wake up the screen cleanly
void tft_init(void); // sends the long initialization sequence to the display controller
void tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1); // defines a rectangular bounding box on the screen to draw inside
void tft_fill_screen(uint16_t color); // fills the entire 320x240 screen with a single color
void tft_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size); // draws one scaled character using your 5x7 font array
void tft_print(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t bg, uint8_t size); // ;loops through a string to draw full words

#endif // TFT_DISPLAY_H