/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
// #include "i2c_handler.h"
#include <stdio.h>
#include <time.h>

#define SDA_PIN 21
#define SCL_PIN 22

#define RTC_ADDRESS 0x6F

uint8_t int_to_bcd(int val);
int bcd_to_int(uint8_t val);
uint8_t readData(uint8_t reg);
void writeData(uint8_t reg, uint8_t value);
void enableBatteryBackup(bool enable);
void startClock(void);
void stopClock(void);
bool setClock(struct tm *tm);
// void readClock();
i2c_master_dev_handle_t dev_handel;




/*    i2c.h  Header starts   */

/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdint.h>
// #include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_device_config_t eeprom_device;  /*!< Configuration for eeprom device */
    uint8_t addr_wordlen;               /*!< block address wordlen */
    uint8_t write_time_ms;              /*!< eeprom write time, typically 10ms*/
} i2c_eeprom_config_t;

struct i2c_eeprom_t {
    i2c_master_dev_handle_t i2c_dev;      /*!< I2C device handle */
    uint8_t addr_wordlen;                 /*!< block address wordlen */
    uint8_t *buffer;                      /*!< I2C transaction buffer */
    uint8_t write_time_ms;                /*!< I2C eeprom write time(ms)*/
};

typedef struct i2c_eeprom_t i2c_eeprom_t;

/* handle of EEPROM device */
typedef struct i2c_eeprom_t *i2c_eeprom_handle_t;

/**
 * @brief Init an EEPROM device.
 *
 * @param[in] bus_handle I2C master bus handle
 * @param[in] eeprom_config Configuration of EEPROM
 * @param[out] eeprom_handle Handle of EEPROM
 * @return ESP_OK: Init success. ESP_FAIL: Not success.
 */
esp_err_t i2c_eeprom_init(i2c_master_bus_handle_t bus_handle, const i2c_eeprom_config_t *eeprom_config, i2c_eeprom_handle_t *eeprom_handle);

/**
 * @brief Write data to EEPROM
 *
 * @param[in] eeprom_handle EEPROM handle
 * @param[in] address Block address inside EEPROM
 * @param[in] data Data to write
 * @param[in] size Data write size
 * @return ESP_OK: Write success. Otherwise failed, please check I2C function fail reason.
 */
esp_err_t i2c_eeprom_write(i2c_eeprom_handle_t eeprom_handle, uint32_t address, const uint8_t *data, uint32_t size);

/**
 * @brief Read data from EEPROM
 *
 * @param eeprom_handle EEPROM handle
 * @param address Block address inside EEPROM
 * @param data Data read from EEPROM
 * @param size Data read size
 * @return ESP_OK: Read success. Otherwise failed, please check I2C function fail reason.
 */
esp_err_t i2c_eeprom_read(i2c_eeprom_handle_t eeprom_handle, uint32_t address, uint8_t *data, uint32_t size);

/**
 * @brief Wait eeprom finish. Typically 5ms
 *
 * @param eeprom_handle EEPROM handle
 */
void i2c_eeprom_wait_idle(i2c_eeprom_handle_t eeprom_handle);

#ifdef __cplusplus
}
#endif



/*      i2c.h  Header Stops  */






// void app_main(void)
void rtc_init_()
{
    i2c_master_bus_config_t i2c_master_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
        .scl_io_num = SCL_PIN,
        .sda_io_num = SDA_PIN};
    i2c_master_bus_handle_t i2c_master_bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_master_bus_config, &i2c_master_bus_handle));

    i2c_device_config_t i2c_device_config = {
        .scl_speed_hz = 100000,
        .device_address = RTC_ADDRESS};

    i2c_master_bus_add_device(i2c_master_bus_handle, &i2c_device_config, &dev_handel);


    /*enable battery backup*/
    enableBatteryBackup(true);
    /*start clock*/
    startClock();
}



