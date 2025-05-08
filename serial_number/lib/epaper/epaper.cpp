#include <epaper.h>

GxEPD2_3C<GxEPD2_266c, GxEPD2_266c::HEIGHT> epaper(GxEPD2_266c(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

static void centerText(const char *text, int16_t boxY, int16_t boxH, const GFXfont *font, uint16_t color)
{
    epaper.setFont(font);
    int16_t x1, y1;
    uint16_t w, h;
    epaper.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (epaper.width() - w) / 2 - x1;
    int16_t y = boxY + (boxH - h) / 2 + h;
    epaper.setCursor(x, y);
    epaper.setTextColor(color);
    epaper.print(text);
}

void epaperInit()
{
    SPI1.setSCK(10); // GP10 = SCK
    SPI1.setTX(11);  // GP11 = MOSI
    SPI1.begin();

    SPISettings settings(115200, MSBFIRST, SPI_MODE0);
    epaper.init(115200, true, 2, false, SPI1, settings);
    epaper.setRotation(1);
    epaper.setFullWindow();
}

void epaperDrawTag(const String &serial)
{
    epaper.firstPage();
    do
    {
        epaper.fillScreen(GxEPD_WHITE);
        int16_t halfHeight = epaper.height() / 2;
        epaper.fillRect(0, 0, epaper.width(), halfHeight, GxEPD_RED);

        centerText("SERIAL #", 0, halfHeight, &resolution_medium24pt7b, GxEPD_WHITE);
        centerText(serial.c_str(), halfHeight, halfHeight, &resolution_medium36pt7b, GxEPD_BLACK);
    } while (epaper.nextPage());

    epaper.display();
    epaper.hibernate();
}

void epaperDrawCredit()
{
    epaper.firstPage();
    do
    {
        epaper.fillScreen(GxEPD_WHITE);
        int16_t halfHeight = epaper.height() / 2;
        epaper.fillRect(0, 0, epaper.width(), halfHeight, GxEPD_RED);

        centerText("KTANE IRL", 0, halfHeight, &resolution_medium24pt7b, GxEPD_WHITE);
        centerText("By Rhys Lees", halfHeight, halfHeight, &resolution_medium36pt7b, GxEPD_BLACK);
    } while (epaper.nextPage());

    epaper.display();
    epaper.hibernate();
}

void epaperClear()
{
    epaper.setFullWindow();
    epaper.firstPage();
    do
    {
        epaper.fillScreen(GxEPD_WHITE);
    } while (epaper.nextPage());
    epaper.hibernate();
}