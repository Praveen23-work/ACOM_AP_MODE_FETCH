#include <stdio.h>
#include "modbus_esp.h"
#include <math.h>

uint8_t len_of_response_num = 0;

// float pressureVal, phVal, tempVal, ufFlowVal1, ufVolumeVal1, chlorineVal, tdsVal,  ufFlowVal2, ufVolumeVal2;
    
// sensor_config_t default_sensors[MAX_SENSORS] = {
//        0 {"Pressure Sensor", "PSR", true, true, 0.0, 1.00, 0.0, false},
//        1 {"pH", "PH", true, true, 0.0, 1.00, 0.0, false},
//        2 {"TDS", "TDS", true, true, 0.0, 1.00, 0.0, false},
//        3 {"Chlorine Sensor", "CLO", true, true, 0.0, 1.00, 0.0, false},
//        4 {"TempW", "TPW", true, false, 0.0, 1.00, 0.0, false},
//        5 {"Weight Sensor", "WGT", true, true, 0.0, 1.00, 0.0, false},
//        6 {"Turbidity", "TBD", true, false, 0.0, 1.00, 0.0, false},
//        7 {"Ultrasonic Flow", "UFM1_F", true, true, 0.0, 1.00, 0.0, false},
//        8 {"Ultrasonic Volume", "UFM1_V", true, true, 0.0, 2.00, 0.0, false},
//        9 {"Ultrasonic Flow", "UFM2_F", true, true, 0.0, 1.00, 0.0, false},
//        10 {"Ultrasonic Flow2", "UFM2_V", true, true, 0.0, 2.00, 0.0, false},
//        11{"Chlorine NC", "CLO_NC", true, true, 0.0, 1.00, 0.0, false},

//        12 {"TempA", "TPA", true, false, 0.0, 1.00, 0.0, false},
//        13 {"Flow Sensor", "FLW", true, false, 0.0, 1.00, 0.0, false},
//        14 {"Switch 1", "SW1", true, false, 0.0, 1.00, 0.0, false},
//        15 {"Switch 2", "SW2", true, false, 0.0, 1.00, 0.0, false}
//     };

// sensor_map_t modbus_map[] = {
//     {PRESSURE, 0, &pressureVal},
//     {PH, 1, &phVal},
//     {TDS, 2, &tdsVal},
//     {WEIGHT_SENSOR, 5, &weight_val},
//     {TURBIDITY, 6, &turbidityVal},
//     {CHLORINE_PC, 3, &chlorine_PCVal},
//     {CHLORINE_NC, 11, &chlorine_NCVal},
//     {UFM1_FLOW, 7, &ufFlowVal1},
//     {UFM1_VOLUME, 8, &ufVolumeVal1},
//     {UFM2_FLOW, 9, &ufFlowVal2},
//     {UFM2_VOLUME, 10, &ufVolumeVal2},
//     {SW1_mod, 14, &SW1_mod_val},
//     {SW2_mod, 15, &SW2_mod_val},
// };

// const size_t modbus_map_size = sizeof(modbus_map) / sizeof(modbus_map[0]);

