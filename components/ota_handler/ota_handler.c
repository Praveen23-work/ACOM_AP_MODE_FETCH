#include <stdio.h>
#include "ota_handler.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "support.h"

#include "esp_ota_ops.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"  // for esp_timer_get_time()

#include "nvs.h"
#include "nvs_flash.h"

#include "UART_HANDLER.h"
#include "GSM_handler.h"
#include "sensor_config.h"

#define STORAGE_NAMESPACE "ota_storage"

#define BLINK_INTERVAL 1000 // 1 second

// uint8_t *UART1_data = NULL;
// uint8_t *UART2_data = NULL;
uint8_t *uart_data_rxd___ = NULL;

// bool UART1_rx_done = false;
// bool UART2_rx_done = false;
size_t total_ota_received = 0;   // Track total OTA bytes

const char* new_fw_ver;

volatile bool ota_rx_success = false;

QueueHandle_t ota_queue;
esp_ota_handle_t ota_handle;

int RXD_size = 0;
bool OTA_active = false;


uint32_t expected_ota_file_size = 0; // Set this when starting OTA

TaskHandle_t ota_task_Handle;

esp_err_t create_config_file(const char *device_id, const char *device_name, const char* fr_version);
esp_err_t delete_config_file(const char *device_id);
DeviceConfig device_config;

/*****************************/
/*  UART RELATED FUNCTIONS   */
/*****************************/


static esp_err_t uart_write_modem_ftp(const char *cmd, const char *resp, int timeout)
{
    memset(UART2_data, 0, RX_BUF_SIZE + 1);
     memset(uart_data_rxd___, 0, RX_BUF_SIZE + 1);

    uart_write_bytes(UART_PORT, cmd, strlen(cmd));
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100)); // ensure full send

    if (get_UART2_data(uart_data_rxd___, timeout, resp) == ESP_OK) {
        return ESP_OK;
    } else {

        return ESP_FAIL;
    }

}


/********************************/
/*  UART RELATED FUNCTIONS END */
/******************************/


static esp_err_t process_json_response(const char* device_id, const char *device_name, const char *current_version)
{
    const char *expected_site_id = device_name;
    const char *expected_fw_ver = current_version;
    bool config_file_update = false;

    if (uart_data_rxd___ == NULL || strlen( (const char *) uart_data_rxd___) == 0) {
        ESP_LOGE("JSON", "Empty JSON string");
        return ESP_FAIL;
    }

    printf("Processing JSON: %s\n", uart_data_rxd___);

    // Temporary buffers for extracted strings
    char site_id[64] = {0};
    char fw_ver[64] = {0};

    // Extract SITE_ID
    const char *ptr = strstr((const char *) uart_data_rxd___, "\"SITE_ID\"");
    if (ptr) {
        // Move to ':' and skip spaces
        ptr = strchr(ptr, ':');
        if (ptr) {
            ptr++; // skip ':'
            while (*ptr == ' ' || *ptr == '\"') ptr++; // skip spaces or quote
            int i = 0;
            while (*ptr && *ptr != '\"' && i < (int)(sizeof(site_id)-1)) {
                site_id[i++] = *ptr++;
            }
            site_id[i] = '\0';
        }
    }

    if (strlen(site_id) == 0) {
        ESP_LOGE("JSON", "SITE_ID missing in JSON");
        config_file_update = true;
    } else {
        if (strcmp(site_id, expected_site_id) != 0) {
            ESP_LOGW("JSON", "SITE_ID mismatch. Expected: %s, Got: %s", expected_site_id, site_id);
            config_file_update = true;
        } else {
            ESP_LOGI("JSON", "SITE_ID matches: %s", site_id);
        }
    }

    // Extract FIRMWARE_VERSION
    ptr = strstr( (const char *) uart_data_rxd___, "\"FIRMWARE_VERSION\"");
    if (ptr) {
        ptr = strchr(ptr, ':');
        if (ptr) {
            ptr++; // skip ':'
            while (*ptr == ' ' || *ptr == '\"') ptr++;
            int i = 0;
            while (*ptr && *ptr != '\"' && i < (int)(sizeof(fw_ver)-1)) {
                fw_ver[i++] = *ptr++;
            }
            fw_ver[i] = '\0';
        }
    }

    if (strlen(fw_ver) == 0) {
        ESP_LOGE("JSON", "FIRMWARE_VERSION missing in JSON");
         config_file_update = true;
        // return ESP_FAIL;
    } else {
        if (strcmp(fw_ver, expected_fw_ver) != 0) {
            ESP_LOGW("JSON", "FIRMWARE_VERSION mismatch. Expected: %s, Got: %s", expected_fw_ver, fw_ver);
            new_fw_ver = strdup(fw_ver); // store new version
            config_file_update = false; // Will update ConfigFile after OTA update
        } else {
            ESP_LOGI("JSON", "FIRMWARE_VERSION matches: %s", fw_ver);
            // return ESP_FAIL;
        }
    }

    if (config_file_update) // Updates the file when Site ID mismatch but Firmware version is similar
    {
        // First delete the file
        ESP_LOGI("JSON", "Updating config file due to SITE_ID mismatch");
        if( delete_config_file(device_id) == ESP_FAIL ) return ESP_FAIL;

        // Then create a new one
        create_config_file(device_id, device_name, current_version);
    }

    return ESP_OK;
}


