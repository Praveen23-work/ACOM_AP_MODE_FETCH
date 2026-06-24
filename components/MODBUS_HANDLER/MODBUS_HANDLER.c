#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "MODBUS_HANDLER.h"

Sensor MODBUS_sensors[] = {
    {
        .slave_id = 1,
        .type     = SENSOR_TYPE_TDS,
        .data     = { .tds = {0} },
        .num_regs = sizeof(TdsData) / 2
    },

    {
        .slave_id = 2,
        .type     = SENSOR_TYPE_PH,
        .data     = { .ph = {0} },
        .num_regs = sizeof(PhData) / 2
    },
    {
        .slave_id = 3,
        .type     = SENSOR_TYPE_CHL,
        .data     = { .chl = {0} },
        .num_regs = sizeof(ChlData) / 2
    },

    // {
    //     .slave_id = 3,
    //     .type     = SENSOR_TYPE_TEMP,
    //     .data     = { .temp = {0} },
    //     .num_regs = sizeof(TempData) / 2
    // }
};


static const size_t num_sensors = sizeof(MODBUS_sensors) / sizeof(MODBUS_sensors[0]);

size_t get_num_sensors(void) {
    return num_sensors;
}



uint16_t modbus_crc16(uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc = crc >> 1;
        }
    }
    return crc;
}


uint16_t build_read_request(uint8_t *buffer, uint8_t slave_id, uint16_t reg_addr, uint16_t num_regs) {
    buffer[0] = slave_id;
    buffer[1] = 0x03;
    buffer[2] = (reg_addr >> 8) & 0xFF;
    buffer[3] = reg_addr & 0xFF;
    buffer[4] = (num_regs >> 8) & 0xFF;
    buffer[5] = num_regs & 0xFF;

    uint16_t crc = modbus_crc16(buffer, 6);
    buffer[6] = crc & 0xFF;
    buffer[7] = (crc >> 8) & 0xFF;

    return 8; // Length
}


uint16_t build_write_request(uint8_t *buffer, uint8_t slave_id, uint16_t reg_addr, uint16_t value) {
    buffer[0] = slave_id;
    buffer[1] = 0x06;
    buffer[2] = (reg_addr >> 8) & 0xFF;
    buffer[3] = reg_addr & 0xFF;
    buffer[4] = (value >> 8) & 0xFF;
    buffer[5] = value & 0xFF;

    uint16_t crc = modbus_crc16(buffer, 6);
    buffer[6] = crc & 0xFF;
    buffer[7] = (crc >> 8) & 0xFF;

    return 8;
}




void decode_sensor_data(Sensor* sensor, const uint8_t* data) {
    switch (sensor->type) {
        case SENSOR_TYPE_TDS:
            sensor->data.tds.tds_ch1  = (data[0] << 8) | data[1];
            sensor->data.tds.temp_ch1 = (data[2] << 8) | data[3];
            sensor->data.tds.tds_ch2  = (data[4] << 8) | data[5];
            sensor->data.tds.temp_ch2 = (data[6] << 8) | data[7];
            break;

        case SENSOR_TYPE_PH:
            memcpy(&sensor->data.ph.ph_ch1, &data[0], 4);
            // memcpy(&sensor->data.ph.ph_ch2, &data[4], 4);
            break;

        case SENSOR_TYPE_TEMP:
            // memcpy(&sensor->data.temp.temperature, &data[0], 4);
            // memcpy(&sensor->data.temp.humidity, &data[4], 4);
            break;

       case SENSOR_TYPE_CHL:
            memcpy(&sensor->data.chl.chlorine, &data[0], 4);
            break;
        
        case SENSOR_TYPE_UFM:
        break;
    }
}