void MODBUS_handler( uint8_t SensorSelect, uint8_t *modbusRequest)
{
    if (SensorSelect == PRESSURE) 
    {
    // 0x01,  // Slave Address
    // 0x03,  // Function Code
    // 0x00,  // Start Address High
    // 0x07,  // Start Address Low
    // 0x00,  // Quantity High
    // 0x01,  // Quantity Low
    // 0X35,  // CRC Low
    
    // 0XCB   // CRC High
    // Serial.println("PRESSURE");
    modbusRequest[0] = 0X03;  // SLAVE-ID
    modbusRequest[1] = 0X03;  // FUN-CODE
    modbusRequest[2] = 0X00;  // START-ADD-HIGH
    modbusRequest[3] = 0X01;  // START-ADD-LOW
    modbusRequest[4] = 0X00;  // NO-OF-REG-HIGH
    modbusRequest[5] = 0X01;  // NO-OF-REG-LOW
    modbusRequest[6] = 0XD4;
    modbusRequest[7] = 0X28;

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];

    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);
    // SensorSelect++;
  } 
      else if (SensorSelect == TURBIDITY) {
    modbusRequest[0] = 0X03;
    modbusRequest[1] = 0X03;
    modbusRequest[2] = 0X00;
    modbusRequest[3] = 0X03;
    modbusRequest[4] = 0X00;
    modbusRequest[5] = 0X01;
    modbusRequest[6] = 0X75;
    modbusRequest[7] = 0XE8;

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];

    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);
  }
  else if (SensorSelect == SW1_mod) {

    // 0x02,  // Slave Address
    // 0x03,  // Function Code
    // 0x00,  // Start Address High
    // 0x03,  // Start Address Low
    // 0x00,  // Quantity High
    // 0x02,  // Quantity Low
    // 0x34,  // CRC Low
    // 0x38   // CRC High
    // Serial.println("PH");

    modbusRequest[0] = 0X03;
    modbusRequest[1] = 0X03;
    modbusRequest[2] = 0X00;
    modbusRequest[3] = 0X05;
    modbusRequest[4] = 0X00;
    modbusRequest[5] = 0X01;
    modbusRequest[6] = 0X95;
    modbusRequest[7] = 0XE9;

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];

    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);

    // SensorSelect++;
  } 
    else if (SensorSelect == SW2_mod) {

    // 0x02,  // Slave Address
    // 0x03,  // Function Code
    // 0x00,  // Start Address High
    // 0x03,  // Start Address Low
    // 0x00,  // Quantity High
    // 0x02,  // Quantity Low
    // 0x34,  // CRC Low
    // 0x38   // CRC High
    // Serial.println("PH");

    modbusRequest[0] = 0X03;
    modbusRequest[1] = 0X03;
    modbusRequest[2] = 0X00;
    modbusRequest[3] = 0X06;
    modbusRequest[4] = 0X00;
    modbusRequest[5] = 0X01;
    modbusRequest[6] = 0X65;
    modbusRequest[7] = 0XE9;

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];

    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);

    // SensorSelect++;
  } 
    else if (SensorSelect == WEIGHT_SENSOR) {


    modbusRequest[0] = 0X04;
    modbusRequest[1] = 0X03;
    modbusRequest[2] = 0X00;
    modbusRequest[3] = 0X03;
    modbusRequest[4] = 0X00;
    modbusRequest[5] = 0X01;
    modbusRequest[6] = 0X74;
    modbusRequest[7] = 0X5F;

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];

    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);
  }
  else if (SensorSelect == PH) {

    // 0x02,  // Slave Address
    // 0x03,  // Function Code
    // 0x00,  // Start Address High
    // 0x03,  // Start Address Low
    // 0x00,  // Quantity High
    // 0x02,  // Quantity Low
    // 0x34,  // CRC Low
    // 0x38   // CRC High
    // Serial.println("PH");

    modbusRequest[0] = 0X04; 
    modbusRequest[1] = 0X03;
    modbusRequest[2] = 0X00;
    modbusRequest[3] = 0X04;
    modbusRequest[4] = 0X00;
    modbusRequest[5] = 0X01;
    modbusRequest[6] = 0XC5;
    modbusRequest[7] = 0X9E;

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];

    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);

    // SensorSelect++;
  } 
  else if (SensorSelect == TDS) {

    // 0x04,  // Slave Address
    // 0x03,  // Function Code
    // 0x00,  // Start Address High
    // 0X05,  // Start Address Low
    // 0x00,  // Quantity High
    // 0x02,  // Quantity Low
    // 0XD4,  // CRC Low
    // 0X5F   // CRC High

    // Serial.println("TDS");
    modbusRequest[0] = 0X05;
    modbusRequest[1] = 0X03;
    modbusRequest[2] = 0X00;
    modbusRequest[3] = 0X01;
    modbusRequest[4] = 0X00;
    modbusRequest[5] = 0X01;
    modbusRequest[6] = 0XD4;
    modbusRequest[7] = 0X4E;

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];

    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);

    // SensorSelect = 0;
  } 
  else if (SensorSelect == TPW) {

    // 0x04,  // Slave Address
    // 0x03,  // Function Code
    // 0x00,  // Start Address High
    // 0X05,  // Start Address Low
    // 0x00,  // Quantity High
    // 0x02,  // Quantity Low
    // 0XD4,  // CRC Low
    // 0X5F   // CRC High

    // Serial.println("TPW");
    modbusRequest[0] = 0X05;
    modbusRequest[1] = 0X03;
    modbusRequest[2] = 0X00;
    modbusRequest[3] = 0X02;
    modbusRequest[4] = 0X00;
    modbusRequest[5] = 0X01;
    modbusRequest[6] = 0X24;
    modbusRequest[7] = 0X4E;

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];

    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);

    // SensorSelect = 0;
  }
  else if (SensorSelect == CHLORINE_PC) {

    // 0x04,  // Slave Address
    // 0x03,  // Function Code
    // 0x00,  // Start Address High
    // 0X05,  // Start Address Low
    // 0x00,  // Quantity High
    // 0x02,  // Quantity Low
    // 0XD4,  // CRC Low
    // 0X5F   // CRC High

    // Serial.println("CHLORINE");
    modbusRequest[0] = 0X0C;  // SLAVE-ID
    modbusRequest[1] = 0X04;  // FUN-CODE
    modbusRequest[2] = 0X00;  // START-ADD-HIGH
    modbusRequest[3] = 0X00;  // START-ADD-LOW
    modbusRequest[4] = 0X00;  // NO-OF-REG-HIGH
    modbusRequest[5] = 0X02;  // NO-OF-REG-LOW
    modbusRequest[6] = 0X70;
    modbusRequest[7] = 0XD6;
    
    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];
    
    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);
    // SensorSelect = 0;
  } 
  else if (SensorSelect == CHLORINE_NC) {

    // 0x04,  // Slave Address
    // 0x03,  // Function Code
    // 0x00,  // Start Address High
    // 0X05,  // Start Address Low
    // 0x00,  // Quantity High
    // 0x02,  // Quantity Low
    // 0XD4,  // CRC Low
    // 0X5F   // CRC High

    // Serial.println("CHLORINE");
    modbusRequest[0] = 0X06;
    modbusRequest[1] = 0X03;
    modbusRequest[2] = 0X00;
    modbusRequest[3] = 0X00;
    modbusRequest[4] = 0X00;
    modbusRequest[5] = 0X02;
    modbusRequest[6] = 0XC5;
    modbusRequest[7] = 0XBC;
    
    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];
    
    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);
    // SensorSelect = 0;
  } 
