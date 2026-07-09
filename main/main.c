// tds scaling factor adjusted to 0.5 from 0.7 as per request on 30 june 2026 from Rohit

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_spiffs.h"
#include "esp_console.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "BLE.h"
#include "sensor_config.h"
#include "logs_handler.h"
#include "UART_HANDLER.h"
// #include "MODBUS_HANDLER.h"
#include "modbus_esp.h"
#include "RTC_handler.h"
#include "ds18x20_handler.h"
#include "GSM_handler.h"
#include "ota_handler.h"
#include "main.h"
#include "support.h"
#include "wifi_handler.h"
#include "wifi_ap_handler.h"

#include <time.h>

#include "driver/periph_ctrl.h"
#include "esp_private/periph_ctrl.h"
#include "soc/soc.h"
#include <sys/time.h>   // for settimeofday()
/*	Select anyone of the following */
/*	DEFAULT    LOG_AT_UART1		LOG_AT_UART2		DISABLE_LOGS */

#define DEBUG LOG_AT_UART2

/*								  */

/*
#include "uart.h"
#include "config_store.h"
#include "log_manager.h"
#include "gpio_manager.h"
#include "gsm_handler.h"
#include "rtc_sync.h"
*/

// 5 hours * 3600s + 30 minutes * 60s = 19800 seconds
#define IST_OFFSET_SEC (19800LL)
/*
SPIFFS Size
Flash end = 0x400000

Storage start = 0x110000

Size = 0x400000 – 0x110000 = 0x2F0000 (~2.94 MB).
*/



char all_sensor_info[4096] = {0}; // Adjust size as needed
char device_config_str[512] = {0}; // Adjust size as needed
char device_status_str[512] = {0};  // Declare this globally or adjust as needed

void initiate_app_sequence();
void process_rxd_ble_data(const char *data); 

void get_sensor_data();
void get_device_config();
void get_device_status();
void get_all_configs();

static DeviceStatus status;
static DeviceConfig config;

#define MIN_SPACE_FOR_LOG 300 // Minimum space required for a log entry


void write_SR_all(uint8_t value);
void shiftOut(uint8_t val);

void sensor_modbus_requests();
void collect_and_log(void);
void post_data(void);
void backlog_post_task(void *pvParameters);
static void sync_log_schedule_to_rtc(uint32_t interval_sec);
static time_t get_next_scheduled_log_ts(uint32_t interval_sec);

void process_data_modbus(void);
esp_err_t gsm_command_exe();
void time_sync(const char *str);
void send_all_logs();


float SOLARPercentage(float voltage);
float BatteryPercentage(float voltage);

void buzzer_beep(uint32_t on_ms, uint32_t off_ms, uint32_t repeat);

bool is_bat_15V = true;
static time_t scheduled_log_unix = 0;
static uint32_t scheduled_interval_sec = 0;
static TaskHandle_t backlog_post_task_handle = NULL;
static uint8_t backlog_post_fail_streak = 0;

static int64_t last_rtc_sync_unix = 0;

/* ── BLE fast-log mode ──────────────────────────────────────────────────────
 * When BLE is connected we log every BLE_LOG_INTERVAL_SEC seconds using the
 * ACTUAL RTC time (not the scheduler counter).  The normal 60-second scheduler
 * is frozen while BLE is active so it resumes exactly where it left off once
 * BLE disconnects.  We do NOT advance scheduled_log_unix during BLE mode.
 * ─────────────────────────────────────────────────────────────────────────── */
#define BLE_LOG_INTERVAL_SEC   10      /* log & post every 10 s while BLE on  */
#define BLE_LOG_INTERVAL_MS    (BLE_LOG_INTERVAL_SEC * 1000ULL)
static bool     ble_mode_active        = false;  /* true while BLE connected  */
static uint64_t ble_last_log_ms        = 0;      /* millis() of last BLE log  */
static uint32_t normal_elapsed_saved   = 0;      /* elapsed already consumed before BLE took over */
static time_t get_realtime_rtc_ts(void);

bool BLE_INTERRUPT = false;
SemaphoreHandle_t BLE_INTERRUPTxMutex;

void print_binary(uint8_t value) {
    printf("Binary: ");
    for (int i = 7; i >= 0; i--) {
        printf("%d", (value >> i) & 1);
    }
    printf("\n");
}