int ftp_get_file_size(const char *filename)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+QFTPSIZE=\"%s\"\r", filename);

    if ( uart_write_modem_ftp(cmd, "+QFTPSIZE:", 5000) == ESP_FAIL ) {
        printf("Failed to send QFTPSIZE command\n");
        return -1;
    }
    

    char *ptr = strstr((const char *) UART2_data, "+QFTPSIZE: 0,");
    if (ptr) {
        int size = atoi(ptr + strlen("+QFTPSIZE: 0,"));
        printf("FTP file size = %d bytes\n", size);
        return size;
    } else {
        printf("Failed to get file size\n");
        return -1;
    }
}


/*****************************/
/*  GSM RELATED FUNCTIONS   */
/*****************************/

esp_err_t gsm_init_ftp(void)
{
    if(uart_data_rxd___ == NULL )
    {
        uart_data_rxd___ = (uint8_t *) malloc(RX_BUF_SIZE + 1);
    }

    if ( uart_write_modem_ftp("AT\r\n", "OK", 1500) == ESP_FAIL )
    {
        ESP_LOGE("gsm_init_ftp", "AT - MODEM not responding\n");
        return ESP_FAIL;
    }

    uart_write_modem_ftp("AT+CFUN?\r\n",  "OK", 1500);
    uart_write_modem_ftp("AT+CMEE=1\r\n", "OK", 1500);
    uart_write_modem_ftp("AT+GSN\r\n",    "OK", 1500);
    uart_write_modem_ftp("AT+CPIN?\r\n",  "OK", 1500);

    /* FIX 2: Wait for network registration before returning.
       Without this, start_internet_ftp() fires AT+QIACT before the modem
       is on the network after a cold reset, causing intermittent failures. */
    ESP_LOGW("GSM_NET_FTP", "Waiting for network registration...");
    bool registered = false;
    for (int i = 0; i < 8; i++) {
        if (uart_write_modem_ftp("AT+CREG?\r\n", "+CREG: 0,1", 1000) == ESP_OK ||
            uart_write_modem_ftp("AT+CREG?\r\n", "+CREG: 0,5", 1000) == ESP_OK) {
            registered = true;
            ESP_LOGW("GSM_NET_FTP", "Network registered after %d s", (i + 1) * 1);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (!registered) {
        ESP_LOGE("GSM_NET_FTP", "Network registration timeout — no network");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t start_internet_http(bool sim)
{
    // APN setup
    if (sim) {
        uart_write_modem("AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\",0\r\n", "OK", 3000 );
    } else {
        uart_write_modem("AT+QICSGP=1,1,\"jionet\",\"\",\"\",0\r\n", "OK", 3000);
    }
    // MODEM_DELAY(3000);

    uart_write_modem("AT+QIACT=1\r\n","OK", 1500);
    // MODEM_DELAY(2000);

    if ( uart_write_modem("AT+QIACT?\r\n", "+QIACT: 1,1,1", 5000) == ESP_FAIL )
        return ESP_FAIL;
    // MODEM_DELAY(3000);

    // uart_write_modem("AT+QHTTPCFG=\"contextid\",1\r\n", "OK", 1500);
    // MODEM_DELAY(1500);

    uart_write_modem("AT+QHTTPCFG=\"requestheader\",0\r\n", "OK", 1000);
    // MODEM_DELAY(500);

    uart_write_modem("AT+QHTTPCFG=\"responseheader\",1\r\n", "OK", 1000);
    // MODEM_DELAY(500);

    uart_write_modem("AT+QHTTPCFG=\"sslctxid\",1\r\n", "OK", 1000);
    // MODEM_DELAY(500);

    uart_write_modem("AT+QSSLCFG=\"sslversion\",1,4\r\n", "OK", 1000);
    // MODEM_DELAY(1000);

    uart_write_modem("AT+QSSLCFG=\"seclevel\",1,0\r\n", "OK", 1000);
    // MODEM_DELAY(1000);

    uart_write_modem("AT+QHTTPCFG=\"contenttype\",1\r\n", "OK", 1000);
    // MODEM_DELAY(2000);

    uart_write_modem("AT+QHTTPCFG=\"contextid\",1\r\n", "OK", 1500);
    // MODEM_DELAY(1500);

    uart_write_modem("AT+QHTTPCFG?\r\n", "OK", 1500);


    return ESP_OK;
}

esp_err_t start_internet_ftp(bool sim)
{
    // APN setup
    /* FIX : Airtel auth must be 1 (PAP) — same as start_internet() in GSM_handler.
       auth=0 causes unstable IP lease on Airtel, leading to intermittent failures. */
    if (sim) {
        uart_write_modem_ftp("AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\",1\r\n", "OK", 3000 );
    } else {
        uart_write_modem_ftp("AT+QICSGP=1,1,\"jionet\",\"\",\"\",0\r\n", "OK", 3000);
    }
    delay_ms(3000);

    uart_write_modem_ftp("AT+QIACT=1\r\n","OK", 1500);
    if ( uart_write_modem_ftp("AT+QIACT?\r\n", "+QIACT: 1,1,1", 5000) == ESP_FAIL )
        return ESP_FAIL;

    // uart_write_modem_ftp("AT+QPING=1,\"www.google.com\"\r\n","++QPING:", 5000);

    // uart_write_modem_ftp("AT+QHTTPCFG=\"requestheader\",0\r\n", "OK", 1000);
    // uart_write_modem_ftp("AT+QHTTPCFG=\"responseheader\",1\r\n", "OK", 1000);
    // uart_write_modem_ftp("AT+QHTTPCFG=\"sslctxid\",1\r\n", "OK", 1000);
    // uart_write_modem_ftp("AT+QSSLCFG=\"sslversion\",1,4\r\n", "OK", 1000);
    // uart_write_modem_ftp("AT+QSSLCFG=\"seclevel\",1,0\r\n", "OK", 1000);
    // uart_write_modem_ftp("AT+QHTTPCFG=\"contenttype\",1\r\n", "OK", 1000);
    // uart_write_modem_ftp("AT+QHTTPCFG=\"contextid\",1\r\n", "OK", 1500);
    // uart_write_modem_ftp("AT+QHTTPCFG?\r\n", "OK", 1500);

    return ESP_OK;
}

esp_err_t ftp_setup(void)
{
    uart_write_modem_ftp("AT+QFTPCFG=\"contextid\",1\r", "OK", 1500);
    // uart_write_modem_ftp("AT+QFTPCFG=\"account\",\"tushar_cyamsys\",\"Cyamsys@ota1234\"\r", "OK", 1500);
    uart_write_modem_ftp("AT+QFTPCFG=\"account\",\"ftpuser\",\"cyamsysFTP\"\r", "OK", 1500);
    uart_write_modem_ftp("AT+QFTPCFG=\"filetype\",1\r", "OK", 1500);
    uart_write_modem_ftp("AT+QFTPCFG=\"transmode\",1\r", "OK", 1500);
    uart_write_modem_ftp("AT+QFTPCFG=\"rsptimeout\",90\r", "OK", 1500);

    // uart_write_modem_ftp("AT+QFTPCFG=\"passive\",1\r", "OK", 3000);
    
    return ESP_OK;
}

esp_err_t ftp_open(void)
{
    /* Close any stale session first — ignore result intentionally.
       If no session exists the modem returns ERROR, which is fine. */
    uart_write_modem_ftp("AT+QFTPCLOSE\r", "OK", 3000);
    delay_ms(500);

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        ESP_LOGI("FTP", "FTP open attempt %d/3", attempt);

        if (uart_write_modem_ftp("AT+QFTPOPEN=\"13.201.57.30\",21\r",
                                  "+QFTPOPEN: 0,0", 12000) == ESP_FAIL)
        {
            ESP_LOGW("FTP", "QFTPOPEN attempt %d failed, retrying...", attempt);
            delay_ms(2000);
            continue;
        }

        /* +QFTPOPEN: 0,0 received — verify the session is alive immediately
           before returning. The TCP connection can drop within ms of opening
           due to network instability, causing 625 on all subsequent commands. */
        delay_ms(500);
        if (uart_write_modem_ftp("AT+QFTPCWD=\"/\"\r", "+QFTPCWD:", 5000) == ESP_OK)
        {
            ESP_LOGI("FTP", "FTP session open and verified on attempt %d", attempt);
            return ESP_OK;  /* Already at root — callers need not call ftp_go_to_root_folder() again */
        }

        ESP_LOGW("FTP", "FTP session opened but dropped immediately (625) — attempt %d, retrying...", attempt);
        delay_ms(2000);
    }

    ESP_LOGE("FTP", "FTP open failed after 3 attempts");
    return ESP_FAIL;
}

esp_err_t ftp_close(void)
{
    // AT command to close the FTP service
    // Expected URC response on success is typically "+QFTPCLOSE: 0,0" or just "OK"
    uart_write_modem_ftp("AT+QFTPCLOSE\r", "OK", 5000); 
    delay_ms(1000);
    return ESP_OK;
}

esp_err_t ftp_go_to_root_folder(void)
{
    uart_write_modem_ftp("AT+QFTPCWD=\"/\"\r" , "+QFTPCWD:", 5000);
    return ESP_OK;
}

esp_err_t ftp_list(void)
{
    uart_write_modem_ftp("AT+QFTPLIST=\".\"\r", "+QFTPLIST:", 5000);
    return ESP_OK;
}

ota_result_t get_config_file(const char* device_id)
{
    ftp_go_to_root_folder();
    delay_ms(1000);

    char cmd[64];  // big enough to hold your full command
    
    snprintf(cmd, sizeof(cmd), "AT+QFTPGET=\"%s.json\",\"COM:\",0\r", device_id);
    
    ftp_list();
    delay_ms(2000);

    uart_write_modem_ftp("AT+QFTPCWD=\"ACOM_OTA\"\r", "+QFTPCWD:", 3000);
    delay_ms(1000);
    uart_write_modem_ftp("AT+QFTPCWD=\"ConfigFiles\"\r", "+QFTPCWD:", 3000);
    delay_ms(1000);

    ftp_list();
    delay_ms(2000);

    if( uart_write_modem_ftp(cmd, "}", 10000) == ESP_FAIL )
        return FTP_FILE_NOT_FOUND;
    delay_ms(1000);

    return FTP_FETCH_OK;
}

esp_err_t delete_config_file(const char *device_id)
{
    printf("----Deleting Config File----\n");

    ftp_go_to_root_folder();
    delay_ms(1000);
    
    uart_write_modem_ftp("AT+QFTPCWD=\"ACOM_OTA\"\r", "+QFTPCWD:", 3000);
    delay_ms(1000);
    uart_write_modem_ftp("AT+QFTPCWD=\"ConfigFiles\"\r", "+QFTPCWD:", 3000);
    delay_ms(1000);

    char cmd[64];  // big enough to hold your full command
    snprintf(cmd, sizeof(cmd), "AT+QFTPDEL=\"%s.json\"\r", device_id);
    if( uart_write_modem_ftp(cmd, "OK", 5000) == ESP_FAIL ) return ESP_FAIL;

    ftp_list();
    delay_ms(2000);

    return ESP_OK;
}

esp_err_t create_config_file(const char *device_id, const char *device_name, const char* fr_version)
{
    printf("----Creating New Config File----\n");

    char cmd[64];  // big enough to hold your full command
    snprintf(cmd, sizeof(cmd), "AT+QFTPPUT=\"%s.json\",\"COM:\",0\r", device_id);

    char json_buf[256];
    snprintf(json_buf, sizeof(json_buf),
            "{\r\n"
            "  \"DEVICE_ID\": \"%s\",\r\n"
            "  \"SITE_ID\": \"%s\",\r\n"
            "  \"FIRMWARE_VERSION\": \"%s\"\r\n"
            "}\r\n",
            device_id, device_name, fr_version);

    ftp_go_to_root_folder();
    delay_ms(1000);

    uart_write_modem_ftp("AT+QFTPCWD=\"ACOM_OTA\"\r", "+QFTPCWD:", 3000);
     delay_ms(1000);
    uart_write_modem_ftp("AT+QFTPCWD=\"ConfigFiles\"\r", "+QFTPCWD:", 3000);
     delay_ms(1000);

    ftp_list();
    delay_ms(2000);

    uart_write_modem_ftp(cmd, "CONNECT", 7000);     // Create and open file
    if( uart_write_modem_ftp(json_buf, "+QFTPPUT: 609,0", 30000) != ESP_OK)                                // Write json content to the file
    return ESP_FAIL;
    // uart_write_modem_ftp("\x1A", "OK", 1500);                                   // Clt+z to close the file - Not necessary

    return ESP_OK;
}

esp_err_t start_ota(const char* new_fw_ver)
{
    ftp_go_to_root_folder();
    
    uart_write_modem_ftp("AT+QFTPCWD=\"ACOM_OTA\"\r", "+QFTPCWD:", 3000);
    delay_ms(1000);
    uart_write_modem_ftp("AT+QFTPCWD=\"FirmwareFiles\"\r", "+QFTPCWD:", 3000);
    delay_ms(1000);

    int file_size = ftp_get_file_size(new_fw_ver);
    if( file_size <= 0 ) return ESP_FAIL;

    expected_ota_file_size = file_size;


    char cmd[64];  // big enough to hold your full command
    snprintf(cmd, sizeof(cmd), "AT+QFTPGET=\"%s\",\"COM:\"\r", new_fw_ver);

    OTA_active = true; // Enable OTA mode

    printf("----Starting OTA for %s----\n", new_fw_ver);

    memset(UART2_data, 0, RX_BUF_SIZE + 1);
    memset(uart_data_rxd___, 0, RX_BUF_SIZE + 1);

    uart_write_bytes(UART_PORT, cmd, strlen(cmd));
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100)); // ensure full send

        /* Further OTA data processing is done at UART2_task & ota_task*/

    int64_t start_time = esp_timer_get_time(); // returns time in microseconds
    const int OTA_TIMEOUT_MS = 200000; // 200 seconds timeout
    int64_t timeout_us = OTA_TIMEOUT_MS * 1000; // convert ms to us

    while (OTA_active) 
    {
        vTaskDelay(pdMS_TO_TICKS(100)); // 100 ms delay per iteration

        int64_t elapsed = esp_timer_get_time() - start_time;
        if (elapsed >= timeout_us) 
        {
            ESP_LOGW("OTA_WAIT", "OTA timed out after %d ms, total bytes received: %d", OTA_TIMEOUT_MS, total_ota_received);
            // optionally mark OTA failed
            OTA_active = false;
            return ESP_FAIL; // exit the loop / function
        }
    }

    // After data is processed by ota_task, check if Ending ota was success
    if( ota_rx_success == true)
    {
        ESP_LOGI("OTA_WAIT", "OTA RX completed successfully within timeout");
        return ESP_OK;
    }
    else
    {
        return ESP_FAIL;
    }

}

void MODEM_RESTART(void)
{
    write_SR(GSM_en, LOW);
    delay_ms(2000);
    write_SR(GSM_en, HIGH);
}

/*****************************/
/* GSM RELATED FUNCTIONS END */
/*****************************/



esp_err_t load_config_data()
{

    return ESP_OK;
}

const char cmd_url_ota[] = "http://13.201.57.30:8080/ACOM_OTA/ConfigFiles/\r\n";

/**
 * Configure the HTTP URL
 */
esp_err_t configurl_ota(void)
{    

    char temp_buff[50] = "";
    int str_len = strlen(cmd_url_ota) - 2;

    snprintf(temp_buff, sizeof(temp_buff), "AT+QHTTPURL=%d,60\r\n", str_len);

    if ( uart_write_modem(temp_buff, "CONNECT", 5000) == ESP_FAIL )
    {
       ESP_LOGE("configurl","Connect Not detected - aborting after URL len setup failure\n");
        return ESP_FAIL;
    }

    
    if ( uart_write_modem(cmd_url_ota, "CONNECT", 5000) == ESP_OK )
    {
        ESP_LOGE("configurl","Connect Not detected - aborting after URL failure\n");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t use_http_file_check()
{
    
    return ESP_OK;
}

const char* version_check(const char* device_id, const char* device_name, const char* current_version, int sim)
{
    printf("\n--------> Starting the version Check <---------\n");

    printf("\n---* GSM INIT *---\n");
    if(gsm_init_ftp() == ESP_FAIL)  return NULL;

    printf("\n---* INIT INTERNET *---\n");
    if(start_internet_ftp(sim) == ESP_FAIL)  return NULL;

    printf("\n---* FTP SETUP *---\n");
    if(ftp_setup() == ESP_FAIL) return NULL;

    printf("\n---* OPEN FTP *---\n");
    if(ftp_open() == ESP_FAIL) return NULL;

    printf("\n---* FETCH CONFIG FILEs *---\n");

    ota_result_t config_result = get_config_file(device_id);

    if(config_result == FTP_FILE_NOT_FOUND)
    {
        if(create_config_file(device_id, device_name, current_version) == ESP_FAIL) return NULL;
        ftp_close();
        return NULL;
    }
    else if(config_result == FTP_FETCH_OK)
    {
        /* FIX : Was `else if(FTP_FETCH_OK)` which is always true (enum value = 2).
           Now correctly checks the actual return value of get_config_file().
           Previously, even a failed FTP fetch (FTP_FETCH_FAILED) would fall
           through here and try to parse garbage data as JSON. */
        printf("\n---* PROCESS CONFIG FILEs *---\n");
        if(process_json_response(device_id, device_name, current_version) == ESP_OK)
            return new_fw_ver;
        else
            return NULL;
    }
    else
    {
        /* FTP_FETCH_FAILED or any unexpected result */
        ESP_LOGE("VERSION_CHECK", "get_config_file failed — aborting version check");
        ftp_close();
        return NULL;
    }
}



/**
 * @brief  Find a byte sequence within a data buffer
 * @param  data      Pointer to the data buffer
 * @param  size      Size of the data buffer
 * @param  seq       Pointer to the sequence to search for
 * @param  seq_len   Length of the sequence
 * @retval Index of the first occurrence, or -1 if not found
 */
int find_subsequence(const uint8_t *data, size_t size, const uint8_t *seq, size_t seq_len)
{
    if (!data || !seq || seq_len == 0 || size < seq_len) return -1;

    for (size_t i = 0; i <= size - seq_len; i++)
    {
        if (memcmp(&data[i], seq, seq_len) == 0)
        {
            return (int)i; // sequence found at index i
        }
    }
    return -1; // not found
}


/**
 * @brief Get the stored status of the last OTA check
 * Reads from NVS if the OTA check has been performed since last reset.
 * @param status Pointer to int32_t to store the status (0: not checked, 1: checked)
 * @return esp_err_t ESP_OK on success, else error code
 */
esp_err_t get_ota_checked_status(int32_t *status)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) return err;

    err = nvs_get_i32(my_handle, "ota_checked", status);
    nvs_close(my_handle);

    // If key not found, default to 0 (OTA NOT checked)
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *status = 0;
        err = ESP_OK;
    }

    return err;
}