void set_rtc_time(int day, int week_day, int month, int year, int hour, int min, int sec)
{
    struct tm dateTime = {
        .tm_sec = sec,       // was hardcoded 0 — caused up to 59s error on every BLE sync
        .tm_min = min,
        .tm_hour = hour,
        .tm_wday = week_day,
        .tm_mday = day,
        .tm_mon = month,
        .tm_year = year};

    setClock(&dateTime);
}

    // uint8_t write_buffer[]= {0X00,
    //     int_to_bcd(1),
    //     int_to_bcd(0),
    //     int_to_bcd(19),
    //     int_to_bcd(4),
    //     int_to_bcd(9),
    //     int_to_bcd(1),
    //     int_to_bcd(25),
    //     };
    // ESP_ERROR_CHECK(i2c_master_transmit(dev_handel,write_buffer,sizeof(write_buffer),-1));

    /********* ENABLE BATTERY BACKUP *********/
    // uint8_t write_buffer_for_read[] = {0X03};
    // uint8_t read_buffer[1];
    // uint8_t write_buffer[] = {0X03, 0x00}; // Enable battery backup
    // ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handel, write_buffer_for_read, sizeof(write_buffer_for_read), read_buffer, sizeof(read_buffer), -1));
    // read_buffer[0] = bcd_to_int(read_buffer[0]);
    // printf("vbat: %d\n", (read_buffer[0]));
    // read_buffer[0] |= (1 << 3); // Enable battery backup
    // write_buffer[0] = 0x03;     // Register address to write
    // write_buffer[1] = int_to_bcd(read_buffer[0]);
    // ESP_ERROR_CHECK(i2c_master_transmit(dev_handel, write_buffer, sizeof(write_buffer), -1));
    // ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handel, write_buffer_for_read, sizeof(write_buffer_for_read), read_buffer, sizeof(read_buffer), -1));
    // printf("vbat: %d\n", bcd_to_int(read_buffer[0]));

    /********* enable crystal oscillator *********/
    // write_buffer_for_read[0] = 0x00; // Register address to read
    // ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handel, write_buffer_for_read, sizeof(write_buffer_for_read), read_buffer, sizeof(read_buffer), -1));
    // read_buffer[0] = bcd_to_int(read_buffer[0]);
    // printf("oscillator: %d\n", (read_buffer[0]));
    // read_buffer[0] |= (1 << 7); // Enable crystal oscillator
    // write_buffer[0] = 0x00;     // Register address to write
    // write_buffer[1] = int_to_bcd(read_buffer[0]);
    // ESP_ERROR_CHECK(i2c_master_transmit(dev_handel, write_buffer, sizeof(write_buffer), -1));
    // ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handel, write_buffer_for_read, sizeof(write_buffer_for_read), read_buffer, sizeof(read_buffer), -1));
    // printf("oscillator: %d\n", bcd_to_int(read_buffer[0]));

    // char time_buffer[50];
    // strftime(time_buffer,sizeof(time_buffer),"%c",&dateTime);





    // while (true)
    // {
    //     readClock();
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }



uint8_t int_to_bcd(int val)
{
    return ((val / 10) << 4) | (val % 10);
}

int bcd_to_int(uint8_t val)
{
    return (((val & 0xf0) >> 4) * 10) + (val & 0x0f);
}

uint8_t readData(uint8_t reg)
{
    uint8_t write_buffer_for_read[] = {reg};
    uint8_t read_buffer[1];
    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handel, write_buffer_for_read, sizeof(write_buffer_for_read), read_buffer, sizeof(read_buffer), -1));
    return read_buffer[0];
}

void writeData(uint8_t reg, uint8_t value)
{
    uint8_t write_buffer[] = {reg, value};
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handel, write_buffer, sizeof(write_buffer), -1));
}

void enableBatteryBackup(bool enable)
{ // Writing to register 0x03 will clear the power-fail flag and
    // zero the power fail and power restored timestamps. Only
    // actually enable the bit if it is not already set.
    if ((bool)(readData(0x03) & 0x08) == enable)
        // State matches that requested
        return;

    stopClock();
    uint8_t d = readData(0x03);
    if (enable)
        d |= 0x08;
    else
        d &= 0xf7;
    writeData((uint8_t)0x03, d);
    startClock();
}