//   else if (SensorSelect == UFM_Test) {



   
//     modbusRequest[0] = 0X01;  // SLAVE-ID
//     modbusRequest[1] = 0X03;  // FUN-CODE
//     modbusRequest[2] = 0X01;  // START-ADD-HIGH
//     modbusRequest[3] = 0X69;  // START-ADD-LOW
//     modbusRequest[4] = 0X00;  // NO-OF-REG-HIGH
//     modbusRequest[5] = 0X02;  // NO-OF-REG-LOW
//     modbusRequest[6] = 0X15;
//     modbusRequest[7] = 0XEB;



//      uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];

//     // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
//      len_of_response_num = (5 + num_registers * 2);
//     //  ESP_LOGW("Mod_LEN", "Expected Response Length: %d\n",len_of_response_num);
// }

  else if (SensorSelect == UFM1_FLOW) {
    /*  Adrs | num of reg | data type |      Content      |      Unit       */
    /*  1447 |      2     | IEEE754   | Instantaneousflow | Unit:cubicmeter */

    modbusRequest[0] = 0X01;  // SLAVE-ID
    modbusRequest[1] = 0X03;  // FUN-CODE
    modbusRequest[2] = 0X05;  // START-ADD-HIGH
    modbusRequest[3] = 0XA7;  // START-ADD-LOW
    modbusRequest[4] = 0X00;  // NO-OF-REG-HIGH
    modbusRequest[5] = 0X02;  // NO-OF-REG-LOW
    modbusRequest[6] = 0X75;
    modbusRequest[7] = 0X24;

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];

    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);
    
  }
  
  else if (SensorSelect == UFM2_FLOW) {

    //1445
    modbusRequest[0] = 0X02;  // SLAVE-ID
    modbusRequest[1] = 0X03;  // FUN-CODE
    modbusRequest[2] = 0X05;  // START-ADD-HIGH
    modbusRequest[3] = 0XA7;  // START-ADD-LOW
    modbusRequest[4] = 0X00;  // NO-OF-REG-HIGH
    modbusRequest[5] = 0X02;  // NO-OF-REG-LOW
    modbusRequest[6] = 0X75;
    modbusRequest[7] = 0X17;

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];

    // Response: Slave ID (1) + Function Code (1) + Byte Count (1) + N*2 bytes + CRC (2)
     len_of_response_num = (5 + num_registers * 2);

  }
  else if (SensorSelect == UFM1_VOLUME) {


    // Reading Positive Cumulative Flow (Address 1464 -> 0x05B7) read 1463 in modbus
    // Sending exactly: 01 03 05 B7 00 02 74 E1
    modbusRequest[0] = 0X01;  // SLAVE-ID
    modbusRequest[1] = 0X03;  // FUN-CODE
    modbusRequest[2] = 0X05;  // START-ADD-HIGH
    modbusRequest[3] = 0XB7;  // START-ADD-LOW  (0xB7)
    modbusRequest[4] = 0X00;  // NO-OF-REG-HIGH
    modbusRequest[5] = 0X02;  // NO-OF-REG-LOW  (2 registers)
    modbusRequest[6] = 0X74;  // CRC LOW
    modbusRequest[7] = 0XE1;  // CRC HIGH

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];
    len_of_response_num = (5 + num_registers * 2);
   }
  
  else if (SensorSelect == UFM2_VOLUME) {

    // Reading Positive Cumulative Flow for Slave 2
    modbusRequest[0] = 0X02;  // SLAVE-ID
    modbusRequest[1] = 0X03;  // FUN-CODE
    modbusRequest[2] = 0X05;  // START-ADD-HIGH
    modbusRequest[3] = 0XB7;  // START-ADD-LOW  
    modbusRequest[4] = 0X00;  // NO-OF-REG-HIGH
    modbusRequest[5] = 0X02;  // NO-OF-REG-LOW  
    modbusRequest[6] = 0X74;  // CRC LOW
    modbusRequest[7] = 0XD2;  // CRC HIGH

    uint16_t num_registers = (modbusRequest[4] << 8) | modbusRequest[5];
    len_of_response_num = (5 + num_registers * 2);

  }


  else
  {
    modbusRequest[0] = 0;
    modbusRequest[1] = 0; 
    modbusRequest[2] = 0; 
    modbusRequest[3] = 0;  
    modbusRequest[4] = 0;  
    modbusRequest[5] = 0; 
    modbusRequest[6] = 0;
    modbusRequest[7] = 0;

    len_of_response_num = 0;
  }
}



