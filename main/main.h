#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "freertos/timers.h"

// // Define all the pinouts

// #define IO_RS485_en   0    // Q0
// #define IO_12V  1         // Q1
// #define IO_7V 2          // Q2
// #define SEN_RS485_en 3    // Q3
// #define IO_RS485_REDE 4  // Q4
// #define SEN_RS485_REDE 5  // Q5
// #define GSM_en 6            // Q6
// #define LED 7               // Q7


/* GPIO Definition */
#define FLOW_SENSOR 25
#define on_board_sw1  36
#define on_board_sw2 39

#define SW1 34
#define SW2 35

#define ADC_WIDTH_BIT    ADC_WIDTH_BIT_12
#define ADC_ATTEN_DB     ADC_ATTEN_DB_12
#define ADC_VREF         1100 // mV

#define ADC_CH_1         ADC2_CHANNEL_5 // GPIO12  - VBAT_SENS
#define ADC_CH_2         ADC2_CHANNEL_6 // GPIO14  - SOLAR_SENS

#define BUZZER_GPIO 2
// ... Add more as per your wiring

// #define HIGH 1
// #define LOW  0

// #define DATA_PIN   26  // DS
// #define LATCH_PIN  32  // STCP
// #define CLOCK_PIN  27  // SHCP

// uint8_t shift_reg_state = 0;  // Holds current 8-bit register state
bool ble_interrupted = false;
static uint32_t last_sent_config_crc = 0;   /* guards against echo-back loop */
char time_app_hms[16];

volatile uint32_t flow_pulse_count = 0;
portMUX_TYPE flowMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t flow_task_handle = NULL;
TaskHandle_t tempr_task_handle = NULL;
TaskHandle_t gsm_task_handle = NULL;
TaskHandle_t adc_sampling_handle = NULL;

QueueHandle_t BLE_INT_QUEUE;         // for "interrupt request"
SemaphoreHandle_t BLE_INT_POLL_PAUSED;      // to block/resume POLL
SemaphoreHandle_t BLE_INT_BLE_DONE;      // to block/resume POLL

float flow_rate_lpm = 0;
bool flow_counting = false;

float temperature = 0;
bool temp_checking = false;

double longitude, latitude;

bool gsm_cmd_checking = false;
bool sensor_data_colleccted = false;

static float sampled_voltage1 = 0.0f;
static float sampled_voltage2 = 0.0f;
static bool adc_sampling = false;

time_t unix_timestamp;
bool prev_data_post_fail = false;

bool check_GPS_data = true;
bool gsm_requires_init = true;
void update_and_print_time();

volatile bool ble_cmd_processing = false;

#pragma once
#include "esp_log.h"

// Extra ANSI colors (256-color & RGB supported too)
#define LOG_COLOR_ORANGE   "\x1b[38;5;208m"
#define LOG_COLOR_PINK     "\x1b[38;5;213m"
#define LOG_COLOR_CYAN     "\x1b[36m"
#define LOG_COLOR_PURPLE   "\x1b[35m"
#define LOG_COLOR_BLUE     "\x1b[34m"
#define LOG_COLOR_RESET    "\x1b[0m"

#define ESP_LOGO(tag, fmt, ...)  ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO, tag, LOG_COLOR_ORANGE fmt LOG_COLOR_RESET, ##__VA_ARGS__)
#define ESP_LOGP(tag, fmt, ...)  ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO, tag, LOG_COLOR_PINK fmt LOG_COLOR_RESET, ##__VA_ARGS__)
#define ESP_LOGC(tag, fmt, ...)  ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO, tag, LOG_COLOR_CYAN fmt LOG_COLOR_RESET, ##__VA_ARGS__)
#define ESP_LOGU(tag, fmt, ...)  ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO, tag, LOG_COLOR_PURPLE fmt LOG_COLOR_RESET, ##__VA_ARGS__)
#define ESP_LOGB(tag, fmt, ...)  ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO, tag, LOG_COLOR_BLUE fmt LOG_COLOR_RESET, ##__VA_ARGS__)



#endif