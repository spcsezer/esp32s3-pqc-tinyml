#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#define TAG "adxl"

#define PIN_SCLK 12
#define PIN_MOSI 11
#define PIN_MISO 13
#define PIN_CS 10

#define REG_DEVID 0x00
#define REG_BW_RATE 0x2C
#define REG_POWER_CTL 0x2D
#define REG_DATA_FORMAT 0x31
#define REG_DATAX0 0x32

static spi_device_handle_t adxl;

static void adxl_write(uint8_t reg, uint8_t val)
{
    spi_transaction_t t = {.flags = SPI_TRANS_USE_TXDATA, .length = 16};
    t.tx_data[0] = reg;
    t.tx_data[1] = val;
    ESP_ERROR_CHECK(spi_device_transmit(adxl, &t));
}

static uint8_t adxl_read(uint8_t reg)
{
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 16,
    };
    t.tx_data[0] = 0x80 | reg; // bit7 = 1: okuma
    t.tx_data[1] = 0;
    ESP_ERROR_CHECK(spi_device_transmit(adxl, &t));
    return t.rx_data[1];
}

static void adxl_read_xyz(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t tx[7] = {0xC0 | REG_DATAX0}; // okuma + çoklu bayt
    uint8_t rx[7] = {0};
    spi_transaction_t t = {.length = 7 * 8, .tx_buffer = tx, .rx_buffer = rx};
    ESP_ERROR_CHECK(spi_device_transmit(adxl, &t));
    *x = (int16_t)((rx[2] << 8) | rx[1]);
    *y = (int16_t)((rx[4] << 8) | rx[3]);
    *z = (int16_t)((rx[6] << 8) | rx[5]);
}

void app_main(void)
{
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 1 * 1000 * 1000, // 1 MHz
        .mode = 3,                         // ADXL345 SPI modu 3
        .spics_io_num = PIN_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &adxl));

    uint8_t id = adxl_read(REG_DEVID);
    ESP_LOGI(TAG, "DEVID = 0x%02X (beklenen 0xE5)", id);
    if (id != 0xE5)
    {
        ESP_LOGE(TAG, "Sensor bulunamadi, baglantiyi kontrol et");
        return;
    }

    adxl_write(REG_DATA_FORMAT, 0x08); // tam çözünürlük, ±2g
    adxl_write(REG_BW_RATE, 0x0A);     // 100 Hz (sonra 800 Hz yapacağız)
    adxl_write(REG_POWER_CTL, 0x08);   // ölçüm modu

    while (1)
    {
        int16_t x, y, z;
        adxl_read_xyz(&x, &y, &z);
        ESP_LOGI(TAG, "x=%6d y=%6d z=%6d  |  %.2f %.2f %.2f g",
                 x, y, z, x * 0.0039f, y * 0.0039f, z * 0.0039f);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}