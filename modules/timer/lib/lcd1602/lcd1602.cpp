// lcd1602.cpp
#include <Wire.h>
#include "lcd1602.h"

#define REG_RED 0x04
#define REG_GREEN 0x03
#define REG_BLUE 0x02
#define REG_MODE1 0x00
#define REG_MODE2 0x01
#define REG_OUTPUT 0x08

#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_FUNCTIONSET 0x20
#define LCD_SETDDRAMADDR 0x80

#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTDECREMENT 0x00
#define LCD_DISPLAYON 0x04
#define LCD_CURSOROFF 0x00
#define LCD_BLINKOFF 0x00
#define LCD_2LINE 0x08
#define LCD_5x8DOTS 0x00
#define LCD_4BITMODE 0x00

static uint8_t lcdCols = 16;
static uint8_t lcdRows = 2;
static TwoWire *lcdWire = &Wire;

static void lcd1602Command(uint8_t cmd)
{
    lcdWire->beginTransmission(LCD1602_ADDRESS);
    lcdWire->write(0x80); // Co = 1, RS = 0
    lcdWire->write(cmd);
    lcdWire->endTransmission();
}

static void lcd1602Write(uint8_t data)
{
    lcdWire->beginTransmission(LCD1602_ADDRESS);
    lcdWire->write(0x40); // Co = 0, RS = 1
    lcdWire->write(data);
    lcdWire->endTransmission();
}

static void lcd1602SetRegister(uint8_t reg, uint8_t value)
{
    lcdWire->beginTransmission(RGB1602_ADDRESS);
    lcdWire->write(reg);
    lcdWire->write(value);
    lcdWire->endTransmission();
}

void initLcd1602(uint8_t cols, uint8_t rows, TwoWire &wireInstance)
{
    lcdCols = cols;
    lcdRows = rows;
    lcdWire = &wireInstance;

    delay(50);

    uint8_t functionSet = LCD_FUNCTIONSET | LCD_2LINE | LCD_5x8DOTS | LCD_4BITMODE;
    lcd1602Command(functionSet);
    delay(10);
    lcd1602Command(functionSet);
    delay(10);
    lcd1602Command(functionSet);

    uint8_t displayControl = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    lcd1602Command(LCD_DISPLAYCONTROL | displayControl);
    delay(2);

    lcd1602Command(LCD_CLEARDISPLAY);
    delay(2);

    uint8_t entryMode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    lcd1602Command(LCD_ENTRYMODESET | entryMode);

    // RGB Backlight Init
    lcd1602SetRegister(REG_MODE1, 0x00);
    delay(2);
    lcd1602SetRegister(REG_OUTPUT, 0xFF);
    delay(2);
    lcd1602SetRegister(REG_MODE2, 0x20);

    lcd1602SetColor(LCD_COLOR_WHITE);
}

void lcd1602Clear()
{
    lcd1602Command(LCD_CLEARDISPLAY);
    delay(2);
}

void lcd1602Display()
{
    lcd1602Command(LCD_DISPLAYCONTROL | LCD_DISPLAYON);
    delay(2);
}

void lcd1602SetCursor(uint8_t col, uint8_t row)
{
    static const uint8_t rowOffsets[] = {0x00, 0x40, 0x14, 0x54};
    if (row >= lcdRows)
        row = lcdRows - 1;
    lcd1602Command(LCD_SETDDRAMADDR | (col + rowOffsets[row]));
}

void lcd1602Print(const String &text)
{
    for (char c : text)
    {
        lcd1602Write(c);
    }
}

void lcd1602PrintLine(uint8_t row, const String &text)
{
    lcd1602SetCursor(0, row);
    String padded = text;
    while (padded.length() < lcdCols)
        padded += ' ';
    lcd1602Print(padded.substring(0, lcdCols));
}

void lcd1602SetColor(LcdColor color)
{
    switch (color)
    {
    case LCD_COLOR_WHITE:
        lcd1602SetColorRGB(255, 255, 255);
        break;
    case LCD_COLOR_RED:
        lcd1602SetColorRGB(255, 0, 0);
        break;
    case LCD_COLOR_GREEN:
        lcd1602SetColorRGB(0, 255, 0);
        break;
    case LCD_COLOR_BLUE:
        lcd1602SetColorRGB(0, 0, 255);
        break;
    case LCD_COLOR_ORANGE:
        lcd1602SetColorRGB(255, 100, 0);
        break;
    case LCD_COLOR_PURPLE:
        lcd1602SetColorRGB(180, 0, 255);
        break;
    case LCD_COLOR_CYAN:
        lcd1602SetColorRGB(0, 255, 255);
        break;
    }
}

void lcd1602SetColorRGB(uint8_t r, uint8_t g, uint8_t b)
{
    lcd1602SetRegister(REG_RED, r);
    lcd1602SetRegister(REG_GREEN, g);
    lcd1602SetRegister(REG_BLUE, b);
}