/**
 * @brief Set the OTA checked status
 * Writes the OTA check status index to NVS. Must call nvs_commit() to persist.
 * @param status 0 -> Not checked, 1 -> Checked
 * @return esp_err_t ESP_OK on success
 */
esp_err_t set_ota_checked_status(int32_t status)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_i32(my_handle, "ota_checked", status);
    if (err == ESP_OK) {
        err = nvs_commit(my_handle); // Ensure it is written to flash
    }

    nvs_close(my_handle);
    return err;
}

/**
 * @brief Get the last booted OTA slot
 * Reads from NVS the last booted partition index.
 * 0 -> factory / ota_0
 * 1 -> ota_1
 * 2 -> ota_0 (if you use 2 to indicate second OTA slot)
 * @param boot Pointer to int32_t to store boot partition index
 * @return esp_err_t ESP_OK on success, else error code
 */
esp_err_t get_next_ota(int32_t *boot) 
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) return err;

    err = nvs_get_i32(my_handle, "boot_at", boot);
    nvs_close(my_handle);

    // If key not found, default to factory/ota_0
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *boot = 0;
        err = ESP_OK;
    }

    return err;
}

/**
 * @brief Set the next OTA boot slot
 * Writes the next OTA slot index to NVS. Must call nvs_commit() to persist.
 * @param boot 0 -> factory/ota_0, 1 -> ota_1, etc.
 * @return esp_err_t ESP_OK on success
 */
