
#ifndef MODBUS_HANDLER
#define MODBUS_HANDLER

typedef enum {
    SENSOR_TYPE_TDS,
    SENSOR_TYPE_PH,
    SENSOR_TYPE_TEMP,
    SENSOR_TYPE_CHL,
    SENSOR_TYPE_UFM,
    // Add more types as needed
} SensorType;



typedef struct {
    uint16_t tds_ch1;
    uint16_t temp_ch1;
    uint16_t tds_ch2;
    uint16_t temp_ch2;
} TdsData;


typedef struct {
    float ph_ch1;
} PhData;


typedef struct {
    float temperature;
    float humidity;
} TempData;


typedef struct {
    float chlorine;
} ChlData;


typedef union {
    TdsData tds;
    PhData ph;
    TempData temp;
    ChlData chl;
    // Extend as needed
} SensorData;


typedef struct {
    uint8_t      slave_id;
    SensorType   type;
    SensorData   data;
    uint8_t      num_regs;    // ← total number of uint16_t registers
} Sensor;



extern Sensor MODBUS_sensors[];  

// #define NUM_SENSORS 5

void poll_all_sensors();
void decode_sensor_data(Sensor* sensor, const uint8_t* data);


uint16_t build_read_request(uint8_t *buffer, uint8_t slave_id, uint16_t reg_addr, uint16_t num_regs);

uint16_t build_write_request(uint8_t *buffer, uint8_t slave_id, uint16_t reg_addr, uint16_t value);
uint16_t modbus_crc16(uint8_t *data, uint16_t length) ;
size_t get_num_sensors(void) ;

#endif 