void init_shift_register()
{
    gpio_set_direction(DATA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LATCH_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(CLOCK_PIN, GPIO_MODE_OUTPUT);

    // Initialize with all LOW
    write_SR_all(shift_reg_state);
}


// void write_SR_all(uint8_t value)
// {
// 	shift_reg_state = shift_reg_state | value;
//     // gpio_set_level(LATCH_PIN, 0);  // Begin
//     // for (int i = 7; i >= 0; i--) {
//     //     gpio_set_level(CLOCK_PIN, 0);
//     //     gpio_set_level(DATA_PIN, (value >> i) & 0x01);
//     //     gpio_set_level(CLOCK_PIN, 1);
//     // }
//     // gpio_set_level(LATCH_PIN, 1);  // Latch output
// 	shiftOut(shift_reg_state);
// }

// void write_SR(uint8_t pin_bit, uint8_t level)
// {
//     if (level == HIGH) {
//         shift_reg_state |= (1 << pin_bit);
//     } else {
//         shift_reg_state &= ~(1 << pin_bit);
//     }
//     // write_SR_all(shift_reg_state);
// 	shiftOut(shift_reg_state);
// 	// print_binary(shift_reg_state);
// }



char *fake_data_logs[] = {
    "{\"DT\":1759061732,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":90.5,\"Sol\":85.0,\"UFM1F\":1.2,\"UFM1V\":3.3,\"UFM2F\":1.5,\"UFM2V\":3.4,\"SW1\":1.0,\"SW2\":0.0}",
    "{\"DT\":1759061832,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":89.0,\"Sol\":86.5,\"UFM1F\":1.3,\"UFM1V\":3.4,\"UFM2F\":1.6,\"UFM2V\":3.5,\"SW1\":1.0,\"SW2\":0.0}",
    "{\"DT\":1759061932,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":87.0,\"Sol\":84.0,\"UFM1F\":1.4,\"UFM1V\":3.3,\"UFM2F\":1.7,\"UFM2V\":3.6,\"SW1\":0.0,\"SW2\":1.0}",
    "{\"DT\":1759062032,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":85.5,\"Sol\":83.0,\"UFM1F\":1.5,\"UFM1V\":3.2,\"UFM2F\":1.8,\"UFM2V\":3.7,\"SW1\":1.0,\"SW2\":1.0}",
    "{\"DT\":1759062132,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":84.0,\"Sol\":82.5,\"UFM1F\":1.6,\"UFM1V\":3.4,\"UFM2F\":1.9,\"UFM2V\":3.8,\"SW1\":0.0,\"SW2\":0.0}",
    "{\"DT\":1759062232,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":82.5,\"Sol\":81.0,\"UFM1F\":1.7,\"UFM1V\":3.5,\"UFM2F\":2.0,\"UFM2V\":3.9,\"SW1\":1.0,\"SW2\":0.0}",
    "{\"DT\":1759062332,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":81.0,\"Sol\":80.5,\"UFM1F\":1.8,\"UFM1V\":3.6,\"UFM2F\":2.1,\"UFM2V\":4.0,\"SW1\":1.0,\"SW2\":0.0}",
    "{\"DT\":1759062432,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":79.5,\"Sol\":80.0,\"UFM1F\":1.9,\"UFM1V\":3.7,\"UFM2F\":2.2,\"UFM2V\":4.1,\"SW1\":0.0,\"SW2\":1.0}",
    "{\"DT\":1759062532,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":78.0,\"Sol\":79.0,\"UFM1F\":2.0,\"UFM1V\":3.8,\"UFM2F\":2.3,\"UFM2V\":4.2,\"SW1\":1.0,\"SW2\":1.0}",
    "{\"DT\":1759062632,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":76.5,\"Sol\":78.0,\"UFM1F\":2.1,\"UFM1V\":3.9,\"UFM2F\":2.4,\"UFM2V\":4.3,\"SW1\":0.0,\"SW2\":0.0}",
    "{\"DT\":1759062732,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":75.0,\"Sol\":77.5,\"UFM1F\":2.2,\"UFM1V\":4.0,\"UFM2F\":2.5,\"UFM2V\":4.4,\"SW1\":1.0,\"SW2\":0.0}",
    "{\"DT\":1759062832,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":73.5,\"Sol\":76.0,\"UFM1F\":2.3,\"UFM1V\":4.1,\"UFM2F\":2.6,\"UFM2V\":4.5,\"SW1\":1.0,\"SW2\":0.0}",
    "{\"DT\":1759062932,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":72.0,\"Sol\":75.0,\"UFM1F\":2.4,\"UFM1V\":4.2,\"UFM2F\":2.7,\"UFM2V\":4.6,\"SW1\":0.0,\"SW2\":1.0}",
    "{\"DT\":1759063032,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":70.5,\"Sol\":74.0,\"UFM1F\":2.5,\"UFM1V\":4.3,\"UFM2F\":2.8,\"UFM2V\":4.7,\"SW1\":1.0,\"SW2\":1.0}",
    "{\"DT\":1759063132,\"Device\":\"xyz\",\"IMEI\":\"866082073481955\",\"FR_v\":\"1.0.0\",\"Lat\":28.6139,\"Lon\":77.2090,\"Bat\":69.0,\"Sol\":73.0,\"UFM1F\":2.6,\"UFM1V\":4.4,\"UFM2F\":2.9,\"UFM2V\":4.8,\"SW1\":0.0,\"SW2\":0.0}"
};



void temp_device_status(DeviceStatus *status) 
{
    strcpy(status->current_date, "2023-12-10");
    strcpy(status->current_time, "01:10:12");
    status->gsm_signal_strength = 50;
    status->battery_level = 50.0f;
    status->solar_level = 50.0f;
    strcpy(status->gsm_post_status, "FAIL");
    status->log_count = 10;
}

void temp_device_config(DeviceConfig *config) {
    strcpy(config->IMEI_num, "888IMEI999");
    strcpy(config->server_name, "NODE_017");
    strcpy(config->firmware_version, "7.0.0");
    config->data_frequency_sec = 60;
    strcpy(config->date_format, "YYYY-DD-MM");
    strcpy(config->time_format, "HH:MM:SS");
    config->latitude = 0.30000;
    config->longitude = 0.60000;
    config->gpio_extender_enabled = true;
    strcpy(config->gsm_sim_name, "JIO");
}



void extra_func_change_data()
{
	temp_device_status(&status);
	save_device_status_to_nvs(&status);

	temp_device_config(&config);
	save_device_config_to_nvs(&config);
}



const char *sample_status_string =
    "----- Device Status ----- Date: 2025-07-16 Time: 20:32:10 GSM Signal Strength: -79 dBm"
    "GSM Post Status: SUCCESS"
    "Battery Level: 87.45%"
    "Solar Charging Level: 55.20%"
    "GPIO Extender Enabled: Yes"
    "Log Count Since Last Clear: 147";



void check_btn_press()
{
	if (!gpio_get_level(GPIO_NUM_0)) {
		ESP_LOGI("BOOT BUTTON:", "Button Pressed");
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		if (!gpio_get_level(GPIO_NUM_0)) {
			ESP_LOGI("check_btn_press", "Button Pressed for 3 seconds, Erasing logs");
			// erase_logs();
			// ESP_LOGI("BOOT BUTTON:", "Logs Erased");
			// ESP_LOGI("ERASE", "Erased, File Length: %d", get_file_size());
			// extra_func_change_data();
			// print_all_logs();

			// format_spiffs();
			// clear_sensor_nvs_namespace();
			// clear_device_status_nvs();
			// clear_device_config_nvs(); // CLEAR the sensor data if required.

			// ESP_LOGI("BOOT BUTTON", "Device configuration & SPIFF cleared");
			// int day = 25;
			// int mon = 8;
			// int year = 2025;
			// int weekday = 1; //Mon-1 | Tue-2 | Wed-3 | Thu-4 | Fri-5 | Sat-6 | Sun-7
			// int hour = 11; //24hour format
			// int min = 20;

			// set_rtc_time(day,weekday,mon,year,hour,min);
			// sensor_modbus_requests();
		}
	}
}

void btn_press_handler(void *param)
{
    int64_t ble_start_time = 0;
    volatile int64_t m = esp_timer_get_time();
    volatile int64_t x = esp_timer_get_time();
    volatile int64_t connected_time = esp_timer_get_time();
    int64_t last_ble_notify_time = esp_timer_get_time();
    bool last_clicked = false;
    static bool was_connected = false;

    // if( is_ble_active() == false)    initiate_app_sequence();
    last_clicked = true;
    int count = 1; // Counter for status notify chunks

    while (1)
    {    
		if (ap_mode_active) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if(!gpio_get_level(SW2))
        {
            int64_t z = esp_timer_get_time();
            if ((z - x) / 1000 >= 2000 )
            {
                ESP_LOGI("SW1 PRESSED", "TRYING TO START BLE");

                if( is_ble_active() == false){    
                    initiate_app_sequence();
                    ble_start_time = esp_timer_get_time();   // only stamp on fresh start
                }

                write_SR(LED, 1);
                buzzer_beep(500,250,2);

                last_clicked = true;
                }
        }
        else
        {
            x = esp_timer_get_time();
        }

        if (!gpio_get_level(GPIO_NUM_0))
        {
            int64_t n = esp_timer_get_time();
            if ((n - m) / 1000 >= 2000 )
            {
                ESP_LOGI("BOOT BUTTON:", "Button Pressed FOR 3 SECOND, Erasing logs\n");
                m = esp_timer_get_time();

                // format_spiffs();
                erase_logs();
                clear_posted_logs();
                erase_post_state_blob();
                clear_sensor_nvs_namespace();
                clear_device_status_nvs();
                clear_device_config_nvs(); // CLEAR the sensor data if required.

                ESP_LOGI("BOOT BUTTON", "Device configuration & SPIFF cleared");

            }
        }
        else
        {
            m = esp_timer_get_time();
        }
         vTaskDelay(10);

        if ( is_ble_connected_() )
        {    
            /* Instantly push cached data to the app upon connection (0-sec wait) */
            if (!was_connected) {
            was_connected = true;
            ESP_LOGI("BLE", "App connected! Sending initial cached data instantly.");
            get_all_configs();
            get_device_status();   // fresh RTC read — avoids sending stale scheduled DT
            status_notify_chunks(device_config_str);
            status_notify_chunks(all_sensor_info);
            status_notify_chunks(device_status_str);
            last_ble_notify_time = esp_timer_get_time();
        }

            int64_t blink_time = esp_timer_get_time();
            if ((blink_time - connected_time) / 1000 >= 500 )
            {
                write_SR(LED, !gpio_get_level(LED));
                connected_time = esp_timer_get_time();
            }

            if( last_clicked == true)
            {
                get_all_configs();
                // set_ble_data(all_sensor_info);

                last_clicked = false;
            }
            // Periodic 5-second BLE update for fast app refresh
            int64_t now_us = esp_timer_get_time();
			if ((now_us - last_ble_notify_time) / 1000 >= 5000 && sensor_data_colleccted) 
			{
				get_sensor_data();
				status_notify_chunks(all_sensor_info);
				get_device_status();
				status_notify_chunks(device_status_str);
				last_ble_notify_time = now_us;
			}
        }
        else
			{
				was_connected = false;
				if (ble_cmd_processing == true) {
					ble_cmd_processing = false;
				}

				// timeout check must be here — outside the ble_cmd_processing block
				if (ble_start_time > 0 && is_ble_active() &&
					(esp_timer_get_time() - ble_start_time) / 1000 >= (5 * 60 * 1000))
				{
					ESP_LOGW("BLE", "No connection for 5 min — shutting down BLE");
					write_SR(LED, 0);
					stop_ble();
					ble_start_time = 0;
				}
			}

        if( is_ble_rx_done() )            //&& !ble_cmd_processing)
        {	ESP_LOGE("BLE_RXDONE", "is_ble_rx_done() returned TRUE!");
            ESP_LOGE("BLE", "Received BLE data, processing...");

            int msg = 1;
            xQueueOverwrite(BLE_INT_QUEUE, &msg);  // request interrupt
            ESP_LOGB("BLE", "Interrupt request sent");

            // Wait until POLL confirms it has paused
            xSemaphoreTake(BLE_INT_POLL_PAUSED, portMAX_DELAY);
            ESP_LOGW("BLE", "POLL paused, starting urgent work");

            const char *rxd_buff = read_ble_rx_buffer();
            // printf("rxd_buff ptr address: %p\n", (void *)rxd_buff);

            if (rxd_buff == NULL) 
            {
                ESP_LOGW("BLE", "Received NULL data from BLE RX buffer");
                reset_ble_rx_buffer();
                // Handle NULL case here (return, skip, etc.)

                // Signal POLL that urgent work is finished
                xSemaphoreGive(BLE_INT_BLE_DONE);
                ESP_LOGI("BLE", "Urgent work done, POLL may resume");

            } else 
            {
                ESP_LOGI("DEBUG", "First 20 bytes:'%.*s'", 20, rxd_buff);
                // Process rxd_buff as usual
                process_rxd_ble_data(rxd_buff);
                // Signal POLL that urgent work is finished
                xSemaphoreGive(BLE_INT_BLE_DONE);
                ESP_LOGB("BLE", "Urgent work done, POLL may resume");
            }

        }
    }
}





void get_all_configs()
{
	get_sensor_data();
	get_device_config();
	get_device_status();
}

/* djb2 hash — fast, no dependencies, good enough for echo detection */
static uint32_t simple_crc(const char *s) {
    uint32_t h = 5381;
    while (*s) h = ((h << 5) + h) ^ (uint8_t)(*s++);
    return h;
}

void process_rxd_ble_data(const char *data)
{
	ESP_LOGI("BLE", "Processing received BLE data: %s", data);
	// const char *prefix = "{$sensor_config$";
	// printf("Data ptr address: %p\n", (void *)data);

	if (!data) {
    ESP_LOGW("DEBUG", "Data pointer is NULL");
	} else if (strlen(data) == 0) {
		ESP_LOGW("DEBUG", "Data is an empty string");
		// ESP_LOGI("DEBUG", "First 20 bytes:'%.*s'", 20, data);
	} else {
		ESP_LOGI("DEBUG", "First 20 bytes:'%.*s'", 20, data);
	}

    if (data && strcmp(data, "{$send_sensor_data}") == 0) 
	{
		printf("Received command to notify send_config1 data\n");
		get_sensor_data();
		status_notify_chunks(all_sensor_info);
	}

	else if (data && strcmp(data, "{$send_device_config}") == 0) 
	{
		printf("Received command to notify send_config2 data\n");
		get_device_config();
		status_notify_chunks(device_config_str);
	}

	else if (data && strcmp(data, "{$send_device_status}") == 0) 
	{
		printf("Received command to notify send_config3 data\n");
		get_device_status();
		status_notify_chunks(device_status_str);
	}

	else if( data && strcmp(data, "{$send_all_data_config}") == 0) 
	{	
		printf("Received command to notify send_all_data_config data\n");
		get_all_configs();
		status_notify_chunks(all_sensor_info);
		status_notify_chunks(device_status_str);
		status_notify_chunks(device_config_str);
	}

	else if (data && strncmp(data, "{$device_config$", 16) == 0)
	{	
		/* If this is the same payload we just notified to the app,
		* it is an echo-back — the app received our config push and
		* re-sent it unchanged.  Ignore it completely. */
		if (simple_crc(data) == last_sent_config_crc) {
			ESP_LOGW("BLE", "Ignoring config echo-back (CRC match)");
			reset_ble_rx_buffer();
			return;
		}

		process_device_config_data(data, &config);
		save_device_config_to_nvs(&config);
		load_device_config_from_nvs(&config);

		get_device_config();
		status_notify_chunks(device_config_str);
		ble_interrupted = true;
	}

	else if (data && strncmp(data, "{$sensor_config$", 16) == 0)
	{
		process_sensor_config_data(data);
		save_sensors_to_nvs();
		load_sensors_from_nvs();

		get_sensor_data();
		status_notify_chunks(all_sensor_info);
		ble_interrupted = true;
	}

	// else if( data && strncmp(data, "{$device_config$", 16) == 0)
	// {
	// 	process_device_config_data(data, &config);
	// }

	else if( data && strncmp(data, "{$send_logs}", 12) == 0)
	{
		send_all_logs();
		// for (int i = 0; i < (sizeof(fake_data_logs) / sizeof(fake_data_logs[0])); i++) 
		// // for (int i = 0; i < 2; i++) 
		// {
		// 	status_notify_chunks(fake_data_logs[i]);
		// 	// vTaskDelay(pdMS_TO_TICKS(100));  
		// }
	}
	else if( data && strncmp(data, "{$start_ap}", 11) == 0)
	{
		ESP_LOGW("BLE", "Start AP mode requested...");
		// --- ADDED VISIBLE PRINT STATEMENTS ---
		ESP_LOGE("BLE_TEST", "========================================");
		ESP_LOGE("BLE_TEST", "✅ COMMAND RECEIVED: {$start_ap}");
		ESP_LOGE("BLE_TEST", "📶 AP SSID WILL BE : %s", AP_SSID);
		ESP_LOGE("BLE_TEST", "🔑 AP PASS WILL BE : %s", AP_PASS);
		ESP_LOGE("BLE_TEST", "========================================");
		/* Notify app with SSID/PASS BEFORE tearing down BLE */
		char ap_info[100];
		snprintf(ap_info, sizeof(ap_info), "{$ap_info$SSID:%s,PASS:%s}", AP_SSID, AP_PASS);
		status_notify_chunks(ap_info);
		ble_notify_drain();

		vTaskDelay(pdMS_TO_TICKS(300));  /* let notify flush */

		/* Stop BLE fully — frees heap for WiFi AP + HTTP server */
		stop_ble();

		/* Start AP + HTTP server */
		start_ap_mode();
	}
	else if( data && strncmp(data, "{$delete_logs}", 12) == 0)
	{
		ESP_LOGW("BLE", "Deleting Logs...");
		erase_logs();
		// Delete the logs files

	}
	else if( data && strncmp(data, "{$time_sync$", 12) == 0)
	{
		ESP_LOGW("BLE", "Syncing Time...");
		// Time sync
		time_sync(data);
		get_device_config();
		status_notify_chunks(device_config_str);
		ble_interrupted = true;
	}
	else 
	{
		ESP_LOGW("BLE", "Unknown command received: %s", data);
	}

	reset_ble_rx_buffer();
}

void CHECK_BLE_INTERRUPT()
{
	    int msg = 0;
        if (xQueueReceive(BLE_INT_QUEUE, &msg, 0) == pdPASS && msg == 1) {
            ESP_LOGW("POLL", "Interrupt received, pausing");

            // tell BLE: I have paused
            xSemaphoreGive(BLE_INT_POLL_PAUSED);

            // wait until BLE finishes
            xSemaphoreTake(BLE_INT_BLE_DONE, portMAX_DELAY);

            ESP_LOGW("POLL", "BLE done, resuming");
            return;  // exit poll and restart fresh
        }
}

void initiate_app_sequence()
{
	ESP_LOGI("INITIATE", "Initiating application sequence...");
	// set_ble_name("ESP_TEST1_device");
	// start_BLE(DEVICE_ID);
	start_BLE(config.server_name);
	sleep(1); //Let BLE init
}



void get_sensor_data()
{
	memset(all_sensor_info, 0, sizeof(all_sensor_info));
	// if (load_sensors_from_nvs() == ESP_OK) 
	// {
		int sensor_count = get_sensor_count();
		printf("Sensor Count : %d\n",sensor_count);

		strcat(all_sensor_info, "{");
		for (int i = 0; i < sensor_count; i++) 
		{
			char sensor_info[128];
			snprintf(sensor_info, sizeof(sensor_info),
				"S_Name:%s,Key:%s,Val:%.2f,is_en:%s,Offset:%.2f,Scale:%.2f,RS485:%s,Response:%s,",
				sensors[i].name,
				sensors[i].key,
				sensors[i].value,
				sensors[i].is_enabled ? "1" : "0",
				sensors[i].offset,
				sensors[i].scale,
				sensors[i].is_rs485 ? "Yes" : "No",
				sensors[i].response_ok ? "OK" : "Fail");
			strcat(all_sensor_info, sensor_info);
		}
		strcat(all_sensor_info, "}");
		// ESP_LOGI("SENSOR INFO", "%s", all_sensor_info);
    
	// } else {
	//     ESP_LOGE(TAG_NVS, "Failed to load device config");
	// }
}



void get_device_config()
{
	memset(device_config_str, 0, sizeof(device_config_str));

	// DeviceConfig config;

	// if (load_device_config_from_nvs(&config) == ESP_OK) 
	{
		snprintf(device_config_str, sizeof(device_config_str),
		"{$device_config$"
		"Device_type:%s,"
        "IMEI_num:%s,"
        "Servr_name:%s,"
        "Fr_vr:%s,"
        "data_freq:%ld,"
        "date_fr:%s,"
		"time_fr:%s,"
		"lat:%.6f,"
		"lon:%.6f,"
		"wifi_enabled_gsm_disabled:%s,"
		"wifi_ssid:%s,"
		"wifi_password:%s,"
		"gpio_extender_enabled:%s,"
		"gsm_sim_name:%s"
		"}",
		config.device_type,
        config.IMEI_num,
        config.server_name,
        config.firmware_version,
        config.data_frequency_sec,
        config.date_format,
        config.time_format,
		config.latitude,
		config.longitude,
		config.wifi_enabled_gsm_disabled ? "1" : "0",
		config.wifi_ssid,
    	config.wifi_password,
		config.gpio_extender_enabled ? "1" : "0",
		config.gsm_sim_name
		);
		last_sent_config_crc = simple_crc(device_config_str);
		ESP_LOGI("CONFIG", "%s", device_config_str);
		// You can now use `device_config_str` in BLE notify or file write
	}

}



void get_device_status()
{
	memset(device_status_str, 0, sizeof(device_status_str));
	update_and_print_time();

    // if (load_device_status_from_nvs(&status) == ESP_OK)  
    {
        snprintf(device_status_str, sizeof(device_status_str),
			"{$device_status$"
            "date:%s,"
            "time:%s,"
            "gsm_sig:%d,"
            "gsm_post_status:%s,"
            "battery_lvl:%.2f%%,"
            "solar_lvl:%.2f%%,"
            "log_count:%ld}",
            status.current_date,
            time_app_hms,
            status.gsm_signal_strength,
            status.gsm_post_status,
            status.battery_level,
            status.solar_level,
            status.log_count
        );

        ESP_LOGI("STATUS", "%s", device_status_str);
        // You can now use `device_status_str` in BLE notify or elsewhere
    }
}

#define IST_OFFSET_SEC  (5*3600 + 30*60)

void time_sync(const char *str)
{
	const char *key = "$time_sync$";
	char *ptr = strstr(str, key);

    if (ptr != NULL) {
        // Move pointer right after $time_sync$
        ptr += strlen(key);
	}
	else
		return;
	
	 // Step 3: Extract the number until space or end of string
        char temp[20];  // buffer for the number
        int i = 0;
        while (*ptr && *ptr != '}' && i < (int)(sizeof(temp)-1)) {
            temp[i++] = *ptr++;
        }
        temp[i] = '\0';  // null terminate the number string

        // Step 4: Convert to integer
        uint32_t  timestamp = (uint32_t)strtoul(temp, NULL, 10);

        // Print result
        printf("Unix timestamp: %lu\n", timestamp);

		
		// Step 5: Convert to date/time structure
        time_t rawtime = (time_t)timestamp;
		rawtime += IST_OFFSET_SEC;
        struct tm ts;
        gmtime_r(&rawtime, &ts);  // or use gmtime_r for UTC

		// Add IST offset manually (5 hours 30 min)
		// ts.tm_hour += 5;
		// ts.tm_min += 30;

		// Normalize in case minutes/hours overflow
		mktime(&ts);

        // Step 6: Store in separate variables
        int year = ts.tm_year + 1900;
        int month = ts.tm_mon;
        int day = ts.tm_mday;
        int hour = ts.tm_hour;
        int minute = ts.tm_min;
        int second = ts.tm_sec;
		int weekday = ts.tm_wday;

        // Print results
        printf("Date: %02d-%02d-%04d\n", day, month, year);
        printf("Time: %02d:%02d:%02d\n", hour, minute, second);
		printf("Weekday:%d\n",weekday);

		set_rtc_time(day, weekday, month, year, hour, minute, second);
		/* RTC changed via BLE, re-anchor scheduler on next cycle. */
		scheduled_log_unix = 0;
		scheduled_interval_sec = 0;
}


char compact_summary_str[512];  // Adjust size as needed

void generate_compact_summary_string()
{
    compact_summary_str[0] = '\0';  // Clear buffer

    // Start JSON object
    snprintf(compact_summary_str, sizeof(compact_summary_str),
             "{"
             "\"DT\":%lld,", unix_timestamp);

    // Add config and status data with proper JSON quoting
    char head[512];
    snprintf(head, sizeof(head),
             "\"Device\":\"%s\","
			 "\"D_type\":\"%s\","
             "\"IMEI\":\"%s\","
             "\"FR_v\":\"%s\","
             "\"Lat\":%.6f,"
             "\"Lon\":%.6f,"
             "\"Bat\":%.2f,"
             "\"Sol\":%.2f,",
             config.server_name,
			 config.device_type,
             config.IMEI_num,
             config.firmware_version,
             config.latitude,
             config.longitude,
             status.battery_level,
             status.solar_level
    );

    strlcat(compact_summary_str, head, sizeof(compact_summary_str));

    // Add sensor data (only if enabled)
    int sensor_count = get_sensor_count();

    for (int i = 0; i < sensor_count; i++) {
        if (!sensors[i].is_enabled) continue;

        char sensor_data[64];
        snprintf(sensor_data, sizeof(sensor_data),
                 "\"%s\":%.2f,",
                 sensors[i].key,
                 sensors[i].value);

        strlcat(compact_summary_str, sensor_data, sizeof(compact_summary_str));
    }

    // Remove trailing comma
    size_t len = strlen(compact_summary_str);
    if (len > 0 && compact_summary_str[len - 1] == ',') {
        compact_summary_str[len - 1] = '\0';
    }

    // Close JSON object
    strlcat(compact_summary_str, "}", sizeof(compact_summary_str));
}

FILE *log_file = NULL;

void send_all_logs()
{
    FILE *f = fopen(log_file_path, "r");
    if (!f)
    {
        ESP_LOGE("send_all_logs", "Unable to open log file for reading");
        status_notify_fast("{$log_start$}");
        status_notify_fast("{$log_end$}");
        ble_notify_drain();
        return;
    }

    ESP_LOGW("send_all_logs", "Sending all logs (fast mode)");

    char line[512];
    uint32_t log_count = 0;

    status_notify_fast("{$log_start$}");

    while (fgets(line, sizeof(line), f))
    {
        status_notify_fast(line);
        log_count++;

        /* Every 20 logs, let the BLE controller drain to prevent queue overflow.
         * This spacing is critical on ESP32-NimBLE to avoid VHCI watchdog crashes. */
        if (log_count % 20 == 0) {
            ble_notify_drain();
            ESP_LOGI("send_all_logs", "Sent %lu logs so far...", (unsigned long)log_count);
        }
    }
    fclose(f);

    /* Send end marker and wait for BLE controller to flush all pending notifies */
    status_notify_fast("{$log_end$}");
    ble_notify_drain();

    ESP_LOGI("send_all_logs", "Done — sent %lu logs total", (unsigned long)log_count);
}

int uart_vprintf(const char *fmt, va_list args)
{
    char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len > 0) {
        uart_write_bytes(UART_NUM_2, buf, len);
    }
    return len;
}

