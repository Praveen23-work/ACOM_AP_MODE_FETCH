#ifndef OTA_HANDLER
#define OTA_HANDLER

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#define CHUNK_SIZE 1024

static const int RX_BUF_SIZE;
extern const char* new_fw_ver;
extern bool OTA_active;
extern size_t total_ota_received;   // Track total OTA bytes
extern volatile bool ota_rx_success ;
extern QueueHandle_t ota_queue;
extern uint32_t expected_ota_file_size; // Set this when starting OTA

typedef enum
{
    FTP_FETCH_FAILED,
    FTP_FILE_NOT_FOUND,
    FTP_FETCH_OK,              // Base FTP connection/file found OK
    OTA_TRANSFER_COMPLETE,     // New state: File completely received by UART task
    OTA_VALIDATE_SUCCESS,      // New state: Image written AND verified OK
    OTA_VALIDATE_FAILED,       // New state: Image written but validation FAILED
} ota_result_t; // Renamed to reflect final OTA status

typedef struct {
    uint8_t data[CHUNK_SIZE];  // OTA chunk
    size_t len;                // Number of valid bytes in this chunk
} ota_chunk_t;

//UART 2 - GSM MODEM
#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)
#define UART_PORT UART_NUM_2



//APIs
void UART_INIT();
void DELETE_UART_TASK();
extern int find_subsequence(const uint8_t *data, size_t size, const uint8_t *seq, size_t seq_len);
void check_fw_update();
esp_err_t get_ota_checked_status(int32_t *status);
esp_err_t set_ota_checked_status(int32_t status);
#endif



/*
====================================================================
FTP SERVER FILE STRUCTURE
====================================================================

FTP ROOT FOLDER
│
├── ConfigFiles/                // Contains per-device configuration
│   ├── ACOM_D1.json
│   ├── ACOM_D2.json
│   ├── ACOM_D3.json
│   └── ACOM_Dx.json            // One JSON file per device
│
└── FirmwareFiles/              // Contains firmware binaries
    ├── FR_1.0.bin
    ├── FR_1.5.bin
    ├── FR_2.0.bin
    └── FR_x.x.bin              // Firmware versions


====================================================================
CONFIG FILE JSON FORMAT (example: ACOM_D1.json)
====================================================================

{
    "DEVICE_ID": "ACOM_D1",
    "SITE_ID": "NOIDA",
    "FIRMWARE_VERSION": "FR_1.1.bin"
}

--------------------------------------------------------------------
Notes:
- DEVICE_ID    → Unique device identifier (matches filename)
- SERVER_ID    → Deployment / location name
- FIRMWARE_VERSION → Minimum required firmware version for device
====================================================================
*/
