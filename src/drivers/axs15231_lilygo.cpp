#include "drivers/axs15231_lilygo.h"

#include <Arduino.h>
#include <driver/spi_master.h>

#include "config/app_config.h"

namespace lilygo_display {
namespace {

constexpr uint32_t kSpiFrequencyHz = 32000000;
constexpr uint8_t kSpiMode = SPI_MODE0;
constexpr size_t kChunkPixels = 14400;

struct LcdCmd {
    uint8_t cmd;
    uint8_t data[8];
    uint8_t len;
};

const LcdCmd kInitCommands[] = {
    {0x28, {0x00}, 0x40},
    {0x10, {0x00}, 0x20},
    {0x11, {0x00}, 0x80},
    {0x29, {0x00}, 0x00},
};

spi_device_handle_t gSpi = nullptr;
bool gReady = false;

void sendCommand(uint32_t cmd, const uint8_t* data, uint32_t len) {
    if (gSpi == nullptr) {
        return;
    }

    spi_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    transaction.cmd = 0x02;
    transaction.addr = static_cast<uint32_t>(cmd) << 8;
    transaction.tx_buffer = (len == 0) ? nullptr : data;
    transaction.length = len * 8;

    digitalWrite(app::DISPLAY_QSPI_CS_PIN, LOW);
    ESP_ERROR_CHECK(spi_device_polling_transmit(gSpi, &transaction));
    digitalWrite(app::DISPLAY_QSPI_CS_PIN, HIGH);
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

    pinMode(app::DISPLAY_QSPI_CS_PIN, OUTPUT);
    pinMode(app::DISPLAY_RESET_PIN, OUTPUT);
    digitalWrite(app::DISPLAY_QSPI_CS_PIN, HIGH);

    digitalWrite(app::DISPLAY_RESET_PIN, HIGH);
    delay(130);
    digitalWrite(app::DISPLAY_RESET_PIN, LOW);
    delay(130);
    digitalWrite(app::DISPLAY_RESET_PIN, HIGH);
    delay(300);

    spi_bus_config_t buscfg = {};
    buscfg.data0_io_num = app::DISPLAY_QSPI_D0_PIN;
    buscfg.data1_io_num = app::DISPLAY_QSPI_D1_PIN;
    buscfg.sclk_io_num = app::DISPLAY_QSPI_SCK_PIN;
    buscfg.data2_io_num = app::DISPLAY_QSPI_D2_PIN;
    buscfg.data3_io_num = app::DISPLAY_QSPI_D3_PIN;
    buscfg.max_transfer_sz = static_cast<int>((kChunkPixels * 2) + 8);
    buscfg.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return false;
    }

    spi_device_interface_config_t devcfg = {};
    devcfg.command_bits = 8;
    devcfg.address_bits = 24;
    devcfg.mode = kSpiMode;
    devcfg.clock_speed_hz = kSpiFrequencyHz;
    devcfg.spics_io_num = -1;
    devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    devcfg.queue_size = 1;

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &gSpi);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return false;
    }

    for (const LcdCmd& command : kInitCommands) {
        sendCommand(command.cmd, command.data, command.len & 0x3F);
        if (command.len & 0x80) {
            delay(200);
        }
        if (command.len & 0x40) {
            delay(20);
        }
    }

    setRotation(0);
    gReady = true;
    return true;
}

void setRotation(uint8_t rotation) {
    uint8_t madctl = 0x00;
    switch (rotation & 0x03) {
        case 1:
            madctl = 0x40 | 0x20;
            break;
        case 2:
            madctl = 0x40 | 0x80;
            break;
        case 3:
            madctl = 0x20 | 0x80;
            break;
        case 0:
        default:
            madctl = 0x00;
            break;
    }
    sendCommand(0x36, &madctl, 1);
}

bool present(uint16_t* framebuffer, uint16_t width, uint16_t height) {
    if (!gReady || framebuffer == nullptr || width == 0 || height == 0) {
        return false;
    }

    setAddressWindow(0, 0, static_cast<uint16_t>(width - 1), static_cast<uint16_t>(height - 1));

    bool firstSend = true;
    size_t remaining = static_cast<size_t>(width) * static_cast<size_t>(height);
    uint16_t* data = framebuffer;

    digitalWrite(app::DISPLAY_QSPI_CS_PIN, LOW);
    while (remaining > 0) {
        size_t chunkSize = remaining > kChunkPixels ? kChunkPixels : remaining;

        spi_transaction_ext_t transaction;
        memset(&transaction, 0, sizeof(transaction));
        if (firstSend) {
            transaction.base.flags = SPI_TRANS_MODE_QIO;
            transaction.base.cmd = 0x32;
            transaction.base.addr = 0x002C00;
            firstSend = false;
        } else {
            transaction.base.flags = SPI_TRANS_MODE_QIO |
                                     SPI_TRANS_VARIABLE_CMD |
                                     SPI_TRANS_VARIABLE_ADDR |
                                     SPI_TRANS_VARIABLE_DUMMY;
            transaction.command_bits = 0;
            transaction.address_bits = 0;
            transaction.dummy_bits = 0;
        }
        transaction.base.tx_buffer = data;
        transaction.base.length = static_cast<uint32_t>(chunkSize * 16);
        ESP_ERROR_CHECK(spi_device_polling_transmit(gSpi, reinterpret_cast<spi_transaction_t*>(&transaction)));

        remaining -= chunkSize;
        data += chunkSize;
    }
    digitalWrite(app::DISPLAY_QSPI_CS_PIN, HIGH);
    return true;
}

}  // namespace lilygo_display
