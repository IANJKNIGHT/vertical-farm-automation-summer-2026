// tftspi.h
#ifndef TFT_SPI_H
#define TFT_SPI_H

#include "pico/stdlib.h"
#include "hardware/spi.h"

// Make sure to set these in main.c
extern const int SPI_DISP_SCK;   // SPI Clock pin
extern const int SPI_DISP_CSn;   // Chip Select (CS) pin
extern const int SPI_DISP_TX;    // MOSI (TX) pin
extern const int TFT_DC;         // Data/Command (DC) pin
extern const int TFT_RST;        // Reset (RST) pin

// Initialize TFT SPI pins and settings
void init_tft_pins();

// Send a command to the TFT display
void send_tft_cmd(spi_inst_t* spi, uint8_t cmd);

// Send data to the TFT display
void send_tft_data(spi_inst_t* spi, uint8_t data);

// Initialize the TFT display
void tft_init();

// Clear the screen with a color
void tft_clear(uint16_t color);

// Draw a pixel at (x, y) with a color
void tft_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

// Draw a rectangle (filled or outline)
void tft_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color, bool fill);

// Display a string on the first line
void tft_display1(const char *str, uint16_t color);

// Display a string on the second line
void tft_display2(const char *str, uint16_t color);

#endif
