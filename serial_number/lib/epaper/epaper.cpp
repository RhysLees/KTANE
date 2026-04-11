#include <epaper.h>
#include <atomic>

namespace
{
    using CommandType = uint8_t;

    enum : CommandType
    {
        CommandNone = 0,
        CommandDrawTag,
        CommandDrawCredit,
        CommandClear
    };

    struct EpaperCommand
    {
        CommandType type = CommandNone;
        char text[32] = {0};
    };

    constexpr size_t kQueueSize = 4;

    std::atomic<uint8_t> queueHead{0};
    std::atomic<uint8_t> queueTail{0};
    EpaperCommand commandQueue[kQueueSize];

    GxEPD2_3C<GxEPD2_266c, GxEPD2_266c::HEIGHT> epaper(GxEPD2_266c(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

    inline uint8_t advanceIndex(uint8_t index)
    {
        return static_cast<uint8_t>((index + 1u) % kQueueSize);
    }

    bool enqueueCommand(const EpaperCommand &command)
    {
        const uint8_t currentTail = queueTail.load(std::memory_order_relaxed);
        uint8_t nextTail = advanceIndex(currentTail);

        if (nextTail == queueHead.load(std::memory_order_acquire))
        {
            return false; // queue full
        }

        commandQueue[currentTail] = command;
        queueTail.store(nextTail, std::memory_order_release);
        return true;
    }

    bool dequeueCommand(EpaperCommand &outCommand)
    {
        const uint8_t currentHead = queueHead.load(std::memory_order_relaxed);
        if (currentHead == queueTail.load(std::memory_order_acquire))
        {
            return false; // queue empty
        }

        outCommand = commandQueue[currentHead];
        queueHead.store(advanceIndex(currentHead), std::memory_order_release);
        return true;
    }

    void centerText(const char *text, int16_t boxY, int16_t boxH, const GFXfont *font, uint16_t color)
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

    void initializeDisplay()
    {
        static bool initialized = false;
        if (initialized)
        {
            return;
        }

        SPI1.setSCK(10); // GP10 = SCK
        SPI1.setTX(11);  // GP11 = MOSI
        SPI1.begin();

        SPISettings settings(115200, MSBFIRST, SPI_MODE0);
        epaper.init(115200, true, 2, false, SPI1, settings);
        epaper.setRotation(1);
        epaper.setFullWindow();

        initialized = true;
    }

    void executeDrawTag(const char *serial)
    {
        initializeDisplay();
        epaper.firstPage();
        do
        {
            epaper.fillScreen(GxEPD_WHITE);
            int16_t halfHeight = epaper.height() / 2;
            epaper.fillRect(0, 0, epaper.width(), halfHeight, GxEPD_RED);

            centerText("SERIAL #", 0, halfHeight, &resolution_medium24pt7b, GxEPD_WHITE);
            centerText(serial, halfHeight, halfHeight, &resolution_medium36pt7b, GxEPD_BLACK);
        } while (epaper.nextPage());

        epaper.display();
        epaper.hibernate();
    }

    void executeDrawCredit()
    {
        initializeDisplay();
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

    void executeClear()
    {
        initializeDisplay();
        epaper.setFullWindow();
        epaper.firstPage();
        do
        {
            epaper.fillScreen(GxEPD_WHITE);
        } while (epaper.nextPage());
        epaper.hibernate();
    }

    void processCommand(const EpaperCommand &command)
    {
        switch (command.type)
        {
        case CommandDrawTag:
            executeDrawTag(command.text);
            break;
        case CommandDrawCredit:
            executeDrawCredit();
            break;
        case CommandClear:
            executeClear();
            break;
        default:
            break;
        }
    }
} // namespace

void epaperTaskSetup()
{
    initializeDisplay();
}

void epaperTaskLoop()
{
    EpaperCommand command;
    if (dequeueCommand(command))
    {
        processCommand(command);
    }
    else
    {
        delay(10);
    }
}

void epaperDrawTag(const String &serial)
{
    EpaperCommand command;
    command.type = CommandDrawTag;
    serial.toCharArray(command.text, sizeof(command.text));

    while (!enqueueCommand(command))
    {
        delay(1);
    }
}

void epaperDrawCredit()
{
    EpaperCommand command;
    command.type = CommandDrawCredit;

    while (!enqueueCommand(command))
    {
        delay(1);
    }
}

void epaperClear()
{
    EpaperCommand command;
    command.type = CommandClear;

    while (!enqueueCommand(command))
    {
        delay(1);
    }
}