// Start the clock. If bcdSec is >= 0 zero use its value for the
// seconds instead of reading the current value. This helps reduce the
// number of I2C reads and writes required.
void startClock()
{
    int16_t bcdSec = -1; // Default to reading the current value
    uint8_t s;
    uint8_t reg = 0;
    if (bcdSec < 0)
        s = readData(reg);
    else
        s = (uint8_t)(bcdSec & 0x7f);

    uint8_t s2 = s;
    s2 |= 0x80; // Enable start bit
    // Write back the data if it is different to the contents of the
    // register.  Always write back if the data wasn't fetched with
    // readData as the contents of the stop bit are unknown.
    if (s != s2 || bcdSec < 0)
        writeData(reg, s2);
}

void stopClock(void)
{
    uint8_t reg = 0;
    uint8_t s = readData(reg);
    s &= 0x7f;
    writeData(reg, s);
}

bool setClock(struct tm *tm)
{
    uint8_t clockHalt = 0;
    uint8_t osconEtc = 0;

    stopClock();

    // Preserve OSCON, VBAT, VBATEN on MCP7941x
    osconEtc = readData((uint8_t)0x03) & 0x38;

    writeData((uint8_t)0x00, int_to_bcd(tm->tm_sec) | clockHalt);

    writeData((uint8_t)0x01, int_to_bcd(tm->tm_min));
    writeData((uint8_t)0x02, int_to_bcd(tm->tm_hour));                // Forces 24h mode
    writeData((uint8_t)0x03, int_to_bcd(tm->tm_wday + 1) | osconEtc); // Clock uses [1..7]

    writeData((uint8_t)0x04, int_to_bcd(tm->tm_mday));

    // Leap year bit on MCP7941x is read-only so ignore it
    writeData((uint8_t)0x05, int_to_bcd(tm->tm_mon + 1));

    writeData((uint8_t)0x06, int_to_bcd(tm->tm_year % 100));

    // startClock(decToBcd(tm->tm_sec));
    startClock();
    return true;
}

/* Read a time from the clock. The same function is also used to read
 * the alarms as the register layout is essentially identical but with
 * week day and year omitted.
 */

 void readClock(struct tm *time)
{
    if (!time) return;  // Safety check

    time->tm_sec  = bcd_to_int(readData(0x00) & 0x7f);
    time->tm_min  = bcd_to_int(readData(0x01) & 0x7f);
    time->tm_hour = bcd_to_int(readData(0x02) & 0x3f);
    time->tm_wday = bcd_to_int(readData(0x03) & 0x07) - 1; // Clock uses [1..7]
    time->tm_mday = bcd_to_int(readData(0x04) & 0x3f);
    time->tm_mon  = bcd_to_int(readData(0x05) & 0x1f) - 1; // Clock uses [1..12]
    time->tm_year = bcd_to_int(readData(0x06)) + 100;      // Assume 21st century
    time->tm_yday = -1;                                    // Not supported by the clock
}


// void readClock()
// {
//     struct tm time;
//     time.tm_sec = bcd_to_int(readData(0x00) & 0x7f);
//     time.tm_min = bcd_to_int(readData(0x01) & 0x7f);
//     time.tm_hour = bcd_to_int(readData(0x02) & 0x3f);
//     time.tm_wday = bcd_to_int(readData(0x03) & 0x07) - 1; // Clock uses [1..7]
//     time.tm_mday = bcd_to_int(readData(0x04) & 0x3f);
//     time.tm_mon = bcd_to_int(readData(0x05) & 0x1f) - 1; // Clock uses [1..12]
//     time.tm_year = bcd_to_int(readData(0x06)) + 100;     // Assume 21st century
//     time.tm_yday = -1;                                   // Not supported by the clock

