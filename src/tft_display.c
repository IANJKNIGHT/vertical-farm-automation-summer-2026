#include "tft_display.h"

// standard 5x7 ASCII Font
// each character is represented by 5 bytes and each byte corresponds to a vertical column 
// of pixels (with 7 bits typically used for the height)
// https://github.com/greiman/SSD1306Ascii/blob/master/src/fonts/System5x7.h
const uint8_t font_5x7[96][5] = {
    {0 }, {0x00,0x00,0x5f,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7f,0x14,0x7f,0x14}, // Space ! " #
    {0x24,0x2a,0x7f,0x2a,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00}, // $ % & '
    {0x00,0x1c,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1c,0x00}, {0x14,0x08,0x3e,0x08,0x14}, {0x08,0x08,0x3e,0x08,0x08}, // ( ) * +
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02}, // , - . /
    {0x3e,0x51,0x49,0x45,0x3e}, {0x00,0x42,0x7f,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31}, // 0 1 2 3
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03}, // 4 5 6 7
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00}, // 8 9 : ;
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06}, // < = > ?
    {0x32,0x49,0x79,0x41,0x3e}, {0x7e,0x11,0x11,0x11,0x7e}, {0x7f,0x49,0x49,0x49,0x36}, {0x3e,0x41,0x41,0x41,0x22}, // @ A B C
    {0x7f,0x41,0x41,0x22,0x1c}, {0x7f,0x49,0x49,0x49,0x41}, {0x7f,0x09,0x09,0x09,0x01}, {0x3e,0x41,0x49,0x49,0x7a}, // D E F G
    {0x7f,0x08,0x08,0x08,0x7f}, {0x00,0x41,0x7f,0x41,0x00}, {0x20,0x40,0x41,0x3f,0x01}, {0x7f,0x08,0x14,0x22,0x41}, // H I J K
    {0x7f,0x40,0x40,0x40,0x40}, {0x7f,0x02,0x0c,0x02,0x7f}, {0x7f,0x04,0x08,0x10,0x7f}, {0x3e,0x41,0x41,0x41,0x3e}, // L M N O
    {0x7f,0x09,0x09,0x09,0x06}, {0x3e,0x41,0x51,0x21,0x5e}, {0x7f,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31}, // P Q R S
    {0x01,0x01,0x7f,0x01,0x01}, {0x3f,0x40,0x40,0x40,0x3f}, {0x1f,0x20,0x40,0x20,0x1f}, {0x3f,0x40,0x38,0x40,0x3f}, // T U V W
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7f,0x41,0x41,0x00}, // X Y Z [
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7f,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40}, // \ ] ^ _
    {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7f,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20}, // ` a b c
    {0x38,0x44,0x44,0x48,0x7f}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7e,0x09,0x01,0x02}, {0x0c,0x52,0x52,0x52,0x3e}, // d e f g
    {0x7f,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7d,0x40,0x00}, {0x20,0x40,0x44,0x3d,0x00}, {0x7f,0x10,0x28,0x44,0x00}, // h i j k
    {0x00,0x41,0x7f,0x40,0x00}, {0x7c,0x04,0x18,0x04,0x78}, {0x7c,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38}, // l m n o
    {0x7c,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7c}, {0x7c,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20}, // p q r s
    {0x04,0x3f,0x44,0x40,0x20}, {0x3c,0x40,0x40,0x20,0x7c}, {0x1c,0x20,0x40,0x20,0x1c}, {0x3c,0x40,0x30,0x40,0x3c}, // t u v w
    {0x44,0x28,0x10,0x28,0x44}, {0x0c,0x50,0x50,0x50,0x3c}, {0x44,0x64,0x54,0x4c,0x44}, {0x00,0x08,0x36,0x41,0x00}  // x y z {
};

// initializes the hardware SPI peripheral and sets up required GPIO pins
void display_init_spi(void) {
    // initialize SPI bus at specified baudrate, changeable in .h file
    spi_init(TFT_SPI_PORT, TFT_BAUDRATE);
    
    // SPI Mode 0 configuration
    spi_set_format(TFT_SPI_PORT, 8, 0, 0, SPI_MSB_FIRST);

    // map hardware SPI functions to the SCK and TX pins
    gpio_set_function(SPI_TFT_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TFT_TX,  GPIO_FUNC_SPI);

    // initialize CSN, active low
    gpio_init(SPI_TFT_CSN);
    gpio_set_dir(SPI_TFT_CSN, GPIO_OUT);
    gpio_put(SPI_TFT_CSN, 1); // set high by default

    // initialize DC pin, low for command, high for data
    gpio_init(SPI_TFT_DC);
    gpio_set_dir(SPI_TFT_DC, GPIO_OUT);
    gpio_put(SPI_TFT_DC, 1); 

    // initialize RST pin, active low
    gpio_init(SPI_TFT_RST);
    gpio_set_dir(SPI_TFT_RST, GPIO_OUT);
    gpio_put(SPI_TFT_RST, 1); 
}