esp_err_t set_next_ota(int32_t boot)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_i32(my_handle, "boot_at", boot);
    if (err == ESP_OK) {
        err = nvs_commit(my_handle);  // Ensure it is written to flash
    }

    nvs_close(my_handle);
    return err;
}


/*****************************/
/* NVS RELATED FUNCTIONS END */
/*****************************/



/*****************************/
/*   OTA RELATED FUNCTIONS */
/*****************************/

const esp_partition_t *update_partition;        // selected OTA target in OTA_init()

/**
 * @brief Initialize OTA update process
 * 
 * This function selects the correct OTA partition to write the new firmware to.
 * It reads the last booted partition from NVS (`boot_at`) and chooses the other
 * partition for the OTA update. This ensures the running firmware is never overwritten.
 * It then calls `esp_ota_begin()` to prepare the OTA write handle.
 * 
 * Must be called **before starting the FTP GET command** to receive firmware.
 */

esp_err_t OTA_init(esp_ota_handle_t *ota_handle) 
{
    const char* TAG = "OTA_init";

    // FIX: Always use esp_ota_get_next_update_partition() — this is the only
    // reliable way to select the inactive OTA slot.  The previous approach read
    // a custom "boot_at" key from NVS, but that key is only written by this
    // firmware's OTA_end_and_finalize().  When the device is first flashed via
    // the Arduino BT-OTA sketch (stage 1), "boot_at" is never written, so it
    // defaults to 0.  The device actually boots into ota_1 (the slot the BT
    // sketch wrote to), but OTA_init thought curr_ota==0 and also targeted
    // ota_1 — producing ESP_ERR_OTA_PARTITION_CONFLICT (0x1501) because you
    // cannot call esp_ota_begin() on the currently running partition.
    //
    // esp_ota_get_next_update_partition(NULL) reads the real otadata partition
    // (written by esp_ota_set_boot_partition) and always returns the slot that
    // is NOT currently running — exactly what we need, regardless of how the
    // device arrived at its current firmware.
    const esp_partition_t *running = esp_ota_get_running_partition();
    update_partition = esp_ota_get_next_update_partition(NULL);

    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA partition found!");
        return ESP_FAIL;
    }

    // Safety guard: must never write to the running partition
    if (update_partition->address == running->address) {
        ESP_LOGE(TAG, "CONFLICT: update partition == running partition (%s)! Aborting.", running->label);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Running partition : %s (0x%08lX)", running->label, running->address);
    printf("OTA Target Partition:\n");
    printf(" Label: %s\n", update_partition->label);
    printf(" Address: 0x%08lX\n", update_partition->address);
    printf(" Size: %lu bytes\n", update_partition->size);
    printf(" Encrypted: %s\n", update_partition->encrypted ? "Yes" : "No");

    // Begin OTA update on the selected partition
    esp_err_t ret = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s (0x%X)", esp_err_to_name(ret), ret);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA init successful. Ready to receive firmware.");
    return ESP_OK;
}

