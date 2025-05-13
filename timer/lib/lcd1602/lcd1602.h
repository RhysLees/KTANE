// lcd1602.h
#pragma once

#include <Arduino.h>
#include <Wire.h> // 👈 Add this before using TwoWire

// Use bit-shifted 8-bit I2C address style from Waveshare example
#define LCD1602_ADDRESS (0x7C >> 1) // 0x3E
#define RGB1602_ADDRESS (0xC0 >> 1) // 0x60

enum LcdColor
{
    LCD_COLOR_WHITE,
    LCD_COLOR_RED,
    LCD_COLOR_GREEN,
    LCD_COLOR_BLUE
};

// Initialize the LCD with given number of columns and rows, and a TwoWire instance
void initLcd1602(uint8_t cols = 16, uint8_t rows = 2, TwoWire &wireInstance = Wire);
void lcd1602Clear();
void lcd1602Display();
void lcd1602SetCursor(uint8_t col, uint8_t row);
void lcd1602Print(const String &text);
void lcd1602PrintLine(uint8_t row, const String &text);
void lcd1602SetColor(LcdColor color);
