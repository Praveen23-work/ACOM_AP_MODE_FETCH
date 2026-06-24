#ifndef MODBUS_ESP
#define MODBUS_ESP

#include <sensor_config.h>

void MODBUS_handler( uint8_t SensorSelect, uint8_t *modbusRequest);
uint8_t MODBUS_Sensor_Count();
uint8_t len_of_modbus_response();
uint16_t modbus_crc(uint8_t *data, uint8_t len);


float decodeModbusResponse_Chl(uint8_t *receivedData);
float decodeModbusResponse_UFM( uint8_t *floatBytes);
float decodeModbusResponse_Cumulative(uint8_t *receivedData);
float decodeModbusResponse_TDS(uint8_t *receivedData);

#endif