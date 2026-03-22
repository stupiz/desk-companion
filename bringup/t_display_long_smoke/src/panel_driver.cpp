#include "panel_driver.h"

#include <Arduino.h>
#include <driver/spi_master.h>

namespace smoke_panel {
namespace {

constexpr int8_t kPinCs = 12;
constexpr int8_t kPinSck = 17;
constexpr int8_t kPinD0 = 13;
constexpr int8_t kPinD1 = 18;
constexpr int8_t kPinD2 = 21;
constexpr int8_t kPinD3 = 14;
constexpr int8_t kPinReset = 16;
constexpr int8_t kPinBacklight = 1;

constexpr uint32_t kSpiFrequencyHz = 32000000;
constexpr size_t kChunkPixels = 14400;

struct LcdCommand {
    uint8_t cmd;
    uint8_t data[8];
    uint8_t len;
};

const LcdCommand kInitCommands[] = {
    {0x28, {0x00}, 0x40},
    {0x10, {0x00}, 0x20},
    {0x11, {0x00}, 0x80},
    {0x29, {0x00}, 0x00},
};

spi_device_handle_t gSpi = nullptr;
bool gReady = false;

void sendCommand(uint32_t cmd, const uint8_t* data, uint32_t len) {
    spi_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    transaction.cmd = 0x02;
    transaction.addr = cmd << 8;
    transaction.tx_buffer = (len == 0) ? nullptr : data;
    transaction.length = len * 8;

    digitalWrite(kPinCs, LOW);
    ESP_ERROR_CHECK(spi_device_polling_transmit(gSpi, &transaction));
    digitalWrite(kPinCs, HIGH);
}

void setAddressWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    const uint8_t col[] = {
        static_cast<uint8_t>(x1 >> 8),
        static_cast<uint8_t>(x1),
        static_cast<uint8_t>(x2 >> 8),
        static_cast<uint8_t>(x2),
    };
    const uint8_t row[] = {
        static_cast<uint8_t>(y1 >> 8),
        static_cast<uint8_t>(y1),
        static_cast<uint8_t>(y2 >> 8),
        static_cast<uint8_t>(y2),
    };
    sendCommand(0x2A, col, sizeof(col));
    sendCommand(0x2B, row, sizeof(row));
}

}  // namespace

bool begin() {
    if (gReady) {
        return true;
    }

    pinMode(kPinCs, OUTPUT);
    pinMode(kPinReset, OUTPUT);
    pinMode(kPinBacklight, OUTPUT);
    digitalWrite(kPinCs, HIGH);
    digitalWrite(kPinBacklight, LOW);

    digitalWrite(kPinReset, HIGH);
    delay(130);
    digitalWrite(kPinReset, LOW);
    delay(130);
    digitalWrite(kPinReset, HIGH);
    delay(300);

    spi_bus_config_t buscfg = {};
    buscfg.data0_io_num = kPinD0;
    buscfg.data1_io_num = kPinD1;
    buscfg.sclk_io_num = kPinSck;
    buscfg.data2_io_num = kPinD2;
    buscfg.data3_io_num = kPinD3;
    buscfg.max_transfer_sz = static_cast<int>((kChunkPixels * 2) + 8);
    buscfg.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        Serial.printf("spi_bus_initialize failed: %d\n", static_cast<int>(ret));
        return false;
    }

    spi_device_interface_config_t devcfg = {};
    devcfg.command_bits = 8;
    devcfg.address_bits = 24;
    devcfg.mode = SPI_MODE0;
    devcfg.clock_speed_hz = kSpiFrequencyHz;
    devcfg.spics_io_num = -1;
    devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    devcfg.queue_size = 1;

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &gSpi);
    if (ret != ESP_OK) {
        Serial.printf("spi_bus_add_device failed: %d\n", static_cast<int>(ret));
        return false;
    }

    for (const LcdCommand& command : kInitCommands) {
        sendCommand(command.cmd, command.data, command.len & 0x3F);
        if (command.len & 0x80) {
            delay(200);
        }
        if (command.len & 0x40) {
            delay(20);
        }
    }

    const uint8_t madctl = 0x00;
    sendCommand(0x36, &madctl, 1);

    digitalWrite(kPinBacklight, HIGH);
    gReady = true;
    return true;
}

bool pushNative(const uint16_t* pixels, uint16_t width, uint16_t height) {
    if (!gReady || pixels == nullptr || width == 0 || height == 0) {
        return false;
    }

    setAddressWindow(0, 0, static_cast<uint16_t>(width - 1), static_cast<uint16_t>(height - 1));

    size_t remaining = static_cast<size_t>(width) * static_cast<size_t>(height);
    const uint16_t* cursor = pixels;
    bool firstChunk = true;

    digitalWrite(kPinCs, LOW);
    while (remaining > 0) {
        const size_t chunkPixels = remaining > kChunkPixels ? kChunkPixels : remaining;

        spi_transaction_ext_t transaction;
        memset(&transaction, 0, sizeof(transaction));
        if (firstChunk) {
            transaction.base.flags = SPI_TRANS_MODE_QIO;
            transaction.base.cmd = 0x32;
            transaction.base.addr = 0x002C00;
            firstChunk = false;
        } else {
            transaction.base.flags = SPI_TRANS_MODE_QIO |
                                     SPI_TRANS_VARIABLE_CMD |
                                     SPI_TRANS_VARIABLE_ADDR |
                                     SPI_TRANS_VARIABLE_DUMMY;
            transaction.command_bits = 0;
            transaction.address_bits = 0;
            transaction.dummy_bits = 0;
        }
        transaction.base.tx_buffer = cursor;
        transaction.base.length = static_cast<uint32_t>(chunkPixels * 16);

        ESP_ERROR_CHECK(spi_device_polling_transmit(gSpi, reinterpret_cast<spi_transaction_t*>(&transaction)));

        cursor += chunkPixels;
        remaining -= chunkPixels;
    }
    digitalWrite(kPinCs, HIGH);
    return true;
}

}  // namespace smoke_panel