// void shiftOut(uint8_t val) {
//   uint8_t i;
//         // gpio_set_level(CLOCK_PIN, 0);
//         // gpio_set_level(DATA_PIN, (value >> i) & 0x01);
//         // gpio_set_level(CLOCK_PIN, 1);
// 	gpio_set_level(LATCH_PIN, 0);  // Begin
//   for (i = 0; i < 8; i++) {
//     //   digitalWrite(dataPin, !!(val & (1 << (7 - i))));
// 	  gpio_set_level(DATA_PIN, !!(val & (1 << (7 - i))));

//     // digitalWrite(clockPin, HIGH);
// 	gpio_set_level(CLOCK_PIN, 1);
//     // digitalWrite(clockPin, LOW);
// 	gpio_set_level(CLOCK_PIN, 0);
//   }
//   gpio_set_level(LATCH_PIN, 1);  // Stop
// }



void write_sensor_RS485(uint8_t *data, int len)
{
	// printf("REDE Tx\n");
	// RE DE Pin TOGGLE

	write_SR(SEN_RS485_REDE,HIGH);
	//  shiftOut(0b10101010);  // Enable TX
	// write_SR_all(0b10101010);
	// print_binary(0b10001010);

	// vTaskDelay(1 / portTICK_PERIOD_MS);

	
	write_UART1(data, len);
	uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(100));  // Wait max 100 ms
	// ESP_LOGI("RS485","Sent !");

	// vTaskDelay(10 / portTICK_PERIOD_MS);

	// write_SR_all(0b10001010);
	// print_binary(0b10001010);
	//  shiftOut(0b00001010);  // Enable RX
	// RE DE Pin TOGGLE
	// printf("REDE Rx\n");
	write_SR(SEN_RS485_REDE,LOW);
}	


void temp_print_arr(uint8_t *arr)
{
	printf("Sent:");

	for( int i = 0; i < 8; i++)
	{
		printf(" 0x%02X", arr[i]);
	}

	printf("\n");
}

void response_print(uint8_t *arr, uint8_t len)
{
	printf("Response Arr:");

	for( int i = 0; i < len; i++)
	{
		printf(" 0x%02X", arr[i]);
	}

	printf("\n");
}

/**
 * @brief Convert date/time strings into Unix timestamp
 * @param date_str e.g. "2025-01-01"
 * @param time_str e.g. "10:05:06"
 * @return time_t (Unix timestamp), or -1 on error
 */
time_t convert_to_unix(const char *date_str, const char *time_str) {
    struct tm t = {0}; // Zero-initialize

    // Parse date (DD-MM-YYYY)
    sscanf(date_str, "%2d-%2d-%4d", &t.tm_mday, &t.tm_mon, &t.tm_year);
    t.tm_mon -= 1;            // struct tm months are 0–11
    t.tm_year -= 1900;        // struct tm years are since 1900

    // Parse time (HH:MM:SS)
    sscanf(time_str, "%2d:%2d:%2d", &t.tm_hour, &t.tm_min, &t.tm_sec);

    // Convert to Unix timestamp
    return mktime(&t);
}

bool wait_for_flags(int timeout_sec)
{
	while( timeout_sec > 0)
	{
		if (flow_counting == false && temp_checking == false && adc_sampling == false)
    		return true;

			
		vTaskDelay(100 / portTICK_PERIOD_MS);	
		timeout_sec-=100;
		CHECK_BLE_INTERRUPT();
	}
	return false;
}


bool wait_for_gsm(int timeout_sec)
{
	while( timeout_sec > 0)
	{
		if( gsm_cmd_checking == false)
			return true;

			
		vTaskDelay(100 / portTICK_PERIOD_MS);	
		timeout_sec-=100;
	}
	return false;
}


void daily_restart_check(const struct tm *timeinfo)
{
    if (!timeinfo) return;

    int today = timeinfo->tm_mday;

    nvs_handle_t nvs;
    int32_t last_day = -1;  // default invalid value

    if (nvs_open("storage", NVS_READWRITE, &nvs) == ESP_OK) {
        esp_err_t err = nvs_get_i32(nvs, "last_day", &last_day);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "No previous day stored, first run");
        }

        if (today != last_day) {
            ESP_LOGI(TAG, "New day detected! (was %ld, now %d)", last_day, today);

            // Save new day before restart
            nvs_set_i32(nvs, "last_day", today);
            nvs_commit(nvs);
            nvs_close(nvs);

            esp_restart();  // restart once per day
        }

        nvs_close(nvs);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS!");
    }
}


static void sync_rtc_from_wifi(void)
{
    if (last_rtc_sync_unix != 0 &&
        (unix_timestamp - last_rtc_sync_unix) < 86400LL) {   /* once per day */
        return;
    }

    /* sync_time() writes UTC to physical RTC */
    if (sync_time() == ESP_OK) {
        scheduled_log_unix     = 0;
        scheduled_interval_sec = 0;

        // 1. Get the fresh UTC time that sync_time() just fetched
        time_t utc_now = time(NULL); 

        // 2. Force your tracking variables to represent IST Epoch
        unix_timestamp          = utc_now + IST_OFFSET_SEC; 
        last_rtc_sync_unix = unix_timestamp;

        ESP_LOGI("WIFI_NTP", "RTC synced via SNTP and offset to IST");
    } else {
        ESP_LOGW("WIFI_NTP", "SNTP sync failed during WiFi post");
    }
}


// const char *post_body =
// 						"{\"DT\":1755113963,"
// 						"\"Device\":\"Test\","
// 						"\"IMEI\":\"123IEMEI457\","
// 						"\"FR_v\":\"1.0.0\","
// 						"\"Lat\":0.100000,"
// 						"\"Lon\":0.200000,"
// 						"\"Bat\":100.00,"
// 						"\"Sol\":100.00,"
// 						"\"PSR\":0.00,"
// 						"\"PH\":0.00,"
// 						"\"TPA\":0.00,"
// 						"\"FLW\":0.00,"
// 						"\"SW1\":0.00,"
// 						"\"SW2\":0.00}";
// 	// gsm_command_exe();
// 	// gsm_post_data(post_body);

// 	/*
// 		if gsm INIT or POST fails
// 		check 
// 		if data is already marked as failed
// 			yes - do nothing
// 			no  - mark the start of failed data (prev_log_offset)
// 		else
// 			try post failed data first
// 				success - clear failed offset
// 				fail    - do nothing, try next time
// 				atleast 5 success - update/store failed offset
// 		then try post new data
// 			success - do nothing
// 			fail    - mark the start of failed data (prev_log_offset)
// 	*/

/* Sync RTC from GSM NTP once per day.
 * Call only after a confirmed successful POST (PDP context is active).
 * Does NOT touch unix_timestamp (DT) — only writes to physical RTC. */