static const char *TAG = "OTA_END";

/**
 * @brief Finalize OTA update.
 *
 * Call this after all firmware bytes have been written with esp_ota_write().
 *
 * @param new_version_str Optional version string (e.g. "123" or "1"). If numeric, we'll store it using save_version_str().
 *                        Pass NULL to skip saving version.
 * @param reboot_on_success If true, restart device on successful OTA.
 * @return esp_err_t ESP_OK on success, else error code.
 */
esp_err_t OTA_end_and_finalize(const char *new_version_str, bool reboot_on_success)
{
    esp_err_t err;

    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No update_partition set (OTA_init wasn't called?)");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Calling esp_ota_end()...");
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end() failed: %s (0x%X)", esp_err_to_name(err), err);
        // If esp_ota_end fails, the OTA is not valid. Do not switch boot partition.
        // return err;
        return ESP_FAIL;
    }

    // Switch boot partition to the one we wrote
   ESP_LOGI(TAG, "Setting boot partition to: %s (0x%08X)", update_partition->label, (unsigned int)update_partition->address);

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition() failed: %s (0x%X)", esp_err_to_name(err), err);
        return err;
    }

    printf("Device Config before changing\n");
    print_device_config(&device_config);
    // Save new version if provided and numeric
    if (new_version_str != NULL) {

        // try to parse integer version; adapt if you store version differently

        // esp_err_t sret = save_version_str(new_version_str);
        strcpy(device_config.firmware_version, new_version_str);
        esp_err_t sret = save_device_config_to_nvs(&device_config);
        if (sret == ESP_OK) {
            ESP_LOGI(TAG, "Saved new firmware version: %s", device_config.firmware_version);
                printf("Device Config after changing\n");
                print_device_config(&device_config);
        } else {
            ESP_LOGW(TAG, "Failed to save new version to NVS: %s", esp_err_to_name(sret));
        }
    }

    // Mark the boot slot in NVS so future OTA calls (via the old NVS path, if any)
    // stay consistent.  We also rely on esp_ota_set_boot_partition() above which
    // writes the real otadata partition — that is the authoritative source now.
    if (update_partition->label[0] != '\0') {
        if (strcmp(update_partition->label, "ota_0") == 0) {
            set_next_ota(0); // 0 = ota_0
            ESP_LOGI(TAG, "Marked next boot partition as ota_0 (set_next_ota(0)).");
        } else if (strcmp(update_partition->label, "ota_1") == 0) {
            set_next_ota(1); // 1 = ota_1
            ESP_LOGI(TAG, "Marked next boot partition as ota_1 (set_next_ota(1)).");
        } else {
            ESP_LOGW(TAG, "Unknown partition label '%s' — not updating boot_at value in NVS.", update_partition->label);
        }
    }

    ESP_LOGI(TAG, "OTA finalized successfully.");

    // Optional: tidy up other tasks/resources before rebooting
    // Implement terminate_non_uart_tasks() to stop tasks you want killed.
    // If you don't want to kill tasks, remove this call.
    // if (terminate_non_uart_tasks) {
    //     ESP_LOGI(TAG, "Terminating non-UART tasks...");
    //     terminate_non_uart_tasks();
    // }

    if (reboot_on_success) {
        ESP_LOGI(TAG, "Restarting system in 2s to boot new firmware...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
        // not reached
    }

    return ESP_OK;
}