// hard reset of the display using the RST pin
void tft_reset(void) {
    gpio_put(SPI_TFT_RST, 1);
    sleep_ms(50);
    gpio_put(SPI_TFT_RST, 0); // pull reset low to trigger hardware reset
    sleep_ms(50);             
    gpio_put(SPI_TFT_RST, 1); // return to high state for normal operation
    sleep_ms(150); // wait for display internal initialization
}

// sends a single command byte to the display controller
void write_command(uint8_t command) {
    gpio_put(SPI_TFT_DC, 0); // DC = 0 tells display incoming byte is a command
    sleep_us(2); 
    gpio_put(SPI_TFT_CSN, 0); // select the display on the SPI bus
    sleep_us(2); 
    
    // send the command byte over SPI
    spi_write_blocking(TFT_SPI_PORT, &command, 1);
    
    sleep_us(2);
    gpio_put(SPI_TFT_CSN, 1); // deselect the display
}

// sends multiple data bytes to the display controller
void write_data(const uint8_t *data, size_t len) {
    gpio_put(SPI_TFT_DC, 1); // DC = 1 tells display incoming bytes are data
    sleep_us(2); 
    gpio_put(SPI_TFT_CSN, 0); // select the display
    sleep_us(2); 
    
    // transmit the array of data bytes
    spi_write_blocking(TFT_SPI_PORT, data, len);
    
    sleep_us(2);
    gpio_put(SPI_TFT_CSN, 1); // deselect the display
}

// sends the specific initialization sequence required for the ILI9341(our LDC display)
// https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf
// https://www.lcdwiki.com/2.2inch_SPI_Module_ILI9341_SKU:MSP2202
void tft_init(void) {
    //works/fixed with AI as wasnt working and it said could be because the below initializations were not enough
    tft_reset(); // hardware reset
    write_command(0x01); // software reset command
    sleep_ms(150); // delay after software reset

    write_command(0xCB); // power control A - sets core timings and voltages
    uint8_t data_CB[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    write_data(data_CB, 5);

    write_command(0xCF); // power bontrol B - adjusts discharge controls
    uint8_t data_CF[] = {0x00, 0xC1, 0x30};
    write_data(data_CF, 3);

    write_command(0xE8); // driver timing control A
    uint8_t data_E8[] = {0x85, 0x00, 0x78};
    write_data(data_E8, 3);

    write_command(0xEA); // driver timing control B
    uint8_t data_EA[] = {0x00, 0x00};
    write_data(data_EA, 2);

    write_command(0xED); // power on sequence control
    uint8_t data_ED[] = {0x64, 0x03, 0x12, 0x81};
    write_data(data_ED, 4);

    write_command(0xF7); // pump ratio control
    uint8_t data_F7[] = {0x20};
    write_data(data_F7, 1);

    write_command(0xC0); // power control 1 - GVDD level
    uint8_t data_C0[] = {0x23}; 
    write_data(data_C0, 1);

    write_command(0xC1); // power control 2 - Step-up factor
    uint8_t data_C1[] = {0x10}; 
    write_data(data_C1, 1);

    write_command(0xC5); // VCOM control 1 - Display contrast/voltage
    uint8_t data_C5[] = {0x3e, 0x28};
    write_data(data_C5, 2);

    write_command(0xC7); // VCOM control 2
    uint8_t data_C7[] = {0x86};
    write_data(data_C7, 1);

    write_command(0x36); // memory access control (orientation)
    uint8_t data_36[] = {0x28}; // 0x28 sets landscape mode and fixes text mirroring (Row/Col swap, etc.)
    write_data(data_36, 1);

    write_command(0x3A); // pixel format (16-bit) - sets the color depth to RGB565 (2 bytes per pixel)
    uint8_t data_3A[] = {0x55};
    write_data(data_3A, 1);

    write_command(0xB1); // frame rate control
    uint8_t data_B1[] = {0x00, 0x18};
    write_data(data_B1, 2);

    write_command(0xB6); // display function control - set scan direction and row count
    uint8_t data_B6[] = {0x08, 0x82, 0x27};
    write_data(data_B6, 3);

    write_command(0x11); // exit sleep mode - wakes up the display
    sleep_ms(120); //delay allowing charge pumps to stabilize

    write_command(0x29); // display on - turns on the final output
    sleep_ms(120);

    //didnt work originally might work now?
    // tft_reset(); // hardware reset

    // write_command(0x01); // software Reset
    // sleep_ms(150);

    // write_command(0x11); // exit sleep mode
    // sleep_ms(120); // mandatory delay for charge pumps

    // // memory access control (orientation)
    // write_command(0x36); 
    // uint8_t mac = 0x28; 
    // write_data(&mac, 1);

    // // pixel format (16-bit RGB565)
    // write_command(0x3A); 
    // uint8_t pix = 0x55; 
    // write_data(&pix, 1);

    // write_command(0x29); // display on
    // sleep_ms(120);

}

// Defines an active rectangular region where the next pixel data will be written
void tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];
    
    write_command(0x2A); // column address set (x coordinate limits)
    data[0] = x0 >> 8; data[1] = x0 & 0xFF; // start x
    data[2] = x1 >> 8; data[3] = x1 & 0xFF; // end x 
    write_data(data, 4);

    write_command(0x2B); // page address set (y coordinate limits)
    data[0] = y0 >> 8; data[1] = y0 & 0xFF; // start y
    data[2] = y1 >> 8; data[3] = y1 & 0xFF; // end y
    write_data(data, 4);

    write_command(0x2C); // readies display to receive pixel color data
}