static void try_gsm_ntp_sync(void)
{
    /* Once per 6 hours */
    if (last_rtc_sync_unix != 0 &&
        (unix_timestamp - last_rtc_sync_unix) < 21600LL) {   // ← was 86400LL
        return;
    }

    int year, mon, day, hour, min, sec, wday;
    if (gsm_get_network_time(&year, &mon, &day, &hour, &min, &sec, &wday) != ESP_OK) {
        ESP_LOGW("GSM_NTP", "RTC sync skipped — NTP fetch failed");
        return;
    }

    /* gsm_get_network_time returns UTC. Convert to IST before writing to RTC
     * (RTC stores IST, consistent with BLE time_sync behaviour). */
    struct tm utc_t = {
        .tm_year = year - 1900,
        .tm_mon  = mon  - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min  = min,
        .tm_sec  = sec,
        .tm_isdst = 0
    };
    /* mktime with TZ=UTC (ESP-IDF default) treats struct as UTC → correct epoch */
    time_t utc_epoch = mktime(&utc_t);
    time_t ist_epoch = utc_epoch + IST_OFFSET_SEC;

    struct tm ist_t;
    gmtime_r(&ist_epoch, &ist_t);   /* break IST epoch into IST H:M:S */

	/* 1. Update physical RTC (MCP7941x via I2C) with IST */
    set_rtc_time(ist_t.tm_mday, ist_t.tm_wday,
                 ist_t.tm_mon, ist_t.tm_year,
                 ist_t.tm_hour, ist_t.tm_min, ist_t.tm_sec);

	/* 2. Update ESP32 system time with UTC so time(NULL) is correct.
     *    WiFi SNTP does this internally — GSM path must do it manually.
     *    settimeofday takes UTC, TZ handles local conversion. */
    struct timeval tv = {
        .tv_sec  = utc_epoch,
        .tv_usec = 0
    };
    if (settimeofday(&tv, NULL) != 0)
    {
        ESP_LOGW("GSM_NTP", "settimeofday failed");
    }
    else
    {
        /* Apply IST timezone so localtime_r() returns IST correctly */
        setenv("TZ", "IST-5:30", 1);
        tzset();
        ESP_LOGI("GSM_NTP", "ESP32 system time updated via settimeofday");
    }
	/* keep unix_timestamp aligned with the corrected RTC */
    unix_timestamp = (int64_t)ist_epoch;
    /* Re-anchor scheduler so next DT slot aligns to corrected RTC */
    // scheduled_log_unix    = 0;
    // scheduled_interval_sec = 0;

    last_rtc_sync_unix = unix_timestamp;   /* mark — unix_timestamp is current DT, not modified */

    ESP_LOGI("GSM_NTP", "RTC synced from NTP → IST %04d/%02d/%02d %02d:%02d:%02d",
             ist_t.tm_year + 1900, ist_t.tm_mon + 1, ist_t.tm_mday,
             ist_t.tm_hour, ist_t.tm_min, ist_t.tm_sec);
}

esp_err_t handle_gsm_post()
{
	/* Low-battery guard: GSM modem draws up to 2A in TX bursts. At low battery,
	   this surge collapses the terminal voltage enough to trigger the ESP32
	   brownout detector (Level 0, ~2.43V), resetting the device mid-transmission
	   and potentially corrupting the SPIFFS log entry being written. Block GSM
	   transmission below 20% to prevent this reboot loop. The record is already
	   safely written to SPIFFS by write_log_entry() and will be sent once
	   battery recovers and is_failed backlog drain picks it up. */
	if (status.battery_level < 20.0f) {
		ESP_LOGW("POLL", "Battery %.2f%% < 20%% — skipping GSM post to prevent brownout. Record saved to SPIFFS.", status.battery_level);
		if( get_is_failed() == false) {
			set_failed_offset(get_prev_log_offset());
		}
		strcpy(status.gsm_post_status, "FAIL");
		return ESP_FAIL;
	}

	size_t curr_offset = get_curr_log_offset(); // Updated in write_log_entry	
																		//	 prev_log_offset      curr_offset
																		//		   |				   |
																		//         v                   V			
	size_t prev_log_offset = get_prev_log_offset();	// Start of current offset { [100] +  LOG_x   =  [200] }
	ESP_LOGW("POLL", "Backlog pending before post: %zu", get_backlog_count());


	if ( gsm_at() == ESP_FAIL )
	{
		ESP_LOGE("POLL","RESTARTING MODEM");

		write_SR(GSM_en, LOW);
		vTaskDelay(3000 / portTICK_PERIOD_MS);
		write_SR(GSM_en, HIGH);
		vTaskDelay(pdMS_TO_TICKS(10000));
		
		strcpy(status.gsm_post_status, "FAIL");// Reset GSM post status
		gsm_requires_init = true;

		/* If modem stops responding (voltage spike, thermal shutdown), gsm_at() fails, power-cycles modem, returns ESP_FAIL — but never calls set_failed_offset().
		 The current entry is written to SPIFFS but orphaned forever. Next cycle sees is_failed=false and posts the next entry, 
		 skipping the orphaned one permanently. mark entry as backlog so it's retried next cycle */
		if( get_is_failed() == false) {
			set_failed_offset(prev_log_offset);
			ESP_LOGE("POLL", "gsm_at fail — set failed offset %d", prev_log_offset);
		}
		return ESP_FAIL;
	}


	bool init_failed = false;

	if(gsm_requires_init == true)
	{
		static int gsm_init_fail_count = 0;  // counts consecutive gsm_command_exe failures

		esp_err_t ret = gsm_command_exe();

		if( ret == ESP_FAIL )
		{	 
			strcpy(status.gsm_post_status, "FAIL");// Reset GSM post status
			ESP_LOGE("POLL", "GSM init failed");

			gsm_init_fail_count++;
			ESP_LOGW("POLL", "GSM init fail count: %d/3", gsm_init_fail_count);

			/* After 3 consecutive failures, force a full modem power-cycle so the
			   modem re-scans the SIM slot. This recovers from SIM removal + re-insertion
			   without needing a device reboot. 10s stabilisation gives the modem enough
			   time to boot, detect the SIM, and complete CREG before the next cycle. */
			if (gsm_init_fail_count >= 3) {
				ESP_LOGE("POLL", "3 consecutive GSM init failures — power-cycling modem for SIM re-detection");
				write_SR(GSM_en, LOW);
				vTaskDelay(pdMS_TO_TICKS(3000));
				write_SR(GSM_en, HIGH);
    			vTaskDelay(pdMS_TO_TICKS(10000));
				/* No inline stabilisation wait needed. poll_sensor() at the start of
				the next cycle already has vTaskDelay(10000ms) for sensor warmup,
				giving the modem ~70s total (60s interval + 10s stabilize) to
				boot, detect SIM, and complete CREG — more than enough. */
				gsm_init_fail_count = 0;
				ESP_LOGW("POLL", "Modem power-cycled — SIM re-detection will complete by next cycle");
			}

			// size_t failed_offset = get_failed_offset();

			if( get_is_failed() == false)
			{
				set_failed_offset(prev_log_offset); // Failed log address is start of current log address
				ESP_LOGE("POLL", "Set failed offset %d", prev_log_offset);
			}
			else
			{
				ESP_LOGE("POLL", "Failed offset ALREADY SET %d", get_failed_offset());
			}

			init_failed = true;
		}
		else
		{
			gsm_init_fail_count = 0;  // reset on success
			gsm_requires_init = false;
		}
	}


	if( init_failed == false)
	{
		post_status_t checked = check_and_try_failed(false);
		// post_status_t checked = no_failed_data;				// testing

		if( checked == no_failed_data)
		{
			// Post the new data data
			if( gsm_post_data(compact_summary_str) == ESP_FAIL )
			{
				// size_t failed_offset = get_failed_offset();
				
				if( get_is_failed() == false)
				{
					set_failed_offset(prev_log_offset); // Failed log address is start of current log address
					ESP_LOGE("POLL", "Set failed offset %d", prev_log_offset);
				}
				else
				{
					ESP_LOGE("POLL", "Failed offset ALREADY SET %d", get_failed_offset());
				}
				strcpy(status.gsm_post_status, "FAIL");// Reset GSM post status
				gsm_requires_init = true;
				return ESP_FAIL;
			}
			else
			{
				backlog_post_fail_streak = 0;
				strcpy(status.gsm_post_status, "OK");
				ESP_LOGI("POLL", "Posted !!");
				/* Advance NVS to curr_log_offset and clear is_failed.
				* Without this, if a previous fopen failure left is_failed=true,
				* every subsequent cycle re-enters the backlog path forever. */
				set_post_status(get_curr_log_offset(), false, false);
				gsm_post_secondary(compact_summary_str);
				try_gsm_ntp_sync();
				return ESP_OK;
			}
		}
		else if (checked == posted_failed_data)
		{
			backlog_post_fail_streak = 0;
			ESP_LOGI("POLL", "Posted all failed data successfully. Now posting current record.");
			/* After draining the backlog, the current cycle's record (already written
			   to SPIFFS by write_log_entry) must still be posted. Previously this
			   branch returned immediately, silently dropping the current record every
			   time a backlog recovery happened. */

			/* Guard: if the backlog drain already included the current record
			   (i.e. failed_offset had caught up to curr_log_offset, meaning the
			   last backlog entry IS the current entry), skip to avoid a duplicate
			   post that the server rejects with "already exists". */
			if( get_prev_log_offset() == get_curr_log_offset() )
			{
				ESP_LOGW("POLL", "Current record was last in backlog drain — skipping duplicate post.");
				strcpy(status.gsm_post_status, "OK");
				return ESP_OK;
			}

			if( gsm_post_data(compact_summary_str) == ESP_FAIL )
			{
				if( get_is_failed() == false)
				{
					set_failed_offset(prev_log_offset);
					ESP_LOGE("POLL", "Current record post failed after backlog drain, set failed offset %d", prev_log_offset);
				}
				strcpy(status.gsm_post_status, "FAIL");
				gsm_requires_init = true;
				return ESP_FAIL;
			}
			strcpy(status.gsm_post_status, "OK");
			ESP_LOGI("POLL", "Current record posted after backlog drain.");
			gsm_post_secondary(compact_summary_str);
			try_gsm_ntp_sync();   /* once/day RTC correction via NTP */
			return ESP_OK;

		}
		else if ( checked == failed_posting_failed_data )
		{
			ESP_LOGE("POLL", "Failed to post failed data");
			strcpy(status.gsm_post_status, "FAIL"); // Reset GSM post status
			backlog_post_fail_streak++;
			ESP_LOGW("POLL", "Backlog post fail streak: %u/10", backlog_post_fail_streak);
			if (backlog_post_fail_streak >= 10) {
				gsm_requires_init = true;
				backlog_post_fail_streak = 0;
				ESP_LOGW("POLL", "Backlog failed 10 times — forcing GSM re-init");
			}
			return ESP_FAIL;
		}
		else if (checked == backlog_drain_in_progress)
		{
			backlog_post_fail_streak = 0;
			ESP_LOGW("POLL", "Backlog batch done — yielding for next cycle, drain continues");
			strcpy(status.gsm_post_status, "OK");
			try_gsm_ntp_sync();
			return ESP_OK;
		}
	}
	return ESP_FAIL;
}

esp_err_t handle_wifi_post()
{

	size_t curr_offset = get_curr_log_offset(); // Updated in write_log_entry
																		//	 prev_log_offset      curr_offset
																		//		   |				   |
																		//         v                   V
	size_t prev_log_offset = get_prev_log_offset();	// Start of current offset { [100] +  LOG_x   =  [200] }


	esp_err_t wifi_success = ESP_OK;

	if( wifi_is_connected() == false)
	{
		ESP_LOGW("MAIN"," Connecting to WiFi...");
		if( wifi_handler_init((uint8_t *)config.wifi_ssid, (uint8_t *)config.wifi_password) == ESP_OK)
		{
			ESP_LOGW("MAIN"," WiFi Connected Successfully");
			wifi_success = ESP_OK;
		}
		else
		{
			ESP_LOGE("MAIN"," WiFi Connection Failed");
			wifi_success = ESP_FAIL;
		}
	}

	if( wifi_success == ESP_FAIL)
	{
		if( get_is_failed() == false)
		{
			set_failed_offset(prev_log_offset); // Failed log address is start of current log address
			ESP_LOGE("POLL", "Set failed offset %d", prev_log_offset);
		}
		else
		{
			ESP_LOGE("POLL", "Failed offset ALREADY SET %d", get_failed_offset());
		}
		strcpy(status.gsm_post_status, "FAIL");// Reset GSM post status
		return ESP_FAIL;
	}

	// Runs once (rate-limited internally). Fires on very first post after
    // reconnect regardless of whether backlog exists or not.
    // sync_rtc_from_wifi();

	post_status_t checked = check_and_try_failed(true);
	// post_status_t checked = no_failed_data;				// testing

	if (checked == no_failed_data)
	{
		esp_err_t ret = http_send_json(POST_URL, compact_summary_str, HTTP_METHOD_POST);
		if (ret == ESP_OK)
		{
			strcpy(status.gsm_post_status, "OK");
			ESP_LOGI("POLL", "Posted !!");
			set_post_status(get_curr_log_offset(), false, false);
			sync_rtc_from_wifi();
			/* Fire-and-forget to secondary server */
			http_send_json(SECONDARY_URL, compact_summary_str, HTTP_METHOD_POST);
		}
		else
		{
			/* Catches ESP_FAIL, ESP_ERR_HTTP_CONNECT, and any other error.
			 * Old code checked == ESP_FAIL only, so ESP_ERR_HTTP_CONNECT (0x7006)
			 * fell into the silent "else" branch with no return and no NVS save. */
			ESP_LOGE("POLL", "WiFi post failed (err=0x%x)", (unsigned)ret);
			if (get_is_failed() == false)
			{
				set_failed_offset(prev_log_offset);
				ESP_LOGE("POLL", "Set failed_offset to %zu", (size_t)prev_log_offset);
			}
			else
			{
				ESP_LOGE("POLL", "Failed offset ALREADY SET %zu", (size_t)get_failed_offset());
			}
			strcpy(status.gsm_post_status, "FAIL");
			return ESP_FAIL;
		}
	}
	else if (checked == backlog_drain_in_progress)
	{
		ESP_LOGW("POLL", "Backlog batch done — yielding for next cycle, drain continues");
		strcpy(status.gsm_post_status, "OK");
		sync_rtc_from_wifi();   /* first successful post of the day — sync RTC even mid-drain */
		return ESP_OK;
	}
	else if (checked == posted_failed_data)
	{
		ESP_LOGI("POLL", "Posted all backlog successfully. Now posting current record.");
		if (get_prev_log_offset() == get_curr_log_offset()) {
			ESP_LOGW("POLL", "Current record was last in backlog drain — skipping duplicate post.");
			/* Ensure NVS is in sync even on this early-exit path */
			set_post_status(get_curr_log_offset(), false, false);
			strcpy(status.gsm_post_status, "OK");
			return ESP_OK;
		}
		if (http_send_json(POST_URL, compact_summary_str, HTTP_METHOD_POST) != ESP_OK) {
			if (get_is_failed() == false) set_failed_offset(prev_log_offset);
			strcpy(status.gsm_post_status, "FAIL");
			return ESP_FAIL;
		}
		strcpy(status.gsm_post_status, "OK");
		/* Advance NVS offset after successful current post */
		set_post_status(get_curr_log_offset(), false, false);
		sync_rtc_from_wifi();
		http_send_json(SECONDARY_URL, compact_summary_str, HTTP_METHOD_POST);
	}
	else if (checked == failed_posting_failed_data)
	{
		ESP_LOGE("POLL", "Failed to post backlog data");
		strcpy(status.gsm_post_status, "FAIL");
	}

	return ESP_OK;
}