void ota_task(void *pvParameter)
{
    ota_chunk_t chunk;  // struct containing data + length
    size_t bytes_written = 0;
    bool ota_failed = false;
     esp_task_wdt_add(NULL); // Register this task with the watchdog
    // esp_task_wdt_delete(NULL);

    ESP_LOGE("OTA_TASK", "Task created!");
    while(1)
    {
        if(OTA_active) // OTA_active is true during update
        {
            // Wait for a chunk from UART2_task
            if(xQueueReceive(ota_queue, &chunk, pdMS_TO_TICKS(100)))
            {
                // Write only chunk.len bytes to OTA
                esp_err_t ret = esp_ota_write(ota_handle, chunk.data, chunk.len);
                if(ret != ESP_OK)
                {
                    ESP_LOGE("OTA_TASK", "OTA write failed!");
                    OTA_active = false; // stop further OTA
                    ota_failed = true;
                    // Optionally reset GSM / abort FTP
                    break;
                }
                bytes_written += chunk.len;
                // ESP_LOGI("OTA_TASK", "Written %d bytes, total %d", chunk.len, bytes_written);
            }
            else
            {
                // Optional: handle timeout or missing data if needed
            }
            if( ota_rx_success == true ) // OTA complete signal from UART2_task
            {
                ESP_LOGI("OTA_TASK", "OTA RX success signal received");
                break;
            }
        }

            // Give CPU 0 some breathing room and feed watchdog
            vTaskDelay(pdMS_TO_TICKS(10));
            esp_task_wdt_reset(); // Feed the watchdog
    }

    // OTA complete
    if(!ota_failed)
    {
        ESP_LOGE("OTA_TASK", "OTA RX SUCCESS !");
        OTA_active = false; // stop further OTA
    }
    else{
            ota_rx_success = false;
            OTA_active = false; // stop further OTA
            ESP_LOGE("OTA_TASK", "Failed Rx OTA, Aborting");
    }

    vTaskDelete(NULL); // delete this task
    ota_task_Handle = NULL; 
}

