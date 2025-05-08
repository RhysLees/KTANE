// epaper.h
#pragma once

#include <Arduino.h>
#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <fonts.h>

#define EPD_CS 5
#define EPD_DC 6
#define EPD_RST 7
#define EPD_BUSY 8

extern GxEPD2_3C<GxEPD2_266c, GxEPD2_266c::HEIGHT> epaper;

void epaperInit();
void epaperDrawTag(const String &serial);
void epaperDrawCredit();
void epaperClear();