static time_t get_realtime_rtc_ts(void)
{
    struct tm rtc_tm;
    readClock(&rtc_tm);
    
    // 1. Save the current timezone setting to restore later
    char old_tz[32] = {0};
    char *tz = getenv("TZ");
    if (tz) {
        strncpy(old_tz, tz, sizeof(old_tz) - 1);
    }
    
    // 2. Temporarily switch the system timezone to pure UTC
    setenv("TZ", "UTC0", 1);
    tzset();
    
    // 3. Convert broken-down time to epoch (now evaluated cleanly as UTC)
    time_t rtc_now = mktime(&rtc_tm);
    
    // 4. Restore your device's original local timezone (e.g., IST)
    if (old_tz[0]) {
        setenv("TZ", old_tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();

    // Fallback logic
    if (rtc_now <= 0) {
        rtc_now = (unix_timestamp > 0) ? unix_timestamp : 1754894220;
    }
    return rtc_now;
}

uint8_t SensorSelect = PRESSURE;

static uint8_t modbus_arr[8] = {0};

static volatile bool modbus_response_status[SENSOR_TYPE_COUNT] = {false};

uint16_t data_t = 0;
//  float pressureVal, phVal, ufFlowVal1, ufVolumeVal1, tdsVal, chlorine_PCVal, chlorine_NCVal, SW1_mod_val, SW2_mod_val,  ufFlowVal2, ufVolumeVal2, weight_val, turbidityVal;
//  float tempWVal, tempAVal, FlowVal, Sw1Val, Sw2Val, tempVal; 

/* ─────────────────────────────────────────────────────────────────
   PART 1 — collect_and_log()
   Runs every 60s on the main loop timer. Reads all sensors, writes
   the record to SPIFFS. Returns in ~15s. NEVER blocked by GSM.
   ───────────────────────────────────────────────────────────────── */
void collect_and_log(void)
{
    struct tm rtc_now_tm;
    char rtc_now_date[16];
    char rtc_now_time[16];
    readClock(&rtc_now_tm);
    strftime(rtc_now_date, sizeof(rtc_now_date), "%d-%m-%Y", &rtc_now_tm);
    strftime(rtc_now_time, sizeof(rtc_now_time), "%H:%M:%S", &rtc_now_tm);
    ESP_LOGI("POLL", "Current Device Date: %s, Time: %s", rtc_now_date, rtc_now_time);

    ESP_LOGI("POLL", "Collecting sensor data...");
    sensor_data_colleccted = false;

    write_SR(GSM_en, HIGH);          // Power modem ON early — gets free 10s warmup
    if (!get_is_failed()) {
        gsm_requires_init = true;    // Normal cycle: allow cold init if modem was off
    } else {
        ESP_LOGW("POLL", "Backlog pending — keeping modem session without forced re-init");
    }
    write_SR(IO_12V, HIGH);
    write_SR(SEN_RS485_en, HIGH);

    vTaskDelay(10000 / portTICK_PERIOD_MS);  // Sensors stabilize + modem boots in parallel

    /* Sensor Data */
    while(1)
    {
        for( SensorSelect = 0; SensorSelect < MODBUS_Sensor_Count(); SensorSelect++)
        {
            sensor_modbus_requests();
            vTaskDelay(100 / portTICK_PERIOD_MS);
            CHECK_BLE_INTERRUPT();
        }
        process_data_modbus();
        break;
    }

    write_SR(IO_12V, LOW);
    write_SR(SEN_RS485_en, LOW);

    /* Use scheduled slot timestamp so SPIFFS logs stay exactly interval-spaced
       even if backlog posting delays execution.
       EXCEPTION: when BLE is active, use the actual RTC time so rapid
       back-to-back BLE logs get the real wall-clock DT, not future values
       produced by repeatedly advancing the scheduler counter. */
    if (ble_mode_active) {
        unix_timestamp = get_realtime_rtc_ts();
        ESP_LOGW("POLL", "[BLE-MODE] Using real RTC timestamp: %lld", (long long)unix_timestamp);
    } else {
        unix_timestamp = get_next_scheduled_log_ts(config.data_frequency_sec);
    }
    struct tm scheduled_tm;
    localtime_r(&unix_timestamp, &scheduled_tm);
    strftime(status.current_date, sizeof(status.current_date), "%d-%m-%Y", &scheduled_tm);
    strftime(status.current_time, sizeof(status.current_time), "%H:%M:%S", &scheduled_tm);
    strftime(time_app_hms, sizeof(time_app_hms), "%H-%M-%S", &scheduled_tm);
    ESP_LOGI("POLL", "Scheduled Date: %s, Time: %s", status.current_date, status.current_time);
    // struct tm rtc_now_tm;
    // char rtc_now_date[16];
    // char rtc_now_time[16];
    readClock(&rtc_now_tm);
    strftime(rtc_now_date, sizeof(rtc_now_date), "%d-%m-%Y", &rtc_now_tm);
    strftime(rtc_now_time, sizeof(rtc_now_time), "%H:%M:%S", &rtc_now_tm);
    ESP_LOGW("POLL_DBG", "... | backlog_pending=%s",
         get_is_failed() ? "yes" : "no");

	// Line 1074 in handle_gsm_post — runs only during post, count is fine here:
	ESP_LOGW("POLL", "Backlog pending before post: %zu", get_backlog_count());

    /* Wait for ADC / flow / temp tasks */
    ESP_LOGE("POLL", "Waiting for Flags!");
    wait_for_flags(5000);

    /* Flow Sensor */
    sensors[FLW].value = flow_rate_lpm;
    ESP_LOGE("POLL", "Flow Rate: %f", flow_rate_lpm);

    /* Temperature */
    sensors[TPA].value = temperature;
    ESP_LOGE("POLL", "A-Temperature: %f", temperature);

	/* Pump Feedback Relay (SW1ob) */
    // Note: SW1 is GPIO 34
    /* SW1ob onboard GPIO — not used for FLOW_SW (that comes from Modbus SW1_mod) */
	sensors[SW1ob].value = 0.0f;
	sensors[SW1ob].is_enabled = false;  // exclude from JSON log

	// /* --- HIJACK UFM_Test FOR PUMP FEEDBACK --- */
    // // Reading SW1 (GPIO 34, mapped to OP1 terminal)
    // // Overwrites any Modbus failure zeroes for UFM_Test
    // sensors[UFM_Test].value = (float)(!gpio_get_level(SW1));
    // ESP_LOGI("POLL", "Pump Feedback (UFM-T): %.0f", sensors[UFM_Test].value);
    
	/* ADC */
    status.battery_level = BatteryPercentage(sampled_voltage1);
    status.solar_level   = SOLARPercentage(sampled_voltage2);
    ESP_LOGW("BATT_DBG",
             "ADC avg VbatSense=%.3fV VsolSense=%.3fV | Battery=%.2f%% Solar=%.2f%%",
             sampled_voltage1, sampled_voltage2, status.battery_level, status.solar_level);


    ESP_LOGE("POLL", "SW1_mod: %d | SW2_mod: %d\n",
             (int)sensors[SW1_mod].value, (int)sensors[SW2_mod].value);
    sensor_data_colleccted = true;

    CHECK_BLE_INTERRUPT();

    /* GPS (only when needed) */
    if (check_GPS_data || config.latitude == 0.0 || config.longitude == 0.0)
    {
        if (get_gps_location(&latitude, &longitude, CHECK_BLE_INTERRUPT) == ESP_OK)
        {
            ESP_LOGI("GPS data", "Fetch Success");
            check_GPS_data = false;
        }
        else
        {
            ESP_LOGE("GPS data", "Fetch Fail");
        }
    }

    CHECK_BLE_INTERRUPT();

    /* Lat/Long save */
    if (latitude != config.latitude && latitude != 0.0) {
        config.latitude  = latitude;
        config.longitude = longitude;
        save_device_config_to_nvs(&config);
        load_device_config_from_nvs(&config);
        ESP_LOGI("POLL", "SAVING LATITUDE");
    }
    if (longitude != config.longitude && longitude != 0.0) {
        config.latitude  = latitude;
        config.longitude = longitude;
        save_device_config_to_nvs(&config);
        load_device_config_from_nvs(&config);
        ESP_LOGI("POLL", "SAVING LONGITUDE");
    }
    if (longitude != 0.0 || latitude != 0.0) {
        config.latitude  = latitude;
        config.longitude = longitude;
        ESP_LOGI("POLL", "LAT: %f | LONG: %f", latitude, longitude);
    }

    /* Build string and write to SPIFFS — this is the guaranteed 60s log write */
    generate_compact_summary_string();
    printf("Compact Summary: %s\n", compact_summary_str);
    write_log_entry(compact_summary_str);  // ← SPIFFS write. Always happens. Never blocked.
}

static void sync_log_schedule_to_rtc(uint32_t interval_sec)
{
    if (interval_sec == 0) {
        interval_sec = 60;
    }

    /* Always read current RTC to check for drift */
    struct tm rtc_tm;
    readClock(&rtc_tm);

    // 1. Save the current timezone setting to restore later
    char old_tz[32] = {0};
    char *tz = getenv("TZ");
    if (tz) {
        strncpy(old_tz, tz, sizeof(old_tz) - 1);
    }
    
    // 2. Temporarily switch the system timezone to pure UTC to mimic timegm()
    setenv("TZ", "UTC0", 1);
    tzset();
    
    // 3. Convert broken-down time to epoch (TZ-blind conversion)
    time_t rtc_now = mktime(&rtc_tm);   
    
    // 4. Immediately restore your device's original local timezone
    if (old_tz[0]) {
        setenv("TZ", old_tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();

    // Fallback if clock reading fails
    if (rtc_now <= 0) {
        rtc_now = (unix_timestamp > 0) ? unix_timestamp : 1754894220;
    }

    /* Re-anchor conditions:
     * 1. First run (scheduled_log_unix == 0)
     * 2. Interval changed (e.g. config update)
     * 3. DRIFT: scheduler has fallen more than one full interval behind the RTC.
     * This catches RTC forward-jumps from BLE/NTP sync that were not reflected
     * in scheduled_log_unix (the old early-return prevented re-anchoring).
     * Normal operation: (rtc_now - scheduled_log_unix) is 0 to ~interval_sec,
     * so the drift check never fires under normal conditions. */
    bool drift_detected = (scheduled_log_unix != 0) &&
                          ((rtc_now - scheduled_log_unix) > (time_t)interval_sec);

    if (scheduled_log_unix != 0 &&
        scheduled_interval_sec == interval_sec &&
        !drift_detected) {
        return;
    }

    if (drift_detected) {
        ESP_LOGW("POLL", "Scheduler %.0fs behind RTC — re-anchoring to current RTC time",
                 difftime(rtc_now, scheduled_log_unix));
    }

    scheduled_log_unix     = rtc_now;
    scheduled_interval_sec = interval_sec;
    ESP_LOGW("POLL", "Log schedule anchored at %lld with %lu sec interval",
             (long long)scheduled_log_unix, (unsigned long)scheduled_interval_sec);
}

static time_t get_next_scheduled_log_ts(uint32_t interval_sec)
{
    sync_log_schedule_to_rtc(interval_sec);
    time_t ts = scheduled_log_unix;
    scheduled_log_unix += (time_t)scheduled_interval_sec;
    return ts;
}

bool get_wifi_mode_enabled(void)
{
    return config.wifi_enabled_gsm_disabled;
}

/* ─────────────────────────────────────────────────────────────────
   PART 2 — post_data()
   Called after collect_and_log() in the main loop. Handles GSM/WiFi
   posting including backlog drain. May take many minutes during
   backlog recovery — that is fine because collect_and_log() already
   wrote this cycle's record to SPIFFS before post_data() was called.
   ───────────────────────────────────────────────────────────────── */
void post_data(void)
{
    bool backlog_pending = get_is_failed();
    
	if (gsm_requires_init) {
        write_SR(GSM_en, HIGH);
        ESP_LOGW("GSM_PWR", "GSM modem ON — waiting for boot...");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    if( config.wifi_enabled_gsm_disabled == true)
    {
        ESP_LOGW("MAIN", " WiFi mode — ensuring GSM OFF");
		write_SR(GSM_en, LOW);    
        handle_wifi_post();
    }
    else
    {
        ESP_LOGW("MAIN", " GSM Mode Active");
        handle_gsm_post();
    }

    /* Modem power-off after posting */
    if( !check_GPS_data && !backlog_pending)
    {
        ESP_LOGW("GSM_PWR", "Powering OFF GSM modem...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        write_SR(GSM_en, LOW);
        gsm_requires_init = true;
        ESP_LOGW("GSM_PWR", "GSM modem OFF. Will cold-start next cycle.");
    }
    else if (!check_GPS_data && backlog_pending)
    {
        ESP_LOGW("GSM_PWR", "Backlog active — keeping GSM modem ON for faster drain");
    }

    /* Daily restart check */
    struct tm current_time;
    readClock(&current_time);
    daily_restart_check(&current_time);
}

void backlog_post_task(void *pvParameters)
{
    while (1)
    {
        /* Wake when main loop has stored a fresh SPIFFS log. */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		if (ap_mode_active) {
            continue;  /* don't post while serving file */
        }
        /* Skip aggressive retries when battery guard blocks posting. */
        if (status.battery_level < 20.0f) {
            ESP_LOGW("POST_WORKER", "Battery %.2f%% < 20%%, deferring backlog post retry.", status.battery_level);
            continue;
        }

        if (!ble_cmd_processing) {
            post_data();
            // Push updated post status to BLE app immediately after posting
            // (avoids showing stale FAIL when GSM post took longer than BLE notify interval)
            if (is_ble_connected_()) {
                get_device_status();
                status_notify_chunks(device_status_str);
            }
        }

        /* If backlog remains and battery is healthy, self-trigger a fast next pass.
           This preserves quick drain without tight flooding loops. */
        if (get_is_failed() && status.battery_level >= 20.0f) {
            // vTaskDelay(pdMS_TO_TICKS(300));
			vTaskDelay(pdMS_TO_TICKS(1000));    // 1s gap between backlog retries for burst mode
            xTaskNotifyGive(backlog_post_task_handle);
        }
    }
}

// 0 to 5 times fail
// 5 to 10 times pass
// reset count

esp_err_t fake_gsm_post_data(uint8_t count)
{
	static uint8_t fail_count = 0;

	// if fail_count > 5 , pass the data, so 5 times fail
	if( fail_count++ >= count)
	{
		return ESP_OK;
	}
	else if (fail_count >= count * 2)	// at fail_count = 10, i.e 5 times failed and 5 times passed, reset
	{
		fail_count = 0;
		return ESP_OK;
	}
	else
	{
		ESP_LOGE("FAKE GSM POST", "Failed to post data");
		return ESP_FAIL;
	}
}

void print_sensor_name(uint8_t sensor)
{
	switch (sensor) {
		case PRESSURE:
			printf("PRESSURE\n");
			break;

		case PH:
			printf("PH\n");
			break;

		case TDS:
			printf("TDS\n");
			break;
			
		case TPW:
			printf("TDS\n");
			break;

		case CHLORINE_PC:
			printf("CHLORINE PC\n");
			break;

		case CHLORINE_NC:
			printf("CHLORINE NC\n");
			break;

		case SW1_mod:
			printf("FLOW_SW\n");
			break;

		case SW2_mod:
			printf("SW2 MOD\n");
			break;	

		// case UFM_Test:
		// 	 printf("UFM_Test\n");
		// 	 break;

		case UFM1_FLOW:
			 printf("UFM1_FLOW\n");
			 break;

		case UFM1_VOLUME:
			 printf("UFM1_VOLUME\n");
			 break;

		case UFM2_FLOW:
			 printf("UFM2_FLOW\n");
			 break;

		case UFM2_VOLUME:
			 printf("UFM2_VOLUME\n");
			 break;

		case WEIGHT_SENSOR:
			 printf("WEIGHT_SENSOR\n");
			 break;

		case TURBIDITY:
			 printf("TURBIDITY\n");
			 break;

		default:
			printf("Unknown Sensor\n");
			break;
	}
}

float temp_chl = 0.0;
uint8_t count_chl_fail = 0;

void sensor_modbus_requests()
{
	if(SensorSelect == 0)
	{
		for( int i = 0 ; i < SENSOR_TYPE_COUNT; i++)
			modbus_response_status[i] = false;
	}

	write_SR(SEN_RS485_en,HIGH);
	vTaskDelay(100 / portTICK_PERIOD_MS);

	MODBUS_handler(SensorSelect,  modbus_arr);

	uint8_t resp_len = len_of_modbus_response();

	if( resp_len == 0)
	{
		ESP_LOGW("RETURN", " Expected Response Len is 0");
		return;
	}

	// set_UART2_rx_frame_len(resp_len);
	set_UART1_rx_frame_len(resp_len);
	
	print_sensor_name(SensorSelect);
	temp_print_arr(modbus_arr);
	ESP_LOGI("LEN","Expected Response Length: %d\n",resp_len);

	uint8_t resp[resp_len];
	memset(resp, 0, resp_len);

	clear_contentuart1_buff();
	write_sensor_RS485(modbus_arr, sizeof(modbus_arr));
	
        if( get_UART1_data(resp, 500) == ESP_FAIL)
        {
			modbus_response_status[SensorSelect] = false;
            printf("Timeout from slave: ");
			print_sensor_name(SensorSelect);

			
			// if( temp_chl != 0.0 && SensorSelect == CHLORINE_PC && count_chl_fail < 4)
			// {
			// 	// chlorineVal = temp_chl;
			// 	modbus_response_status[SensorSelect] = true;
			// 	count_chl_fail++;
			// }

			return;
        }
		// else
		// {
		// 	// Just for chlorine sensor , keeps failing sometimes randomly
		// 	if(SensorSelect == CHLORINE_PC )
		// 	{
		// 		count_chl_fail = 0;
		// 	}
		// }

	 // CRC validation
    uint16_t crc_calc = modbus_crc(resp, resp_len - 2);
    uint16_t crc_recv = resp[resp_len - 2] | (resp[resp_len - 1] << 8);
	
	// if( SensorSelect != TURBIDITY)  // skip CRC for TURBIDITY
    if (crc_calc != crc_recv) {
		print_sensor_name(SensorSelect);
		modbus_response_status[SensorSelect] = false;
        printf("CRC mismatch! Expected: 0x%04X, Got: 0x%04X\n", crc_calc, crc_recv);
        return;
    }

    uint8_t byte_count = resp[2];
    if (byte_count != (resp_len - 5)) {
        printf("Invalid byte count\n");
		modbus_response_status[SensorSelect] = false;
        return;
    }

	modbus_response_status[SensorSelect] = true;
	// printf("Data rxd for %d\n", SensorSelect);

    switch (SensorSelect) {
        case PRESSURE:
            data_t = (resp[3] << 8) | resp[4];
			// pressureVal = (float) data_t / 100;
			sensors[PRESSURE].value = (float) data_t / 100;
            printf("Pressure: %f | raw: %d \n", sensors[PRESSURE].value, data_t );
            break;

        case PH:
            data_t = (resp[3] << 8) | resp[4];
			sensors[PH].value= (float) data_t / 100;
			if( sensors[PH].value > 14.0 )	sensors[PH].value = 0.0;
            printf("PH: %f | raw: %d \n", sensors[PH].value, data_t);
            break;

        case TDS:
            sensors[TDS].value  = decodeModbusResponse_TDS(resp);
			data_t = (resp[3] << 8) | resp[4];
            // data_t = (resp[5] << 8) | resp[6];
			// tempVal = (float) data_t ;
			// // tempWVal = (float) data_t / 100;
			// sensors[TPW].value = tempVal;   // TempW
            printf("TDS: %f | Raw:%d \n", sensors[TDS].value, data_t);
            break;

		case TPW:
			data_t = (resp[3] << 8) | resp[4];
			sensors[TPW].value =  (float) data_t;
			printf("Temp PW: %f | raw: %d \n", sensors[TPW].value, data_t);
			break;

        case CHLORINE_PC:
			sensors[CHLORINE_PC].value = decodeModbusResponse_Chl(resp);
            // chlorineVal = (resp[3] << 8) | resp[4];
			temp_chl = sensors[CHLORINE_PC].value; 
            printf("Chlorine PC: %f\n", sensors[CHLORINE_PC].value);
            break;
		
		case CHLORINE_NC:
            sensors[CHLORINE_NC].value = (resp[3] << 8) | resp[4];
			sensors[CHLORINE_NC].value = sensors[CHLORINE_NC].value / 1000;
            printf("Chlorine NC: %f\n", sensors[CHLORINE_NC].value);
            break;

		case SW1_mod:
		    sensors[SW1_mod].value = (resp[3] << 8) | resp[4];
            printf("FLOW_SW: %f\n", sensors[SW1_mod].value);
            break;
		
		case SW2_mod:
		     sensors[SW2_mod].value  = (resp[3] << 8) | resp[4];
            printf("SW2 MOD: %f\n",  sensors[SW2_mod].value );
            break;

		// case UFM_Test:
		// 	//  float test = decodeModbusResponse_UFM(resp);
		// 	//  printf("UFM Test: %f\n",test);
		// 	 break;

		case UFM1_FLOW: // Cumulative Flow 
			// 	sensors[UFM1_FLOW].value  = decodeModbusResponse_Cumulative(resp)* 1000.0;
			// Removed * 1000.0 to keep the value in Cubic Meters to match UFM display
			// sensors[UFM1_FLOW].value  = decodeModbusResponse_Cumulative(resp);
			sensors[UFM1_FLOW].value  = decodeModbusResponse_UFM(resp);
            printf("UFM1 Flow: %.2f m3\n",  sensors[UFM1_FLOW].value );
            break;

        case UFM1_VOLUME: // Instantaneous Flow Rate (Register 1447)
		 	// sensors[UFM1_VOLUME].value  = decodeModbusResponse_UFM(resp)* 1000.0; // Floating point
			// sensors[UFM1_VOLUME].value  = decodeModbusResponse_UFM(resp); // Floating point
			sensors[UFM1_VOLUME].value  = decodeModbusResponse_Cumulative(resp); // Floating point

			// FORMULA for decimal point conversion

			//  sensors[UFM1_FLOW].value = 0; // Store final value in this
			//  sensors[UFM1_VOLUME].value  = 0.0;
		
            // ufVolumeVal = (resp[3] << 8) | resp[4];
            printf("UFM1 Volume: %f\n",  sensors[UFM1_VOLUME].value );
            break;

		case UFM2_FLOW: // Cumulative flow 
			//  sensors[UFM2_FLOW].value = decodeModbusResponse_Cumulative(resp)* 1000.0;
			// Removed * 1000.0 to keep the value in Cubic Meters to match UFM display
            //  sensors[UFM2_FLOW].value = decodeModbusResponse_Cumulative(resp);
            sensors[UFM2_FLOW].value  = decodeModbusResponse_UFM(resp);
			printf("UFM2 Cumulative Flow: %.2f m3\n", sensors[UFM2_FLOW].value);
            break;
		
		case UFM2_VOLUME: 
		 	// sensors[UFM2_VOLUME].value  = decodeModbusResponse_UFM(resp); 
			sensors[UFM2_VOLUME].value  = decodeModbusResponse_Cumulative(resp); // Floating point
            printf("UFM2 Volume: %.3f m3/h\n",  sensors[UFM2_VOLUME].value );
            break;

		case WEIGHT_SENSOR:
			data_t = (resp[3] << 8) | resp[4];
			 sensors[WEIGHT_SENSOR].value = (float) data_t;
			if( !( sensors[WEIGHT_SENSOR].value  > 0.0 &&  sensors[WEIGHT_SENSOR].value  < 10000.0))		 sensors[WEIGHT_SENSOR].value  = 0;
            printf("WEIGHT: %f| raw: %d \n",  sensors[WEIGHT_SENSOR].value , data_t);
            break;

		case TURBIDITY:
			data_t = (resp[3] << 8) | resp[4];
			 sensors[TURBIDITY].value = (float) data_t;
            printf("Turbidity: %f| raw: %d \n",  sensors[TURBIDITY].value , data_t);
            break;

        default:
            printf("Unknown sensor select\n");
    }


	// vTaskDelay(10 / portTICK_PERIOD_MS);
	// write_SR(SEN_RS485_en,LOW);
	
}

// extern sensor_config_t sensors[MAX_SENSORS];


 
void process_data_modbus(void)
{

    for (int i = 0; i < SENSOR_TYPE_COUNT; i++) 
	{
        if (modbus_response_status[i] && sensors[i].is_enabled) {
            sensors[i].response_ok = true;
			// sensors[i].value is already set in sensor_modbus_requests
        } else {
            sensors[i].response_ok = false;
            sensors[i].value = 0.0f; 
        }
		
	printf("[MODBUS] Sensor[%d] | %s <- %.2f [%s]  | en: [%s]\n",
       i,
	   sensors[i].key,
       sensors[i].value,
       (sensors[i].response_ok ? "OK" : "FAIL"),
	   (sensors[i].is_enabled ? "YES" : "NO"));
    }
}



static void IRAM_ATTR flow_sensor_isr_handler(void *args)
{
    flow_pulse_count++;
}



void init_flow_buzz_gpio()
{
	// gpio_pad_select_gpio(FLOW_SENSOR);
    gpio_set_direction(FLOW_SENSOR, GPIO_MODE_INPUT);
    // gpio_pulldown_en(INPUT_PIN);
    // gpio_pullup_dis(INPUT_PIN);
    gpio_set_intr_type(FLOW_SENSOR, GPIO_INTR_POSEDGE);
	    //  Install ISR service first!
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);  // Once per application
	// gpio_install_isr_service();

    gpio_isr_handler_add(FLOW_SENSOR, flow_sensor_isr_handler, (void *)FLOW_SENSOR);


	/* init Switch GPIO */
	 gpio_set_direction(FLOW_SENSOR, GPIO_MODE_INPUT);	
	 gpio_set_direction(on_board_sw1, GPIO_MODE_INPUT);
	 gpio_set_direction(on_board_sw2, GPIO_MODE_INPUT);

	 gpio_set_pull_mode(on_board_sw1, GPIO_PULLUP_ONLY);
	 gpio_set_direction(SW1, GPIO_MODE_INPUT);
	 gpio_set_direction(SW2, GPIO_MODE_INPUT);

	 /* Buzzer GPIO init*/
	gpio_reset_pin(BUZZER_GPIO);
	gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);



}



void flow_task(void* pvParameters) {
    uint32_t notificationValue;

    while (1) {
        // Wait for trigger from timer task
        xTaskNotifyWait(0x00, 0xFFFFFFFF, &notificationValue, portMAX_DELAY);

        flow_pulse_count = 0;
		flow_rate_lpm = 0;
		flow_counting = true;

        // Enable interrupt
        gpio_isr_handler_add(FLOW_SENSOR, flow_sensor_isr_handler, NULL);

        vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds

        // Disable interrupt
        gpio_isr_handler_remove(FLOW_SENSOR);

        uint32_t pulses;
        portENTER_CRITICAL(&flowMux);
        pulses = flow_pulse_count;
        portEXIT_CRITICAL(&flowMux);

        // YF-S401 formula (typical): 
        // flow (L/min) = pulses ( Hz) / 7.5      ,  As pulses are counted for 5 sec divide pulse by 5 
        flow_rate_lpm = ((float) pulses / ( 5.0f * 7.5f)); 
		flow_counting = false;
        ESP_LOGI("FLOW", "Pulses: %ld, Flow Rate: %.2f L/min", pulses, flow_rate_lpm);

        // You can send this value to another task or store it
    }
}



void tmper_task(void* pvParameters) {
    uint32_t notificationValue;

    while (1) {
        // Wait for trigger from timer task
        xTaskNotifyWait(0x00, 0xFFFFFFFF, &notificationValue, portMAX_DELAY);
		temperature = 0.0;
		temp_checking = true;
		temperature = get_temp_ds18x20();
		temp_checking = false;

        ESP_LOGI("Tmeperature", "Temperature: %f", temperature);

        // You can send this value to another task or store it
    }
}

// esp_err_t gsm_command_exe()
// {


// 		gsm_err = 

// 		return ESP_OK;
// }



esp_err_t gsm_command_exe()
{

		/* GSM command execution start */
		gsm_cmd_checking = true;

		char imei[20];
		esp_err_t gsm_err = ESP_OK;

		gsm_err = gsm_init(imei);

		if( gsm_err == ESP_OK )
		{
			if( imei[0] != '\0')
			{
				if( strcmp(imei, config.IMEI_num))
				{
					strcpy (config.IMEI_num, imei);
					printf("New IMEI :%s\n",config.IMEI_num);
					save_device_config_to_nvs(&config);
					load_device_config_from_nvs(&config);
					print_device_config(&config);
				}
			}

			// ESP_LOGI("GSM", "Stack high watermark: %d", uxTaskGetStackHighWaterMark(NULL));
			
			int sig_strength = gsm_signal_strength();
			ESP_LOGI("GSM", "Signal Strength: %d", sig_strength);
			// if( sig_strength != 0)
			{
				status.gsm_signal_strength = sig_strength;
			}

			
			if ( strstr(config.gsm_sim_name,"JIO") )
			{
				if( start_internet(0) == ESP_FAIL )
					gsm_err = ESP_FAIL;
			}
			else
			{
				if( start_internet(1) == ESP_FAIL )
					gsm_err = ESP_FAIL;
			}
			
			if( gsm_err == ESP_OK)
			{
				// ESP_LOGI("GSM", "Stack high watermark: %d", uxTaskGetStackHighWaterMark(NULL));
				gsm_err = configurl();

				// // gsm_post_data("Hello !");


			}
		}
		else
		{
			ESP_LOGE("GSM", "Aborting further commands");
		}

		/* GSM command execution end */
		if( gsm_err == ESP_OK)
		{
			ESP_LOGI("GSM", "Executed init ");
		}
		else
		{
			 ESP_LOGI("GSM", "Init Execution failed");
		}
       
		/* GPS Check */
		// if ( check_GPS_data || config.latitude == 0.0 || config.longitude == 0.0)
		// if(  get_gps_location(&latitude, &longitude) == ESP_OK  )
		// {
		// 	ESP_LOGI("GPS data","Fetch Success");
		// }

		gsm_cmd_checking = false;
        
		return gsm_err;
    // }
}

static esp_adc_cal_characteristics_t adc_chars;

void adc_init_custom() {
    adc2_config_channel_atten(ADC_CH_1, ADC_ATTEN_DB);
    adc2_config_channel_atten(ADC_CH_2, ADC_ATTEN_DB);

    esp_adc_cal_characterize(ADC_UNIT_2, ADC_ATTEN_DB, ADC_WIDTH_BIT, ADC_VREF, &adc_chars);
}


esp_err_t read_two_adc_voltages(float *voltage1, float *voltage2) {
    int raw1 = 0, raw2 = 0;

    if (adc2_get_raw(ADC_CH_1, ADC_WIDTH_BIT, &raw1) != ESP_OK) return ESP_FAIL;
    if (adc2_get_raw(ADC_CH_2, ADC_WIDTH_BIT, &raw2) != ESP_OK) return ESP_FAIL;

    uint32_t mv1 = esp_adc_cal_raw_to_voltage(raw1, &adc_chars);
    uint32_t mv2 = esp_adc_cal_raw_to_voltage(raw2, &adc_chars);

    *voltage1 = mv1 / 1000.0f;
    *voltage2 = mv2 / 1000.0f;
    return ESP_OK;
}

void adc_sampling_task(void *pvParameters) 
{
	uint32_t notificationValue;

    while (1) 
	{
		xTaskNotifyWait(0x00, 0xFFFFFFFF, &notificationValue, portMAX_DELAY);
		ESP_LOGI("ADC_samp", "Sampling started...");
		adc_sampling = true;

		TickType_t start_tick = xTaskGetTickCount();
		float sum1 = 0.0f, sum2 = 0.0f;
		int samples = 0;

		while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(5000)) {
			float v1, v2;
			if (read_two_adc_voltages(&v1, &v2) == ESP_OK) {
				sum1 += v1;
				sum2 += v2;
				samples++;
			}
			vTaskDelay(pdMS_TO_TICKS(100));
		}

		if (samples > 0) {
			sampled_voltage1 = sum1 / samples;
			sampled_voltage2 = sum2 / samples;
		}

		adc_sampling = false;
		ESP_LOGI("ADC_samp", "Sampling done: V1=%.3f V, V2=%.3f V", sampled_voltage1, sampled_voltage2);
	}
}

// Float version of map function
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float BatteryPercentage(float voltage)
{
    if (is_bat_15V)
    {
        // 15V
        float actualVolt;
        actualVolt = (53.2 * voltage) / 2.2;
        float percentage = mapFloat(actualVolt, 10, 16.8, 0, 100);
        // Clamp percentage
        if (percentage < 0)
        {
            // percentage = 0;
            return 0;
        }
        else if (percentage > 100)
        {
            // percentage = 100;
            return 100;
        }
        else
        {
            return percentage;
        }
    }
    else
    {
        // 12v
        float actualVolt;
        actualVolt = (53.2 * voltage) / 2.2;
        float percentage = mapFloat(actualVolt, 10, 13.6, 0, 100);
        // Clamp percentage
        if (percentage < 0)
        {
            // percentage = 0;
            return 0;
        }
        else if (percentage > 100)
        {
            // percentage = 100;
            return 100;
        }
        else
        {
            return percentage;
        }
    }
}

float SOLARPercentage(float voltage) {
  float actualVolt;
  actualVolt = (53.2 * voltage) / 2.2;
  float percentage = mapFloat(actualVolt, 10,24, 0, 100);
  // Clamp percentage
  if (percentage < 0) {
    // percentage = 0;
    return 0;
  } else if (percentage > 100) {
    // percentage = 100;
    return 100;
  } else {
    return percentage; 
  }
}


uint64_t millis() {
    return esp_timer_get_time() / 1000; // convert microseconds → ms
}

void update_and_print_time()
{
	struct tm current_time;
	readClock(&current_time);
	printf("Time: %02d:%02d:%02d %02d/%02d/%04d\n",
	current_time.tm_hour, current_time.tm_min, current_time.tm_sec,
	current_time.tm_mday, current_time.tm_mon + 1, current_time.tm_year + 1900);

	// strcpy(config->date_format, "DD-MM-YYYY");
	// strcpy(config->time_format, "HH:MM:SS");
	// strcpy(status->current_date, "2025-01-01");
	// strcpy(status->current_time, "10:05:06");

	// status.current_date, 
	strftime(status.current_date, sizeof(status.current_date), "%d-%m-%Y", &current_time);
	strftime(status.current_time, sizeof(status.current_time), "%H:%M:%S", &current_time);

	strftime(time_app_hms, sizeof(time_app_hms), "%H-%M-%S", &current_time);

	ESP_LOGI("POLL", "Date: %s, Time: %s", status.current_date, status.current_time);
	
}

void buzzer_beep(uint32_t on_ms, uint32_t off_ms, uint32_t repeat)
{
	printf("Buzzer Beep: ON for %ld ms, OFF for %ld ms, Repeat %ld times\n", on_ms, off_ms, repeat);

    for (uint32_t i = 0; i < repeat; i++)
    {
        gpio_set_level(BUZZER_GPIO, 1);   // buzzer ON
        vTaskDelay(pdMS_TO_TICKS(on_ms));

        gpio_set_level(BUZZER_GPIO, 0);   // buzzer OFF
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}



void app_main(void)
{
	
	// vTaskDelay(2000 / portTICK_PERIOD_MS);


	set_UART1_rx_frame_len(10);
	// set_UART2_rx_frame_len(10);

	/* UART Init */
	
	#ifdef DEBUG == DEFAULT
	UART1_init();
	start_UART1_task();
	UART2_init();
	start_UART2_task();

	#elif DEBUG == LOG_AT_UART2
	UART0_init();
	UART1_init();
	start_UART0_task();
	start_UART1_task();

	#elif DEBUG == LOG_AT_UART1
	UART0_init();
	UART2_init();
	start_UART0_task();
	start_UART2_task();

	#elif DEBUG == DISABLE_LOGS
	UART0_init();
	UART1_init();
	UART2_init();
	start_UART0_task();
	start_UART1_task();
	start_UART2_task();
	#endif
	

	/*NVS Read Section */
	esp_err_t err = nvs_flash_init();
	
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
	    ESP_ERROR_CHECK(nvs_flash_erase());
	    err = nvs_flash_init();
	}
 
	ESP_ERROR_CHECK(err);
	
	// clear_sensor_nvs_namespace();
	// clear_device_status_nvs();
	// clear_device_config_nvs(); // CLEAR the sensor data if required.

	// // Try to load saved sensors, else set defaults
	// if ( load_sensors_from_nvs() != ESP_OK ) 
	// {
	//     // Set default values manually
	//     ESP_LOGW(TAG_NVS, "Using default sensor config");
	//     init_default_sensors();  // You define this function to populate sensors[]
	//     save_sensors_to_nvs();   // Save them for future
	// }
	// else
	// {
	// 	printf("NVS load Success \n");
	// }
	load_sensors_with_version_check();
	/*NVS Read Section Ends*/


	// delete_spiffs_file("/spiffs/data_logs.txt");
	// delete_log_file();
	// log_handler_init();



	gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);

	printf("Hello from app_main!\n");
	
	if (load_sensors_from_nvs() == ESP_OK) 
	{
    	// print_sensor_data();
	}
	

	
	if (load_device_status_from_nvs(&status) != ESP_OK) {
	    init_default_device_status(&status);
	    save_device_status_to_nvs(&status);
	}
	
	if (load_device_config_from_nvs(&config) != ESP_OK) {
	    init_default_device_config(&config);
	    save_device_config_to_nvs(&config);
	}



	if (load_device_status_from_nvs(&status) == ESP_OK) {
	    print_device_status(&status);
	}
	
	/* Force post status to FAIL on boot. The app will see FAIL initially. 
	   It will only flip to OK after an actual successful post to the server. */
	strcpy(status.gsm_post_status, "FAIL");
	
	if (load_device_config_from_nvs(&config) == ESP_OK) {
	    print_device_config(&config);
	}
	

	// #ifdef DEBUG == DEFAULT



	// #elif DEBUG == LOG_AT_UART2
 	// // esp_log_set_vprintf(uart_vprintf);

	// #elif DEBUG == LOG_AT_UART1
	// // esp_log_set_vprintf(uart_vprintf);

	// #elif DEBUG == DISABLE_LOGS
	// esp_log_level_set("*", ESP_LOG_NONE); // Disable all logs
	// #endif


	// ESP_LOGI("UART2", "UART2 initialized");

	rtc_init_();
	init_flow_buzz_gpio();
	init_shift_register();
	init_log_system();
	init_temperature_sensor();
	adc_init_custom();



	// format_spiffs();

	if( get_file_size() <= MIN_SPACE_FOR_LOG )
	{
		ESP_LOGI("MAIN", "Enough space available for logs file size %d", get_file_size());
		check_last_log_ind();
	}
	else
	{
		check_last_log_ind();
		size_t curr_offset = get_curr_log_offset();
		ESP_LOGW("MAIN", "Wrap around took place");
		ESP_LOGW("MAIN", "Current Log offset: %zu ", curr_offset);
	}

	ESP_LOGI("MAIN", "Log system initialized");
	size_t last_log_off = get_curr_log_offset();
	size_t failed_offset = get_failed_offset();
	bool failed_flag = get_is_failed();
	bool post_wrap = get_post_wrap();

	if( ! failed_flag )
	{
		set_post_status(last_log_off, post_wrap, failed_flag);
	}

	ESP_LOGE("MAIN", "last log offset: %d, failed offset: %d, failed flag: %d", last_log_off, failed_offset, failed_flag);
	
	BLE_INT_QUEUE = xQueueCreate(1, sizeof(int));    // queue length = 1
    BLE_INT_POLL_PAUSED = xSemaphoreCreateBinary();
    BLE_INT_BLE_DONE = xSemaphoreCreateBinary();

	xTaskCreatePinnedToCore(btn_press_handler, "btn_press_handler", 8192, NULL, 5, NULL, 1);
	xTaskCreate(flow_task, "FlowTask", 2048, NULL, 5, &flow_task_handle);
	xTaskCreate(tmper_task, "TemperatureTask", 4096, NULL, 5, &tempr_task_handle);
	// xTaskCreate(gsm_task, "GsmTask", 4096, NULL, 5, &gsm_task_handle);
	xTaskCreate(adc_sampling_task, "adc_sampling_task", 4096, NULL, 5, &adc_sampling_handle);
	// xTaskCreate(backlog_post_task, "backlog_post_task", 6144, NULL, 5, &backlog_post_task_handle);
	xTaskCreatePinnedToCore(backlog_post_task, "backlog_post_task", 6144, NULL, 5, &backlog_post_task_handle, 1);
	//                                                                                                          ^
	//                                              app_main runs on core 0 by default, post task on core 1

	update_and_print_time();
	unix_timestamp = convert_to_unix(status.current_date,  status.current_time);
    
	if (unix_timestamp != -1) {
        printf("Unix Timestamp: %lld\n", unix_timestamp);
    } else {
        printf("Invalid date/time format!\n");
		unix_timestamp = 1754894220;
    }

	generate_compact_summary_string();
	printf("Compact Summary: %s\n", compact_summary_str);

	write_SR(GSM_en, LOW);
	vTaskDelay(2000 / portTICK_PERIOD_MS);
	write_SR(GSM_en, HIGH);

	write_SR(IO_12V, HIGH);

	write_SR(LED, LOW);
	
	ESP_LOGW("APP","Waiting delay for GSM stability");
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	
	bool sim;
	if ( strstr(config.gsm_sim_name,"JIO") )
	{
		sim = 0;
	}
	else
	{
		sim = 1;
	}

	int ota_status = false;
	bool skip_ota = true;
	get_ota_checked_status(&ota_status);

	if (strcmp(config.server_name, "ACOM_NEW") == 0) 
	{
		printf("The Device name is not set by USER, skipping OTA check!\n");
		skip_ota = false;
	}


	  // --- MODIFIED SECTION START ---
    if( skip_ota && ota_status == false)
    {
        ESP_LOGW("MAIN", "Checking for OTA updates...");
        
        if( config.wifi_enabled_gsm_disabled == true)
        {
            ESP_LOGW("MAIN", "WiFi Enabled & GSM Disabled mode active");
            
            bool wifi_connected = false;

            // Check if already connected
            if( wifi_is_connected() )
            {
                wifi_connected = true;
            }
            else
            {
                ESP_LOGW("MAIN", "Connecting to WiFi for OTA...");
                
                // Retry Loop: Try to connect 3 times
                for(int i = 0; i < 3; i++)
                {
                    if( wifi_handler_init((uint8_t *)config.wifi_ssid, (uint8_t *)config.wifi_password) == ESP_OK)
                    {
                        // Wait briefly to ensure IP is assigned
                        vTaskDelay(pdMS_TO_TICKS(2000)); 
                        
                        if(wifi_is_connected()) {
                            ESP_LOGW("MAIN", "WiFi Connected Successfully (Attempt %d)", i+1);
                            wifi_connected = true;
                            break;
                        }
                    }
                    else
                    {
                        ESP_LOGE("MAIN", "WiFi Connection Attempt %d Failed, Retrying...", i+1);
                        vTaskDelay(pdMS_TO_TICKS(3000)); // Wait 3 seconds before retrying
                    }
                }
            }

            if(wifi_connected)
            {
                start_ota_wifi();
            }
            else
            {
                ESP_LOGE("MAIN", "Could not connect to WiFi for OTA after multiple attempts.");
            }
        }
        else
        {
            ESP_LOGW("MAIN", "GSM Enabled mode active");
            // Ensure GSM has enough time to stabilize
            vTaskDelay(pdMS_TO_TICKS(7000)); 
            check_fw_update(sim);
        }

        // Set status to true so we don't loop OTA checks forever if they fail
        set_ota_checked_status(true);
        
        // Restart is required to clear memory/state after OTA attempt 
        // or to enter normal mode cleanly
        esp_restart(); 
    }
    else
    {
        set_ota_checked_status(false); // Reset for next day
        printf("OTA checked in previous boot or skipped!\n");
    }
    // --- MODIFIED SECTION END ---


	if (backlog_post_task_handle) {
		/* Kick once at boot AFTER OTA check is done so any pre-existing backlog starts draining safely. */
		xTaskNotifyGive(backlog_post_task_handle);
	}

	write_SR(Lamp, HIGH);
	buzzer_beep(500,0,1);
	                 
	uint8_t state = 0;
	
	// start_delay();
	uint64_t start = millis();
	uint32_t delay_ms = config.data_frequency_sec * 1000;  // e.g. 60 000 ms
	if (delay_ms == 0) delay_ms = 60000;

	ESP_LOGW("MAIN", " Data Frequency set to: %ld ms", delay_ms);

	uint32_t pre_trigger_ms = 1000;
	bool pre_done  = false;
	bool first_run = true;

	static int logss_count = 0;

	while (1)
	{	/* ── AP mode: idle everything, wait for download + restart ──────── */
		if (ap_mode_active) {
			if (ap_download_done) {
				ESP_LOGW("MAIN-AP", "Download finished — restarting in 2s");
				vTaskDelay(pdMS_TO_TICKS(2000));
				esp_restart();
			}
			vTaskDelay(pdMS_TO_TICKS(200));
			continue;
		}
		uint64_t now     = millis();
		uint64_t elapsed = now - start;

		/* ── Detect BLE connect / disconnect transitions ─────────────────── */
		bool ble_now = is_ble_connected_();

		if (ble_now && !ble_mode_active) {
			/* BLE just connected — enter fast-log mode.
			   Save how much of the normal interval has already elapsed so we
			   can resume the countdown seamlessly on disconnect. */
			ble_mode_active      = true;
			ble_last_log_ms      = now - BLE_LOG_INTERVAL_MS; /* fire first tick immediately */
			normal_elapsed_saved = (elapsed < delay_ms) ? (uint32_t)elapsed : delay_ms;
			ESP_LOGW("MAIN-BLE", "BLE connected — entering fast-log mode (%d s). "
			         "Normal timer paused at %lu ms elapsed.",
			         BLE_LOG_INTERVAL_SEC, (unsigned long)normal_elapsed_saved);
		}

		if (!ble_now && ble_mode_active) {
			/* BLE just disconnected — leave fast-log mode.
			   Restore the normal interval timer from where it was paused.
			   Re-anchor scheduler to real RTC so next scheduled DT is correct. */
			ble_mode_active        = false;
			start                  = now - normal_elapsed_saved;
			scheduled_log_unix     = 0;
			scheduled_interval_sec = 0;
			ESP_LOGW("MAIN-BLE", "BLE disconnected — Restarting ESP. "
			         "Restored elapsed %lu ms.", (unsigned long)normal_elapsed_saved);
			esp_restart();
		}

		/* ── BLE fast-log path ───────────────────────────────────────────── */
		if (ble_mode_active && !ble_cmd_processing) {

			/* Process BLE commands on EVERY loop iteration — not just on log ticks.
			   This is why RMS saves settings on the first try: it never blocks BLE
			   for more than one loop cycle (~10ms). Without this, commands wait up
			   to BLE_LOG_INTERVAL_MS (10s) before the semaphore is released. */
			CHECK_BLE_INTERRUPT();

			uint64_t ble_elapsed = now - ble_last_log_ms;

			if (ble_elapsed >= BLE_LOG_INTERVAL_MS) {
				ble_last_log_ms = now;

				ESP_LOGW("MAIN-BLE", "BLE fast-log tick (every %d s)", BLE_LOG_INTERVAL_SEC);

				xTaskNotify(adc_sampling_handle, 0, eNoAction);
				xTaskNotify(flow_task_handle,    0, eNoAction);
				xTaskNotify(tempr_task_handle,   0, eNoAction);

				// CHECK_BLE_INTERRUPT();

				collect_and_log();

				if (backlog_post_task_handle) {
					xTaskNotifyGive(backlog_post_task_handle);
				}

				CHECK_BLE_INTERRUPT();
				get_sensor_data();
				status_notify_chunks(all_sensor_info);   ESP_LOGB("MAIN-BLE", "NOTIFIED SENSOR DATA");
				get_device_status();
				status_notify_chunks(device_status_str); ESP_LOGB("MAIN-BLE", "NOTIFIED DEVICE STATUS");
				get_device_config();
				status_notify_chunks(device_config_str); ESP_LOGB("MAIN-BLE", "NOTIFIED DEVICE CONFIG");

			} else {
				/* Between log ticks — push a lightweight time update every ~2s
				   so the app clock ticks in real time (matches RMS behaviour). */
				static uint64_t last_time_push_ms = 0;
				if ((now - last_time_push_ms) >= 2000) {
					last_time_push_ms = now;
					get_device_status();
					status_notify_chunks(device_status_str);
				}
			}

			vTaskDelay(10 / portTICK_PERIOD_MS);
			continue;   /* skip the normal-mode block while BLE is active */
		}

		/* ── Normal mode path (no BLE) ──────────────────────────────────── */
		if ((!pre_done && elapsed >= (delay_ms - pre_trigger_ms)) || first_run) {
			pre_done = true;
		}

		if ((elapsed >= delay_ms || first_run) && pre_done && !ble_cmd_processing) {
			ESP_LOGW("MAIN", "\n✅ Main delay done at %llu ms\n", elapsed);

			/* Anchor interval timer BEFORE collect_and_log */
			start     = now;
			pre_done  = false;
			first_run = false;

			xTaskNotify(adc_sampling_handle, 0, eNoAction);
			xTaskNotify(flow_task_handle,    0, eNoAction);
			xTaskNotify(tempr_task_handle,   0, eNoAction);
			printf(" \n");

			CHECK_BLE_INTERRUPT();

			/* STEP 1: Collect + SPIFFS write (uses scheduled DT, ~15 s) */
		collect_and_log();

		/* STEP 2: Kick async post worker */
		if (backlog_post_task_handle) {
			xTaskNotifyGive(backlog_post_task_handle);
		}

		/* STEP 3: Push fresh data to BLE app if connected */
		if (is_ble_connected_()) {
			CHECK_BLE_INTERRUPT();
			get_sensor_data();
			status_notify_chunks(all_sensor_info);
			ESP_LOGB("MAIN-BLE", "NOTIFIED SENSOR DATA");
			get_device_status();
			status_notify_chunks(device_status_str);
			ESP_LOGB("MAIN-BLE", "NOTIFIED DEVICE STATUS");
			get_device_config();
			status_notify_chunks(device_config_str);
			ESP_LOGB("MAIN-BLE", "NOTIFIED DEVICE CONFIG");
		}
	}

	vTaskDelay(10 / portTICK_PERIOD_MS);

	/* Shrink log interval to 5s when BLE connected, restore when disconnected */
	if (is_ble_connected_() && delay_ms > 5000) {
		delay_ms = 5000;
	} else if (!is_ble_connected_() && delay_ms < (uint32_t)(config.data_frequency_sec * 1000)) {
		delay_ms = config.data_frequency_sec * 1000;
	}
}

	ESP_LOGI("MAIN", "Exiting main loop");
	print_posted_logs();

}