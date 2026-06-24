
#ifndef SENSOR_DATA
#define SENSOR_DATA

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

/* Sensor Data */



#define MAX_SENSOR_NAME_LEN 24 //Updated from 12 to 24
#define MAX_JSON_SIZE 512

#define SENSOR_NAMESPACE "sensor_config"
#define SENSOR_KEY       "sensors"
#define MAX_SENSORS      17

#define SENSOR_CONFIG_VERSION  2
#define SENSOR_VER_NAMESPACE  "sens_meta"   // separate from "sensor_config"
#define SENSOR_VER_KEY        "sens_ver"

extern int sensor_count;
static const char *TAG_NVS = "NVS";

void init_default_sensors();
esp_err_t save_sensors_to_nvs();
esp_err_t load_sensors_from_nvs();
esp_err_t clear_sensor_nvs_namespace(void);
void print_sensor_data(void) ;
int get_sensor_count(void);
esp_err_t load_sensors_with_version_check(void);

//esp_err_t load_sensors_from_nvs(void);

typedef enum {
  PRESSURE = 0,
  PH,
  TDS,
  TPW ,
  CHLORINE_PC,
  CHLORINE_NC,
  WEIGHT_SENSOR,
  TURBIDITY,
  SW1_mod,
  SW2_mod,
//   UFM_Test,
  UFM1_FLOW,
  UFM1_VOLUME,
  UFM2_FLOW,
  UFM2_VOLUME,
  SENSOR_TYPE_COUNT
} modbus_sensor_e;

typedef enum{
    TPA= SENSOR_TYPE_COUNT,
    FLW,
    SW1ob, // onboard switch 1
    SW2ob, // onboard switch 2 BLE
} reg_sensor_e;

typedef struct {
    char name[24];         // Human-readable name (e.g., "pH") // Increased from 16 to 24   
    char key[8];           // BLE/JSON key (e.g., "PH")
    bool is_enabled;       // Whether it's selected/enabled
    bool is_rs485;         // True if it's RS485-based
    float offset;          // Calibration offset
    float scale;           // Calibration scale
    float value;           // Last measured value
    bool response_ok;      // Last sensor response status
} sensor_config_t;

extern sensor_config_t sensors[MAX_SENSORS];  // Declare externally if used in multiple files

// Declare the sensor values (defined elsewhere)
extern float pressureVal, phVal, ufFlowVal1, ufVolumeVal1, tdsVal, chlorine_PCVal, chlorine_NCVal, SW1_mod_val, SW2_mod_val,  ufFlowVal2, ufVolumeVal2, weight_val, turbidityVal;
extern float tempWVal, tempAVal, FlowVal, Sw1Val, Sw2Val, tempVal; 


// extern float pressureVal;
// extern float phVal;
// extern float tdsVal;
// extern float chlorineVal;
// extern float ufm1FlowVal;
// extern float ufm1VolVal;
// extern float ufm2FlowVal;
// extern float ufm2VolVal;




// Struct definition
typedef struct {
    int modbus_index;
    int sensor_index;
    float *value_ptr;
} sensor_map_t;

/* Defined in modbus_esp.c file */
extern sensor_map_t modbus_map[];
extern const size_t modbus_map_size;

extern sensor_map_t sensor_map[];
extern const size_t sensor_map_size;


// typedef enum {
//   PRESSURE = 0,
//   PH,
//   TDS,
//   TEMPW,
//   CHLORINE,
// //   WEIGHT,
// //   TURBIDITY,
//   UFM1_FLOW,
//   UFM1_VOLUME,
//   UFM2_FLOW,
//   UFM2_VOLUME,
//   MODBUS_COUNT,

//   TEMP_A,
//   FLOW,
//   SW1,
//   SW2,
//   SENSOR_LIST_COUNT
// } SENSOR_LIST;


/* Device Configuration Data */

typedef struct {
    char device_type[16];      // e.g., "RMS or ACOM"  -- Defined in support.h
    char IMEI_num[32];
    char server_name[32];
    char firmware_version[32];  // Optional, or hardcoded
    uint32_t data_frequency_sec;
    char date_format[24];   // e.g., "YYYY-MM-DD HH:mm:ss"
    char time_format[24]; 
    double latitude;
    double longitude;

    bool wifi_enabled_gsm_disabled;
    char wifi_ssid[32];
    char wifi_password[62];

    bool gpio_extender_enabled;
    bool gps_enabled;
    bool gsm_enabled;
	
    char gsm_sim_name[16]; // e.g., "SIM1", "JIO", etc.
} DeviceConfig;

extern DeviceConfig device_config;

#define DEVICE_STATUS_NAMESPACE "device_status"
#define DEVICE_CONFIG_NAMESPACE "device_config"
#define DEVICE_STATUS_KEY       "status"
#define DEVICE_CONFIG_KEY       "config"


/* Device Status */
typedef struct {
    char current_date[16];        // "2025-07-15"
    char current_time[16];        // "14:23:58"

    int gsm_signal_strength;      // dBm, e.g., -85
    char gsm_post_status[12];     // "OK", "RETRY", "FAIL"

    float battery_level;          // percentage, e.g., 82.5
    float solar_level;            // percentage, e.g., 76.3

    uint32_t log_count;           // Number of logs since last cleared
} DeviceStatus;



esp_err_t save_device_status_to_nvs(const DeviceStatus *status);
esp_err_t load_device_status_from_nvs(DeviceStatus *status);

esp_err_t save_device_config_to_nvs(const DeviceConfig *config);
esp_err_t load_device_config_from_nvs(DeviceConfig *config);

esp_err_t clear_device_status_nvs(void);
esp_err_t clear_device_config_nvs(void);

void print_device_status(const DeviceStatus *status);
void print_device_config(const DeviceConfig *config);

void init_default_device_status(DeviceStatus *status);
void init_default_device_config(DeviceConfig *config);
void process_sensor_config_data(const char *data);
void process_device_config_data(const char *input, DeviceConfig *device_config);




#endif