/*****************************/
/* OTA RELATED FUNCTIONS END */
/*****************************/


void kill_tasks()
{
     /* Just restart */  
    //  esp_restart();
              
    // if (ota_task_Handle != NULL)
    // {
    //     ESP_LOGI("OTA_TASK", "Deleting OTA task");
    //     vTaskDelete(ota_task_Handle);
    //     delay_ms(100);
    //     ota_task_Handle = NULL; // avoid dangling pointer
    // }

    // if (ota_queue != NULL) {
    //     vQueueDelete(ota_queue);
    //      delay_ms(100);
    //     ota_queue = NULL;
    // }

    // ftp_close();
}

void check_fw_update(int sim)
{   
    printf("\n--------> Starting the Firmware Update Check <---------\n");

    if(uart_data_rxd___ == NULL )
    {
        uart_data_rxd___ = (uint8_t *) malloc(RX_BUF_SIZE + 1);
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *new_partition = esp_ota_get_next_update_partition(NULL);

    ESP_LOGI(TAG, "Running partition: %s", running->label);
    ESP_LOGI(TAG, "New partition: %s", new_partition->label);

    // 1. Create queue before starting UART2_task
    ota_queue = xQueueCreate(100, sizeof(ota_chunk_t));

    if(!ota_queue)
    {
        ESP_LOGE("OTA", "Failed to create OTA queue");
        return;
    } else {
        ESP_LOGI("OTA", "OTA queue created successfully, item size = %d bytes", sizeof(ota_chunk_t));
    }


    // printf("sizeof(ota_chunk_t) = %d\n", sizeof(ota_chunk_t));

    // UART_INIT();

    printf("\n--------> Initing NVS <---------\n");
    // init_nvs();

                // const char *device_id = "ACOM_D_test";
                //  "B-18";

    if (load_device_config_from_nvs(&device_config) != ESP_OK) 
    {
	    init_default_device_config(&device_config);
	    save_device_config_to_nvs(&device_config);
	}

    print_device_config(&device_config);


    const char *device_name = DEVICE_ID;
    const char *device_id = device_config.server_name;
    
    const char *saved_version = device_config.firmware_version;
    printf("device_name: %s\n", device_name);
    printf("device_id: %s\n", device_id);
    printf("Saved version: %s\n", saved_version);

    // // Read
    // if (get_version_str(saved_version, sizeof(saved_version)) == ESP_OK) {
    //     printf("Saved version: %s\n", saved_version);
    // }
    // else{
    //     // Save
    //     save_version_str("FR_0");
    //     get_version_str(saved_version, sizeof(saved_version));
    //     printf("Saved version: %s\n", saved_version);
    // }

    const char* version =  version_check(device_id, device_name,saved_version, sim);
    // char *version = "a"; // testing

    if( version != NULL && strcmp(version, saved_version) != 0)    /* NEW VRESION AVAILABLE */
    {
        printf("\n---- New Version Available: %s ----\n", version);
        printf("\n---- Starting OTA UPDATE ----\n");

        set_UART2_rx_frame_len(3);
        
        xTaskCreate(ota_task, "ota_task", 1024*12, NULL, 2, &ota_task_Handle);

        // esp_ota_handle_t ota_handle;
        if (OTA_init(&ota_handle) == ESP_FAIL) 
        {
            kill_tasks();
            return;
        }
        

                            /* START OTA UPDATE */
        if( start_ota(version) == ESP_FAIL )
        {
            /* Handle OTA FAILURE , DEINIT*/
             printf("\n---- Failed Starting OTA ----\n");
             kill_tasks();
             return;
        }

        /* Handle OTA SUCCESS , DEINIT*/
        if( OTA_end_and_finalize(version, true) == ESP_FAIL )
        {
            /* Handle OTA FAILURE , DEINIT*/
            printf("\n---- Failed Ending OTA ----\n");
            kill_tasks();
            return;
        }
    }
    else{
        printf("\n---- No New Version Available ----\n");
        // kill_tasks();
        return;
    }
    // kill_tasks();

}