//     printf("Time: %02d:%02d:%02d %02d/%02d/%04d\n",
//            time.tm_hour, time.tm_min, time.tm_sec,
//            time.tm_mday, time.tm_mon + 1, time.tm_year + 1900);
// }









                                    /*  i2C Librarry Starts */

/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_types.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
// #include "i2c_handler.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_EEPROM_MAX_TRANS_UNIT (48)
// Different EEPROM device might share one I2C bus

static const char TAG[] = "i2c-eeprom";

esp_err_t i2c_eeprom_init(i2c_master_bus_handle_t bus_handle, const i2c_eeprom_config_t *eeprom_config, i2c_eeprom_handle_t *eeprom_handle)
{
    esp_err_t ret = ESP_OK;
    i2c_eeprom_handle_t out_handle;
    out_handle = (i2c_eeprom_handle_t)calloc(1, sizeof(*out_handle));
    ESP_GOTO_ON_FALSE(out_handle, ESP_ERR_NO_MEM, err, TAG, "no memory for i2c eeprom device");

    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = eeprom_config->eeprom_device.scl_speed_hz,
        .device_address = eeprom_config->eeprom_device.device_address,
    };

    if (out_handle->i2c_dev == NULL) {
        ESP_GOTO_ON_ERROR(i2c_master_bus_add_device(bus_handle, &i2c_dev_conf, &out_handle->i2c_dev), err, TAG, "i2c new bus failed");
    }

    out_handle->buffer = (uint8_t*)calloc(1, eeprom_config->addr_wordlen + I2C_EEPROM_MAX_TRANS_UNIT);
    ESP_GOTO_ON_FALSE(out_handle->buffer, ESP_ERR_NO_MEM, err, TAG, "no memory for i2c eeprom device buffer");

    out_handle->addr_wordlen = eeprom_config->addr_wordlen;
    out_handle->write_time_ms = eeprom_config->write_time_ms;
    *eeprom_handle = out_handle;

    return ESP_OK;

err:
    if (out_handle && out_handle->i2c_dev) {
        i2c_master_bus_rm_device(out_handle->i2c_dev);
    }
    free(out_handle);
    return ret;
}

esp_err_t i2c_eeprom_write(i2c_eeprom_handle_t eeprom_handle, uint32_t address, const uint8_t *data, uint32_t size)
{
    ESP_RETURN_ON_FALSE(eeprom_handle, ESP_ERR_NO_MEM, TAG, "no mem for buffer");
    for (int i = 0; i < eeprom_handle->addr_wordlen; i++) {
        eeprom_handle->buffer[i] = (address & (0xff << ((eeprom_handle->addr_wordlen - 1 - i) * 8))) >> ((eeprom_handle->addr_wordlen - 1 - i) * 8);
    }
    memcpy(eeprom_handle->buffer + eeprom_handle->addr_wordlen, data, size);

    return i2c_master_transmit(eeprom_handle->i2c_dev, eeprom_handle->buffer, eeprom_handle->addr_wordlen + size, -1);
}

esp_err_t i2c_eeprom_read(i2c_eeprom_handle_t eeprom_handle, uint32_t address, uint8_t *data, uint32_t size)
{
    ESP_RETURN_ON_FALSE(eeprom_handle, ESP_ERR_NO_MEM, TAG, "no mem for buffer");
    for (int i = 0; i < eeprom_handle->addr_wordlen; i++) {
        eeprom_handle->buffer[i] = (address & (0xff << ((eeprom_handle->addr_wordlen - 1 - i) * 8))) >> ((eeprom_handle->addr_wordlen - 1 - i) * 8);
    }

    return i2c_master_transmit_receive(eeprom_handle->i2c_dev, eeprom_handle->buffer, eeprom_handle->addr_wordlen, data, size, -1);
}

void i2c_eeprom_wait_idle(i2c_eeprom_handle_t eeprom_handle)
{
    // This is time for EEPROM Self-Timed Write Cycle
    vTaskDelay(pdMS_TO_TICKS(eeprom_handle->write_time_ms));
}





/* i2c library ends */