uint8_t MODBUS_Sensor_Count()
{
    return SENSOR_TYPE_COUNT;
}


uint8_t len_of_modbus_response()
{
    return len_of_response_num;
}



// Standard MODBUS RTU CRC16
uint16_t modbus_crc(uint8_t *data, uint8_t len) {
    uint16_t crc = 0xFFFF;

    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else         crc >>= 1;
        }
    }
    return crc;
}



float decodeModbusResponse_TDS(uint8_t *receivedData)
{
  uint16_t data = (receivedData[3] << 8) | receivedData[4];
  float tds = (float) data * 0.7;
  return tds;
}



float  decodeModbusResponse_Chl(uint8_t *receivedData)
{
    unsigned int rawData = (receivedData[3] << 8) | receivedData[4];  // Combine bytes
    unsigned int decimalPoints = receivedData[5];  // Number of decimal places

    // Convert raw data to floating-point value
    float sensorValue = rawData / pow(10, decimalPoints);  // Apply decimal shift

    // Assign to global variable
    return sensorValue;
}

float decodeModbusResponse_UFM( uint8_t *floatBytes)
{
	 union {
        uint8_t b[4];
        float f;
    } data_f;

    // Copy bytes in correct order for your platform (little-endian)
    data_f.b[0] = floatBytes[6];
    data_f.b[1] = floatBytes[5];
    data_f.b[2] = floatBytes[4];
    data_f.b[3] = floatBytes[3];

	return  data_f.f;
}


// float decodeModbusResponse_Cumulative(uint8_t *receivedData)
// {
//     // Register 1443 & 1444: Long (32-bit signed integer) N
//     // Modbus is typically Big-Endian for Integers. 
//     // receivedData[3] = 1443 High
//     // receivedData[4] = 1443 Low
//     // receivedData[5] = 1444 High
//     // receivedData[6] = 1444 Low
    
//     int32_t N = (int32_t)( (receivedData[3] << 24) | (receivedData[4] << 16) | (receivedData[5] << 8) | receivedData[6] );

//     // Register 1445: Integer (16-bit signed) m
//     // receivedData[7] = 1445 High
//     // receivedData[8] = 1445 Low
    
//     int16_t m = (int16_t)( (receivedData[7] << 8) | receivedData[8] );

//     // Formula from Manual Note 5: Value = N * 10^(m-3)
//     float result = (float)N * pow(10, m - 3);

//     return result;
// }

float decodeModbusResponse_Cumulative(uint8_t *receivedData)
{
    // RX Frame Example: 01 03 04 [00 04] [00 00] [CRC] [CRC]
    // receivedData[3] & [4] = Low Word (e.g., 00 04)
    // receivedData[5] & [6] = High Word (e.g., 00 00)

    uint16_t low_word = (receivedData[3] << 8) | receivedData[4];
    uint16_t high_word = (receivedData[5] << 8) | receivedData[6];

    // Combine into a 32-bit signed integer (Little-Endian order)
    int32_t N = (int32_t)((high_word << 16) | low_word);

    // Apply the 0.1 multiplier (since exponent m = 2)
    float result = (float)N * 0.1f;

    return result;
}