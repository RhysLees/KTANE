#pragma once

#include <Arduino.h>
#include <Wire.h>

// Use bit-shifted 8-bit I2C address style from Waveshare example
#define LCD1602_ADDRESS (0x7C >> 1) // 0x3E
#define RGB1602_ADDRESS (0xC0 >> 1) // 0x60

// Predefined color options
enum LcdColor
{
    LCD_COLOR_WHITE,
    LCD_COLOR_RED,
    LCD_COLOR_GREEN,
    LCD_COLOR_BLUE,
    LCD_COLOR_ORANGE,
    LCD_COLOR_PURPLE,
    LCD_COLOR_CYAN
};

// Initialize the LCD with given number of columns and rows, and a TwoWire instance
void initLcd1602(uint8_t cols = 16, uint8_t rows = 2, TwoWire &wireInstance = Wire);

// Clear the display
void lcd1602Clear();

// Turn on the display
void lcd1602Display();

// Set cursor to a specific position
void lcd1602SetCursor(uint8_t col, uint8_t row);

// Print text starting at the current cursor
void lcd1602Print(const String &text);

// Print text padded and trimmed to one full line
void lcd1602PrintLine(uint8_t row, const String &text);

// Set color using predefined names
void lcd1602SetColor(LcdColor color);

// Set custom RGB color directly
void lcd1602SetColorRGB(uint8_t r, uint8_t g, uint8_t b);