// fills the entire screen with a single 16-bit RGB565 color
void tft_fill_screen(uint16_t color) {
    // set drawing window to encompass the full 320x240 display
    tft_set_window(0, 0, 319, 239);
    
    // Split 16-bit color into two 8-bit bytes for SPI transfer
    uint8_t color_high = color >> 8;
    uint8_t color_low = color & 0xFF;
    uint8_t color_data[2] = {color_high, color_low};

    gpio_put(SPI_TFT_DC, 1); // set to data mode
    gpio_put(SPI_TFT_CSN, 0); // select display
    
    // 320 * 240  = 76,800 
    for(uint32_t i = 0; i < 76800; i++) {
        spi_write_blocking(TFT_SPI_PORT, color_data, 2); // Send 2 bytes per pixel
    }
    gpio_put(SPI_TFT_CSN, 1); // deselect the display
}

// renders a single scaled character on the screen using the 5x7 font array at the top
void tft_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size) {
    // bounds check to ensure character exists in our ASCII array
    if (c < 32 || c > 126) return; 
    int idx = c - 32; // offset by 32 because Space is the first char in our array at index 0
    
    // iterate over the 5 columns of the character bitmap
    for (int8_t i = 0; i < 5; i++) { 
        uint8_t line = font_5x7[idx][i]; // fetch column data
        // iterate over the 8 rows (bits) of the current column
        for (int8_t j = 0; j < 8; j++, line >>= 1) { 
            // check if the current bit is 1 
            if (line & 1) {
                // draw a solid block of (size x size) instead of a single pixel for scaling
                tft_set_window(x + i*size, y + j*size, x + i*size + size - 1, y + j*size + size - 1);
                uint8_t col[2] = {color >> 8, color & 0xFF};
                
                gpio_put(SPI_TFT_DC, 1);
                gpio_put(SPI_TFT_CSN, 0);
                // fill the scaled block with foreground color
                for(int p = 0; p < size * size; p++) {
                    spi_write_blocking(TFT_SPI_PORT, col, 2);
                }
                gpio_put(SPI_TFT_CSN, 1);
            } 
            // draw background block if bg color is different from foreground
            // This prevents character overlapping on a colored background
            else if (bg != color) {
                tft_set_window(x + i*size, y + j*size, x + i*size + size - 1, y + j*size + size - 1);
                uint8_t col[2] = {bg >> 8, bg & 0xFF};
                
                gpio_put(SPI_TFT_DC, 1);
                gpio_put(SPI_TFT_CSN, 0);
                // fill the scaled block with background color
                for(int p = 0; p < size * size; p++) {
                    spi_write_blocking(TFT_SPI_PORT, col, 2);
                }
                gpio_put(SPI_TFT_CSN, 1);
            }
        }
    }
}

// renders a complete string starting from the specified coordinates
void tft_print(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t bg, uint8_t size) {
    uint16_t cursor_x = x;
    // iterate through characters until the null terminator ('\0') is reached
    while (*text) {
        tft_draw_char(cursor_x, y, *text, color, bg, size); // draw current char
        cursor_x += 6 * size; // 5 pixels for char and 1 pixel for spacing, multiplied by scaling factor
        text++; // move pointer to next character
    }
}