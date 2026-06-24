#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include <ctype.h>
#include "GSM_handler.h"
#include "UART_HANDLER.h"
#include <sys/stat.h>
#include "logs_handler.h"
#include "wifi_handler.h"


uint8_t *uart_data_rxd_ = NULL;
int parse_gngga_lat_lon(const char *nmea_str, double *lat, double *lon);

int rssi_to_percentage_from_str(const char *csq_str);
void save_state_if_changed(size_t offset, bool wrapped, bool failed);

// size_t curr_log_offset = 0;  // End of file in bytes
static size_t failed_offset = 0;    // Where last failure happened
static bool is_failed = false;

void delay_(int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void clear_buffers()
{
    uint8_t dump_buf[64];
    while (uart_read_bytes(UART_PORT2, dump_buf, sizeof(dump_buf), 10 / portTICK_PERIOD_MS) > 0) 
    {
        // Discard everything until queue is empty
    }
    uart_data_rxd_[0] = '\0';
}

esp_err_t send_modem_command(modem_cmd_t cmd_id) 
{
    printf("HERE pt 1\n");
                clear_buffers();
                printf("HERE pt 1.2\n");
                // if (xSemaphoreTake(uart2_response_semaphore, portMAX_DELAY)) {
                // uart_flush_input(UART_PORT2);
                // xSemaphoreGive(uart2_response_semaphore);
                // }
                
                printf("HERE pt 1.3\n");
                UART2_rx_done = false;
    printf("HERE pt 2\n");

    const modem_cmd_info_t *cmd_info = &modem_cmds[cmd_id];
    for (int attempt = 0; attempt < cmd_info->retries; attempt++) {
        // uart_write_bytes(UART_PORT, cmd_info->cmd, strlen(cmd_info->cmd));
        write_UART2( cmd_info->cmd);
        ESP_LOGI("MODEM", "Sent: %s (Attempt %d/%d)", cmd_info->cmd, attempt+1, cmd_info->retries);

        if (!cmd_info->expect_response) {
            return ESP_OK; // No need to check response
        }

        // char resp[1024] = {0};
        // int len = uart_read_bytes(UART_PORT, resp, sizeof(resp)-1, cmd_info->timeout_ms / portTICK_PERIOD_MS);
        memset(uart_data_rxd_, 0, RX_BUF_SIZE + 1);
        // delay_(1000);
        esp_err_t x = get_UART2_data( uart_data_rxd_,  cmd_info->timeout_ms, cmd_info->expected);
        
        if (uart_data_rxd_ != NULL)
        {
            if (x == ESP_OK && strstr( (const char *) uart_data_rxd_, cmd_info->expected)) 
            {
                ESP_LOGI("MODEM", "Got expected response: %s", cmd_info->expected);
                return ESP_OK;
            }
            else if (x == ESP_OK )
            {
                ESP_LOGW("MODEM", "String did not match");
                printf("Unmatched : %s\n",uart_data_rxd_);
            }
            else
            {
                ESP_LOGW("MODEM", "No response from uart func");
            }
        }
        else
        {
            ESP_LOGW("MODEM", "uart_data_rxd_ is NULL");
            return ESP_FAIL;
        }

    }

    ESP_LOGE("MODEM", "Command %s failed after %d retries", cmd_info->cmd, cmd_info->retries);
    return ESP_FAIL;
}



void set_modem_command(modem_cmd_t cmd_id, const char *cmd_str)
{                                                                       // int delay_ms, const char *expected_resp, bool check_resp, int retries) {
    if (cmd_id >= CMD_MAX) return; // avoid out-of-bounds

    modem_cmd_info_t *arr = &modem_cmds[cmd_id];

    if (cmd_str) arr->cmd = cmd_str;
    // if (expected_resp) cmd->expected_resp = expected_resp;
    // cmd->delay_ms = delay_ms;
    // cmd->check_resp = check_resp;
    // cmd->retries = retries;
}



void func(void)
{
    write_UART0("hii");
}

esp_err_t gsm_at()
{
    if(uart_data_rxd_ == NULL )
    {
        uart_data_rxd_ = (uint8_t *) malloc(RX_BUF_SIZE + 1);
    }

    // send_modem_command( CMD_AT );
    if( uart_write_modem("AT\r\n", "OK", 1500) == ESP_FAIL )
    {
        ESP_LOGE("AT_CHECK", "AT - MODEM not responding\n");
        return ESP_FAIL;
    }

     return ESP_OK;
}

#define MODEM_DELAY(ms) vTaskDelay((ms) / portTICK_PERIOD_MS)

esp_err_t uart_write_modem(const char *cmd, const char *resp, int timeout)
{
    if(uart_data_rxd_ == NULL )
    {
        uart_data_rxd_ = (uint8_t *) malloc(RX_BUF_SIZE + 1);
    }

    memset(UART2_data, 0, RX_BUF_SIZE + 1);
    memset(uart_data_rxd_, 0, RX_BUF_SIZE + 1); // Clear the temporary Rx buffer

    uart_write_bytes(UART_PORT2, cmd, strlen(cmd));
    uart_wait_tx_done(UART_PORT2, pdMS_TO_TICKS(100)); // ensure full send

    if (get_UART2_data(uart_data_rxd_, timeout, resp) == ESP_OK) {
        return ESP_OK;
    } else {

        return ESP_FAIL;
    }

}



esp_err_t gsm_init(char *imei)
{
    if (uart_data_rxd_ == NULL) {
        uart_data_rxd_ = malloc(RX_BUF_SIZE + 1);
    }

    // uart_write_modem("AT+IFC=?\r\n", "OK", 1500);

    // uart_write_modem("AT+IFC=0,0\r\n", "OK", 1500);

    // uart_write_modem("AT+IPR=115200\r\n", "OK", 1500);

    // uart_write_modem("AT&W\r\n", "OK", 1500);


    if ( uart_write_modem("AT\r\n", "OK", 1500) == ESP_FAIL )
    {
        ESP_LOGE("gsm_init", "AT - MODEM not responding\n");
        return ESP_FAIL;
        // MODEM_DELAY(1000);
        // if ( uart_write_modem("AT\r\n", "OK", 1500) == ESP_FAIL )
        // {
        //     ESP_LOGE("gsm_init", "AT - MODEM not responding\n");
        //     MODEM_DELAY(1000);
        //     if ( uart_write_modem("AT\r\n", "OK", 1500) == ESP_FAIL )
        //     {
        //         ESP_LOGE("gsm_init", "AT - MODEM not responding\n");
        //         MODEM_DELAY(1000);
        //         if ( uart_write_modem("AT\r\n", "OK", 1500) == ESP_FAIL )
        //         {
        //             ESP_LOGE("gsm_init", "AT - MODEM not responding\n");
        //             return ESP_FAIL;
        //         }
        //     }
        // }

    }

    // MODEM_DELAY(1000);

    uart_write_modem("AT+CFUN?\r\n", "OK", 1500);
    // MODEM_DELAY(1000);

    // uart_write_modem("AT+CFUN=1,1\r\n");
    // MODEM_DELAY(5000);

    uart_write_modem("AT+CMEE=1\r\n", "OK", 1500);
    // MODEM_DELAY(4000);

    uart_write_modem("AT+GSN\r\n", "OK", 1500);
    // MODEM_DELAY(2000);

    // printf(" here : %s\n", UART2_data);

    // char imei_arr[16];
    if (extract_imei( (const char *) UART2_data, imei)) 
    {
        printf("IMEI: %s\n", imei);
    } 
    else 
    {
        printf("IMEI not found\n");
        imei[0] = '\0';
    }

    uart_write_modem("AT+CPIN?\r\n", "OK", 1500);
    // MODEM_DELAY(1000);

    /* Wait here for network registration before returning to gsm_command_exe.
       This is the correct place — gsm_init runs once on cold modem start.
       Waiting here means start_internet() can call QIACT immediately without
       its own registration poll, so the polling cycle timer is not affected. */
    ESP_LOGW("GSM_NET", "Waiting for network registration...");
    bool registered = false;
    /* Reduced from 30 iterations x 4000ms = 120s
       to 8 iterations x 3000ms = 24s max.
       1000ms per check is safe for EC21 UART response time (~300-400ms).
       24s is enough for Airtel/Jio cold registration; beyond that the
       cycle must not be blocked further — backlog will catch up. */
    for (int i = 0; i < 8; i++) {
        if (uart_write_modem("AT+CREG?\r\n", "+CREG: 0,1", 1000) == ESP_OK ||
            uart_write_modem("AT+CREG?\r\n", "+CREG: 0,5", 1000) == ESP_OK) {
            registered = true;
            ESP_LOGW("GSM_NET", "Network registered after %d s", (i + 1) * 3);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (!registered) {
        ESP_LOGE("GSM_NET", "Network registration timeout after 24s — no network, saving to backlog");
        return ESP_FAIL;
    }

    return ESP_OK;
}



esp_err_t start_internet(bool sim)
{
    // APN setup — auth=1 (PAP) for Airtel for stable long-term IP leases.
    // Jio does not require authentication, auth=0 is fine.
    if (sim) {
        uart_write_modem("AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\",1\r\n", "OK", 3000 );
    } else {
        uart_write_modem("AT+QICSGP=1,1,\"jionet\",\"\",\"\",0\r\n", "OK", 3000);
    }

    /* Network registration is now confirmed in gsm_init() before we get here,
       so we can go straight to PDP context activation.
       AT+QIACT=1 timeout raised from 1500ms to 30000ms — Airtel towers under
       daytime load take 30-45s to assign an IP. Quectel recommends 150s. */
    uart_write_modem("AT+QIACT=1\r\n", "OK", 30000);

    if ( uart_write_modem("AT+QIACT?\r\n", "+QIACT: 1,1,1", 5000) == ESP_FAIL )
        return ESP_FAIL;
    // MODEM_DELAY(3000);

    // uart_write_modem("AT+QHTTPCFG=\"contextid\",1\r\n", "OK", 1500);
    // MODEM_DELAY(1500);

    // uart_write_modem("AT+QHTTPCFG=\"requestheader\",0\r\n", "OK", 1000);
    // // MODEM_DELAY(500);

    uart_write_modem("AT+QHTTPCFG=\"requestheader\",1\r\n", "OK", 1000);


    uart_write_modem("AT+QHTTPCFG=\"responseheader\",1\r\n", "OK", 1000);
    // MODEM_DELAY(500);

    uart_write_modem("AT+QHTTPCFG=\"sslctxid\",1\r\n", "OK", 1000);
    // MODEM_DELAY(500);

    uart_write_modem("AT+QSSLCFG=\"sslversion\",1,4\r\n", "OK", 1000);
    // MODEM_DELAY(1000);

    uart_write_modem("AT+QSSLCFG=\"seclevel\",1,0\r\n", "OK", 1000);
    // MODEM_DELAY(1000);

    // uart_write_modem("AT+QHTTPCFG=\"contenttype\",1\r\n", "OK", 1000);
    // // MODEM_DELAY(2000);

    uart_write_modem("AT+QHTTPCFG=\"contextid\",1\r\n", "OK", 1500);
    // MODEM_DELAY(1500);

    uart_write_modem("AT+QHTTPCFG?\r\n", "OK", 1500);


    return ESP_OK;
}

const char cmd_url[] = "http://safewaternetwork.in:8091/v1/acom/monitoring/siteData";

/**
 * Configure the HTTP URL
 */
esp_err_t configurl(void)
{    

    char temp_buff[50] = "";
    int str_len = strlen(cmd_url);

    snprintf(temp_buff, sizeof(temp_buff), "AT+QHTTPURL=%d,60\r\n", str_len);

    if ( uart_write_modem(temp_buff, "CONNECT", 5000) == ESP_FAIL )
    {
       ESP_LOGE("configurl","Connect Not detected - aborting after URL len setup failure\n");
        return ESP_FAIL;
    }

    
    if ( uart_write_modem(cmd_url, "OK", 5000) == ESP_FAIL )
    {
        ESP_LOGE("configurl","OK Not detected - aborting after URL failure\n");
        return ESP_FAIL;
    }
 

    // if( get_UART2_data(uart_data_rxd_, 5000, "CONNECT") == ESP_OK )
    //     printf("Connect Detected\n");

    // // Send actual URL
    // uart_write_bytes(UART_PORT2, cmd, str_len);  // no \r\n
    // uart_wait_tx_done(UART_PORT2, pdMS_TO_TICKS(500));

    // uart_write_modem(cmd_url);
    // uart_write_modem("\r\n");
    // MODEM_DELAY(4000);

    // uart_write_modem("AT+QHTTPCFG?\r\n");
    // MODEM_DELAY(3000);
    // uart_write_modem("AT+QHTTPGET=80\r\n");
    // MODEM_DELAY(2000);

    // No error checking here — rely on RX task to monitor output
    return ESP_OK;
}


char payload[] = "{\"DT\":1755113974,\"Device\":\"NODE_001\",\"IMEI\":\"123IEMEI458\",\"FR_v\":\"1.0.0\",\"Lat\":0.100000,\"Lon\":0.200000,\"Bat\":100.00,\"Sol\":100.00,\"PSR\":0.00,\"PH\":0.00,\"TDS\":0.00,\"CLO\":0.00,\"TPW\":0.00,\"WGT\":0.00,\"TBD\":0.00,\"UFM1_F\":0.00,\"UFM1_V\":0.00,\"UFM2_F\":0.00,\"UFM2_V\":0.00,\"TPA\":0.00,\"FLW\":0.00,\"SW1\":0.00,\"SW2\":0.00}";

#define SECONDARY_GSM_URL "https://gkcsq4xz1k.execute-api.ap-south-1.amazonaws.com/default/sendTelemetryData"
#define SECONDARY_GSM_HOST "gkcsq4xz1k.execute-api.ap-south-1.amazonaws.com"
#define SECONDARY_GSM_PATH "/default/sendTelemetryData"
 
/**
 * Fire-and-forget POST to secondary server (our own viewing server).
 * No retry, no NVS, no fail handling — best effort only.
 */
void gsm_post_secondary(const char *datasend)
{
    /* Settle delay: after gsm_post_data completes (AT+QHTTPREAD exchange), the modem
       needs time to flush its response buffer before accepting a new AT+QHTTPURL command.
       Without this, CONNECT never arrives and the function silently skips to restore_primary
       — which is why SEC_POST logs were missing entirely from the terminal output. */
    vTaskDelay(pdMS_TO_TICKS(500));
    clear_buffers();

    char temp_buff[64] = "";
    int  body_len      = strlen(datasend);
 
    /* Secondary URL length without \r\n (modem needs exact char count of URL only) */
    int sec_url_len = strlen(SECONDARY_GSM_URL);
 
    /* Build header for secondary server */
    char sec_header[256] = "";
    snprintf(sec_header, sizeof(sec_header),
             "POST " SECONDARY_GSM_PATH " HTTP/1.1\r\n"
             "Host: " SECONDARY_GSM_HOST "\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "\r\n",
             body_len);
 
    int header_len = strlen(sec_header);
    int total_len  = header_len + body_len;
 
    /* Set secondary URL on modem — length must be URL only, no \r\n */
    snprintf(temp_buff, sizeof(temp_buff), "AT+QHTTPURL=%d,60\r\n", sec_url_len);
    if (uart_write_modem(temp_buff, "CONNECT", 5000) == ESP_FAIL) {
        ESP_LOGW("SEC_POST", "Secondary URL set failed - skipping");
        goto restore_primary;
    }
 
    /* Send URL */
    if (uart_write_modem(SECONDARY_GSM_URL, "OK", 5000) == ESP_FAIL) {
        ESP_LOGW("SEC_POST", "Secondary URL write failed - skipping");
        goto restore_primary;
    }
 
    /* POST — send AT+QHTTPPOST with total length */
    ESP_LOGI("SEC_POST", "Posting to: " SECONDARY_GSM_URL);
    snprintf(temp_buff, sizeof(temp_buff), "AT+QHTTPPOST=%d,77,20\r\n", total_len);
    if (uart_write_modem(temp_buff, "CONNECT", 5000) == ESP_FAIL) {
        ESP_LOGW("SEC_POST", "Secondary CONNECT not received - skipping");
        goto restore_primary;
    }
 
    char *full_req = malloc(total_len + 1);
    if (full_req == NULL) { goto restore_primary; }
    memcpy(full_req, sec_header, header_len);
    memcpy(full_req + header_len, datasend, body_len);
    full_req[total_len] = '\0';
 
    esp_err_t ret = uart_write_modem(full_req, "+QHTTPPOST: 0,200", 8000);
    free(full_req);
 
    if (ret == ESP_FAIL) {
        ESP_LOGI("SEC_POST", "Secondary POST success (AWS 200)");
    } else {
        uart_write_modem("AT+QHTTPREAD\r\n", "OK", 3000);
        ESP_LOGW("SEC_POST", "Secondary POST - no 200 status, server response above");
    }
 
    restore_primary:
    /* Flush any pending modem HTTP session before restoring primary URL. If secondary POST failed mid-way (CONNECT not received, or POST body not accepted), the modem HTTP layer is left in a dirty state.
       AT+QHTTPREAD drains any pending response, the 1s delay lets the modem settle, then configurl() re-sets the primary URL cleanly.
       Without this flush, the next AT+QHTTPPOST is rejected by the modem because it believes a session is still active. */

    uart_write_modem("AT+QHTTPREAD\r\n", "OK", 3000);   /* drain any pending response */
    vTaskDelay(pdMS_TO_TICKS(1000));                     /* let modem HTTP layer reset */
    clear_buffers();                                      /* flush UART rx garbage */
    configurl();                                          /* restore primary URL */
}

/* Sync modem clock via NTP (AT+QNTP), then read back UTC time (AT+QLTS=2).
 * Returns parsed UTC broken-down time in output params.
 * Caller is responsible for IST offset conversion and RTC write.
 * Requires active PDP context (call only after a successful post). */
esp_err_t gsm_get_network_time(int *year, int *mon, int *day,
                                int *hour, int *min, int *sec, int *wday)
{
    int year_raw = 0;
    bool got_time = false;

    /* ── Stage 1: NTP via AT+QNTP (UTC, needs UDP/123 open on carrier) ─── */
    if (uart_write_modem("AT+QNTP=1,\"pool.ntp.org\"\r\n", "+QNTP:", 30000) == ESP_OK) {
        const char *p = strstr((const char *)UART2_data, "+QNTP: 0,\"");
        if (p) {
            p += 10; /* skip '+QNTP: 0,"' */
            if (sscanf(p, "%d/%2d/%2d,%2d:%2d:%2d",
                       &year_raw, mon, day, hour, min, sec) == 6) {
                *year = (year_raw < 100) ? year_raw + 2000 : year_raw;
                got_time = true;
                ESP_LOGI("GSM_NTP", "Time source: AT+QNTP (NTP)");
            } else {
                ESP_LOGW("GSM_NTP", "QNTP parse failed: %s", UART2_data);
            }
        } else {
            ESP_LOGW("GSM_NTP", "QNTP success marker missing: %s", UART2_data);
        }
    } else {
        ESP_LOGW("GSM_NTP", "AT+QNTP failed/timed out — trying QLTS fallback");
    }

    /* ── Stage 2: NITZ via AT+QLTS=2 (IST, carrier-pushed, no UDP needed) ─ */
    if (!got_time) {
        if (uart_write_modem("AT+QLTS=2\r\n", "+QLTS:", 3000) == ESP_OK) {
            /* Response format: +QLTS: "yy/MM/dd,hh:mm:ss+zz,dst"
             * The time is LOCAL (IST for Indian SIMs) — zone offset is +22 (quarters = +5:30).
             * We strip the zone and treat it as IST directly, since try_gsm_ntp_sync()
             * caller expects UTC and will add IST_OFFSET — so we must convert back to UTC. */
            const char *q = strstr((const char *)UART2_data, "+QLTS: \"");
            if (q) {
                q += 8; /* skip '+QLTS: "' */
                int tz_quarters = 0, dst = 0;
                if (sscanf(q, "%d/%2d/%2d,%2d:%2d:%2d+%d,%d",
                           &year_raw, mon, day, hour, min, sec,
                           &tz_quarters, &dst) >= 6) {
                    *year = (year_raw < 100) ? year_raw + 2000 : year_raw;

                    /* Convert IST → UTC: subtract tz_quarters * 15 minutes */
                    struct tm local_t = {
                        .tm_year = *year - 1900,
                        .tm_mon  = *mon  - 1,
                        .tm_mday = *day,
                        .tm_hour = *hour,
                        .tm_min  = *min,
                        .tm_sec  = *sec,
                        .tm_isdst = 0
                    };
                    /* Force UTC interpretation for mktime */
                    setenv("TZ", "UTC0", 1); tzset();
                    time_t local_epoch = mktime(&local_t);
                    unsetenv("TZ"); tzset();

                    time_t utc_epoch = local_epoch - (tz_quarters * 15 * 60);
                    struct tm utc_t;
                    gmtime_r(&utc_epoch, &utc_t);

                    *year = utc_t.tm_year + 1900;
                    *mon  = utc_t.tm_mon  + 1;
                    *day  = utc_t.tm_mday;
                    *hour = utc_t.tm_hour;
                    *min  = utc_t.tm_min;
                    *sec  = utc_t.tm_sec;

                    got_time = true;
                    ESP_LOGI("GSM_NTP", "Time source: AT+QLTS=2 (NITZ/IST→UTC)");
                } else {
                    ESP_LOGW("GSM_NTP", "QLTS parse failed: %s", UART2_data);
                }
            } else {
                ESP_LOGW("GSM_NTP", "QLTS marker missing: %s", UART2_data);
            }
        } else {
            ESP_LOGW("GSM_NTP", "AT+QLTS=2 failed — modem has no NITZ time yet");
        }
    }

    if (!got_time) {
        ESP_LOGE("GSM_NTP", "All time sources failed");
        return ESP_FAIL;
    }

    /* Compute weekday via mktime */
    struct tm t = {
        .tm_year = *year - 1900,
        .tm_mon  = *mon  - 1,
        .tm_mday = *day,
        .tm_hour = *hour,
        .tm_min  = *min,
        .tm_sec  = *sec,
        .tm_isdst = 0
    };
    mktime(&t);
    *wday = t.tm_wday;

    ESP_LOGI("GSM_NTP", "UTC time: %04d/%02d/%02d %02d:%02d:%02d (wday=%d)",
             *year, *mon, *day, *hour, *min, *sec, *wday);
    return ESP_OK;
}


/**
 * Send HTTP POST data
 */
esp_err_t gsm_post_data(const char *datasend)
{
    /*
     * requestheader=1 is active, so we must send the full HTTP request header
     * as part of the POST body. The total length passed to AT+QHTTPPOST must
     * include BOTH the header block AND the JSON payload.
     *
     * Header format (CRLF line endings, blank line to end headers):
     *   POST /v1/acom/monitoring/siteData HTTP/1.1\r\n
     *   Host: safewaternetwork.in:8091\r\n
     *   Content-Type: application/json\r\n
     *   Content-Length: <body_len>\r\n
     *   \r\n
     *   <JSON payload>
     */

    int body_len  = strlen(datasend);
    /* Build the header block first so we know its exact length */

    char http_header[256] = "";
    snprintf(http_header, sizeof(http_header),
             "POST /v1/acom/monitoring/siteData HTTP/1.1\r\n"
             "Host: safewaternetwork.in:8091\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "\r\n",
             body_len);
    ESP_LOGI("GSM_POST", "Posting to: %s", cmd_url);
 
    int header_len = strlen(http_header);
    int total_len  = header_len + body_len;   /* what the modem counts */
 
    char temp_buff[64] = "";
    snprintf(temp_buff, sizeof(temp_buff), "AT+QHTTPPOST=%d,77,20\r\n", total_len);

    // char temp_buff[50] = "";
    // int str_len = strlen(datasend);
    // snprintf(temp_buff, sizeof(temp_buff), "AT+QHTTPPOST=%d,77,20\r\n", str_len);

    if( uart_write_modem(temp_buff, "CONNECT", 5000) == ESP_FAIL)
    {
        ESP_LOGE("gsm_post_data", "Connect Not detected - aborting after POST len failure\n");
        return ESP_FAIL;
    }
       /* Concatenate HTTP header + JSON body into one buffer and send together.
     * The modem expects the full request (header + body) as a single stream
     * after it responds with CONNECT to AT+QHTTPPOST. */

    char *full_request = malloc(total_len + 1);
    if( full_request == NULL )
    {
        ESP_LOGE("gsm_post_data", "malloc failed for full_request\n");
        return ESP_FAIL;
    }
    memcpy(full_request, http_header, header_len);
    memcpy(full_request + header_len, datasend, body_len);
    full_request[total_len] = '\0';
 
    esp_err_t post_ret = uart_write_modem(full_request, "200", 20000);
    free(full_request);

    if( post_ret == ESP_FAIL )
    {
        ESP_LOGE("gsm_post_data", "200 Not detected - aborting after POST failure\n");
        return ESP_FAIL;
    }

    // if( uart_write_modem(datasend, "200", 5000 ) == ESP_FAIL )
    // {
    //     ESP_LOGE("gsm_post_data", "200 Not detected - aborting after POST failure\n");
    //     return ESP_FAIL;
    // }

    /* Read server body ONCE.
       Important: AT+QHTTPREAD consumes the pending response. Issuing it twice
       often returns "+QHTTPREAD: 0" / "ERROR", which causes a false failure
       loop on entries that the server already has ("already exists"). */
    if (uart_write_modem("AT+QHTTPREAD\r\n", "OK", 5000) == ESP_FAIL) {
        printf("Fail response: %s\n", UART2_data);
        ESP_LOGE("gsm_post_data", "QHTTPREAD failed (no OK) - treating as failure");
        return ESP_FAIL;
    }

    /* Keep server response exactly as received; only interpret it. */
    printf("RAW SERVER BODY: %s\n", UART2_data);

    if (strstr((const char *)UART2_data, "Data added successfully") != NULL) {
        printf("SUCCESSFUL POST\n");
        return ESP_OK;
    }

    if (strstr((const char *)UART2_data, "already exists, skipping") != NULL) {
        ESP_LOGW("gsm_post_data", "Server reports entry already exists; treating as success");
        printf("SUCCESSFUL POST\n");
        return ESP_OK;
    }

    ESP_LOGE("gsm_post_data", "Unexpected server body after POST - treating as failure");
    return ESP_FAIL;
    /* IMPORTANT:
       Do not modify backlog state here. This function is used both for:
       1) current record post, and
       2) backlog line post from try_post_from_failed().
       Clearing state here causes backlog to be marked complete after the first
       successful backlog line, which skips remaining pending records.
       Backlog state transitions are handled explicitly by callers
       (try_post_from_failed / handle_gsm_post). */

}



/**
 * Get RSSI in percentage
 */

static const char *TAG_TEST = "GSM";

int gsm_signal_strength(void)
{
    // 1. Check SIM status
    if (uart_write_modem("AT+CPIN?\r\n", "+CPIN:", 2000) != ESP_OK) {
        ESP_LOGE(TAG_TEST , "SIM check failed");
        return 0;
    }

    if (strstr((const char *)UART2_data, "READY") == NULL) {
        ESP_LOGE(TAG_TEST , "SIM not ready -> Signal = 0%%");
        return 0;
    }

    // 2. Check network registration
    if (uart_write_modem("AT+CREG?\r\n", "+CREG:", 2000) != ESP_OK) {
        ESP_LOGE(TAG_TEST , "Network registration check failed");
        return 0;
    }

    if (strstr((const char *)UART2_data, "+CREG: 0,0") != NULL || strstr((const char *)UART2_data, "+CREG: 0,2") != NULL) {
        ESP_LOGE(TAG_TEST , "Not registered on network -> Signal = 0%%");
        return 0;
    }

    // 3. Get signal strength
    if (uart_write_modem("AT+CSQ\r\n", "+CSQ:", 2000) == ESP_OK) {
        ESP_LOGI(TAG_TEST , "+CSQ detected: %s", UART2_data);
        return rssi_to_percentage_from_str((const char *)UART2_data);
    } else {
        ESP_LOGE(TAG_TEST, "+CSQ not detected -> Signal = 0%%");
        return 0;
    }
}  



float map(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int rssi_to_percentage_from_str(const char *csq_str)
{
    if (!csq_str)
    {
        printf("CSQ buffer NULL\n");
        return 0;
    }

    // Skip until "+CSQ:" is found
    const char *start = strstr(csq_str, "+CSQ:");
    if (!start)
    {
        printf("No +CSQ found in string: %s\n", csq_str);
        return 0;
    }

    int rssi_val = -1, ber = -1;
    if (sscanf(start, "+CSQ: %d,%d", &rssi_val, &ber) == 2)
    {
        if (rssi_val == 99)
        {
            printf("Signal quality unknown (99)\n");
            return 0;
        }
        int dbm;
        if (rssi_val == 0)
            dbm = -113;
        else if (rssi_val == 1)
            dbm = -111;
        else if (rssi_val >= 2 && rssi_val <= 30)
        {
            dbm = map(rssi_val, 2, 30, -109, -53); // Linear mapping for the main range
        }
        else if (rssi_val == 31)
            dbm = -51;
        else
            dbm = 0; // Should not happen

        // --- 2. Calculate Percentage ---
        // Map the raw RSSI value (0-31) to a percentage (0-100).
        int percentage = map(rssi_val, 0, 31, 0, 100);
        printf("Signal Strength: %d dBm\n",dbm);
        printf("Signal Strength Precentage: %d%%\n",percentage);
        return percentage;
    }
    printf("Failed to parse CSQ string: %s\n", csq_str);
    return 0;
}


// int gsm_signal_strength(void)
// {
//     int csq_val = 0;

//     if ( uart_write_modem("AT+CSQ\r\n", "+CSQ:", 5000) == ESP_OK )
//     {
//         printf("+CSQ: Detected\n");
//         csq_val = rssi_to_percentage_from_str( (const char *) UART2_data);
//     }
//     else
//     {
//         printf("+CSQ: Not detected - CSQ Val 0\n");
//     }

//     return csq_val;
// }

void delay_100ms()
{
    delay_(100);
    
}

esp_err_t get_gps_location(double *latitude, double *longitude, gps_callback_t callback)
{
    printf("ok here1\n");

    if( uart_write_modem("AT+QGPS?\r\n", "+QGPS: 1", 300) == ESP_OK)
    {
        ESP_LOGI("GPS", "GPS is ON");
    }
    else
    {
        ESP_LOGW("GPS", "GPS is off, Turning it on");
 
        if( uart_write_modem("AT+QGPS=1\r\n", "OK", 3000) == ESP_FAIL)
        {
            ESP_LOGE("GPS", "OK - aborting after GPS is OFF\n");
            return ESP_FAIL;
        }
        if( uart_write_modem("AT+QGPS?\r\n", "+QGPS: 1", 3000) == ESP_FAIL)
        {
            ESP_LOGI("GPS", "GPS is still OFF Aborting");
            return ESP_FAIL;
        }
    }

    for(int i = 0; i < 2000; i+=100)
    {
        delay_100ms();
        callback();
    }

    // delay_(5000);
    // 
    // AT+QGPSGNMEA="GGA"

    if( uart_write_modem("AT+QGPSCFG=\"nmeasrc\",1\r\n", "OK", 3000) == ESP_FAIL )
    {
        ESP_LOGI("GPS", "GPS NMEASRC enabling Failed");
    }

    for(int i = 0; i < 2000; i+=100)
    {
        delay_100ms();
        callback();
    }

    if( uart_write_modem("AT+QGPSGNMEA=\"GGA\"\r\n", "OK", 3000) == ESP_FAIL )
    {
        ESP_LOGI("GPS", "GPS NMEASRC set Failed");
        *latitude = 0;
        *longitude = 0;
        return ESP_FAIL;
    }
    else
    {
        printf("Process the Lat Long data : ");
        printf("%s\n",uart_data_rxd_);

        if( parse_gngga_lat_lon( (const char *) uart_data_rxd_,latitude, longitude) == -1)
        {
                ESP_LOGI("GPS", "GPS LOC Not valid");
                *latitude = 0;
                *longitude = 0;
                return ESP_FAIL;
        }

        if( uart_write_modem("AT+QGPSEND\r\n", "OK", 3000) == ESP_FAIL)
        {
            ESP_LOGI("GPS", "Could not turn off GPS !");
        }
        else
        {
            ESP_LOGI("GPS", "GPS Turned OFF !");
        }
    }

    return ESP_OK;

}


/* Sleep Mode
   While being Resitered on n/w
   AT+QSCLK=1     # enable sleep
   AT+QSCLK=0     # disable sleep (normal idle)

Sleep current: ~1.2–1.4 mA (instead of ~13–28 mA idle).

Wake trigger: any data sent on UART, ring indicator, or hardware wakeup.

Network: Yes, stays attached.

Wake delay: a few ms — practically instant.
*/

esp_err_t GSM_sleep(void)
{
    esp_err_t ret;
    for( int i = 0; i < 3;i++)
    {
        ret = uart_write_modem("AT+QSCLK=1\r\n", "OK", 2000);
        if ( ret == ESP_OK )
        {
            ESP_LOGI("GSM_SLEEP", "GSM is now in sleep mode");
            return ESP_OK;
        }
        else
        {
            ESP_LOGE("GSM_SLEEP", "Failed to put GSM in sleep mode");
        }
    }
     return ESP_FAIL;
}


esp_err_t GSM_wake(void)
{
    if ( uart_write_modem("AT+QSCLK=0\r\n", "OK", 2000) == ESP_OK )
    {
        ESP_LOGI("GSM_WAKE", "GSM is now awake");
        return ESP_OK;
    }
    else
    {
        ESP_LOGE("GSM_WAKE", "Failed to wake GSM from sleep mode");
        return ESP_FAIL;
    }
}

/* Get GSM Signal Strength
+CSQ: 15,99
OK
First number = RSSI (0–31, 99 = unknown)
Second number = Bit Error Rate (0–7, 99 = unknown)

| RSSI | dBm  | % Strength |
| ---- | ---- | ---------- |
| 0    | -113 | 0%         |
| 10   | -93  | 32%        |
| 15   | -83  | 48%        |
| 20   | -73  | 64%        |
| 25   | -63  | 80%        |
| 31   | -51  | 100%       |
*/




int extract_imei(const char *response, char *imei_out) {
    const char *p = response;
    while (*p) {
        // Check if next 15 characters are all digits
        int i;
        for (i = 0; i < 15; i++) {
            if (!isdigit((unsigned char)p[i])) break;
        }
        if (i == 15) {
            // Found IMEI
            strncpy(imei_out, p, 15);
            imei_out[15] = '\0'; // null terminate
            return 1; // success
        }
        p++;
    }

    printf("Not found IMEI: %s\n",response);
    return 0; // IMEI not found
}


// int gsm_signal_strength()
// {
//     if( send_modem_command( CMD_SIG_STREN ) == ESP_OK)
//         return rssi_to_percentage_from_str((const char *) uart_data_rxd_);
//     else
//         return 0;
//     delay_(1000);
// }

void log_failed_data()
{
    
}

void retry_sending_log_data()
{

}

bool try_post_all_logs(void)
{
    size_t file_size = get_file_size();   // keep only for the zero-check

    if (file_size == 0 && get_curr_log_offset() == 0) {
        ESP_LOGW("TRY_FAILED", "Log file empty — skipping post.");
        return no_failed_data;
    }

    FILE *log_file = fopen(log_file_path, "r");
    if (!log_file)
    {
        ESP_LOGE("POST_failed_data", "Unable to open log file for reading");
        return false;
    }

    char line[512]; // Adjust according to max log size
    while (fgets(line, sizeof(line), log_file))
    {
        // Remove trailing newline (optional)
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        ESP_LOGI("POST_failed_data", "Posting log: %s", line);

        esp_err_t post_ok = gsm_post_data(line);
        if (post_ok == ESP_OK)
        {
            ESP_LOGE("POST_failed_data", "Post failed for log: %s", line);
            // Optionally: break or retry
            return false;
        }
        else
        {
            ESP_LOGI("POST_failed_data", "Post success");
        }
    }

    fclose(log_file);
    return true;
}

/**
 * @brief Parse GNGGA NMEA string and extract latitude & longitude.
 * @param nmea_str   Input string (e.g. "+QGPSGNMEA: $GNGGA,120000.00,2836.78498,N,07723.69557,E,...")
 * @param lat        Pointer to float for latitude (decimal degrees)
 * @param lon        Pointer to float for longitude (decimal degrees)
 * @return 0 if OK, -1 if error or invalid data
 */

int parse_gngga_lat_lon(const char *nmea_str, double *lat, double *lon)
{
    if (!nmea_str || !lat || !lon) return -1;

    // Find the "$GNGGA" part
    const char *gga_ptr = strstr(nmea_str, "$GNGGA");
    if (!gga_ptr) return -1;

    // Work on a copy of the remaining string
    char buf[256];
    strncpy(buf, gga_ptr, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Tokenize by comma
    char *token = strtok(buf, ",");
    int field_index = 0;
    char lat_str[20] = {0}, lon_str[20] = {0};
    char ns = 'N', ew = 'E';

    while (token != NULL)
    {
        if (field_index == 2) { // Latitude
            strncpy(lat_str, token, sizeof(lat_str) - 1);
        }
        else if (field_index == 3) { // N/S
            ns = token[0];
        }
        else if (field_index == 4) { // Longitude
            strncpy(lon_str, token, sizeof(lon_str) - 1);
        }
        else if (field_index == 5) { // E/W
            ew = token[0];
            break;
        }
    
        token = strtok(NULL, ",");
        field_index++;
    }

    
    
    if (strlen(lat_str) < 4 || strlen(lon_str) < 5) {
        printf("Invalid lat/lon strings: '%s', '%s'\n", lat_str, lon_str);
        return -1;
    }

    // Convert lat
    double lat_deg = atof(lat_str) / 100.0;
    int lat_int = (int)lat_deg;
    double lat_min = (lat_deg - lat_int) * 100.0;
    *lat = lat_int + lat_min / 60.0;
    if (ns == 'S') *lat = -(*lat);

    // Convert lon
    double lon_deg = atof(lon_str) / 100.0;
    int lon_int = (int)lon_deg;
    double lon_min = (lon_deg - lon_int) * 100.0;
    *lon = lon_int + lon_min / 60.0;
    if (ew == 'W') *lon = -(*lon);

    return 0;
}


/*  FAKE DATA POST FOR LOGS TEST */


const char *posted_log_file_path = "/spiffs/posted_logs.txt";
#define MAX_POSTED_LOGS 50

// Count of how many logs are currently stored
static size_t posted_log_count = 0;

// Function to save posted log (append until max size, then stop)
static void save_posted_log(const char *log_line) {
    struct stat st;
    if (stat(posted_log_file_path, &st) == 0) {
        if (posted_log_count >= MAX_POSTED_LOGS) {
            ESP_LOGW("POST_LOG", "Max file size reached (%ld bytes), not writing", st.st_size);
            return; // stop writing, do not overwrite
        }
    }

    // --- Extract timestamp (DT field) ---
    const char *dt_ptr = strstr(log_line, "\"DT\":");
    if (!dt_ptr) {
        ESP_LOGE("POST_LOG", "No DT field in log line");
        return;
    }
    dt_ptr += 5; // move past "DT":
    long timestamp = atol(dt_ptr);

    FILE *file = fopen(posted_log_file_path, "a");
    if (!file) {
        ESP_LOGE("POST_LOG", "Failed to open posted log file");
        return;
    }

    // --- Write only timestamp ---
    fprintf(file, "%ld\n", timestamp);
    fclose(file);

    posted_log_count++;
    ESP_LOGI("POST_LOG", "Log saved (timestamp %ld). Total count: %d", timestamp, posted_log_count);
}


void clear_posted_logs(void)
{
    FILE *f = fopen(posted_log_file_path, "wb");
    if (f)
    {
        fclose(f);
        posted_log_count = 0;
        ESP_LOGI("POST_LOG", "Posted logs cleared");
    }
    else
    {
        // Not an error after fresh format
        ESP_LOGW("POST_LOG", "Could not clear posted logs (errno=%d) — "
                 "file may not exist yet, this is normal after format",
                 errno);
        posted_log_count = 0;
    }
}

// Print all posted logs
void print_posted_logs(void) {
    FILE *file = fopen(posted_log_file_path, "r");
    if (!file) {
        ESP_LOGW("POST_LOG", "No posted logs file yet");
        return;
    }

    ESP_LOGI("POST_LOG", "=== Posted Logs ===");
    char line[512];
    int idx = 0;
    while (fgets(line, sizeof(line), file)) {
        printf("[%d] %s", idx++, line);
    }
    fclose(file);
}

#define MAX_FAILED_POSTS 3
#define MAX_PASSED_POSTS ( MAX_FAILED_POSTS + 2 )
// Fake post function
esp_err_t fake_post_fail(char *line) {
    static uint8_t failed_data = 0;

    delay_(1000);
    failed_data++; 


    if (failed_data >= MAX_FAILED_POSTS) {
        ESP_LOGE("fake_post_fail", "Simulated POST SUCCESS !");
        failed_offset = curr_log_offset; // no failed data
        is_failed = false;

        // Save the successfully posted log
        save_posted_log(line);

        if( failed_data >= MAX_PASSED_POSTS)
            failed_data = 0; // reset for next cycle

        return ESP_OK;
    }

    ESP_LOGW("fake_post_fail", "Simulated Post failure %d", failed_data);
    return ESP_FAIL;
}



/* FAKE DATA POST FOR LOGS TEST ENDS */

/* For Failed Data Post */

#include "nvs_flash.h"
#include "nvs.h"

typedef struct {
    uint32_t failed_offset;
    bool wrap;
    bool is_failed;
} post_state_t;

static post_state_t last_state = { .failed_offset = (uint32_t)-1, .wrap = false, .is_failed = false };

static void log_backlog_state(const char *tag)
{
    size_t curr_off = get_curr_log_offset();
    size_t pending = get_backlog_count();
    ESP_LOGW("BACKLOG_STATE",
             "%s failed_off=%zu curr_off=%zu wrap=%d failed=%d pending=%zu",
             tag ? tag : "state",
             failed_offset,
             curr_off,
             post_wrap,
             is_failed,
             pending);
}

// Save state as blob
void save_post_state_blob(size_t index, bool wrap, bool extra_flag) {
    post_state_t state = {
        .failed_offset = (uint32_t)index,
        .wrap = wrap,
        .is_failed = extra_flag
    };

    // Skip if nothing changed
    if (state.failed_offset == last_state.failed_offset &&
        state.wrap == last_state.wrap &&
        state.is_failed == last_state.is_failed) {
        return;
    }

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        esp_err_t err = nvs_set_blob(nvs, "post_state", &state, sizeof(state));
        if (err == ESP_OK) {
            nvs_commit(nvs);
            last_state = state; // update cached copy
        } else {
            ESP_LOGE("NVS", "Failed to set blob: %s", esp_err_to_name(err));
        }
        nvs_close(nvs);
    } else {
        ESP_LOGE("NVS", "Failed to open NVS for writing");
    }
}

// Load state as blob
void load_post_state_blob(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        post_state_t state;
        size_t required_size = sizeof(state);

        esp_err_t err = nvs_get_blob(nvs, "post_state", &state, &required_size);
        if (err == ESP_OK && required_size == sizeof(state)) {
            failed_offset = state.failed_offset;
            post_wrap = state.wrap;
            is_failed = state.is_failed;
            last_state = state;

            ESP_LOGI("NVS", "Loaded state: failed_offset=%u | wrap=%d | failed=%d",
                     failed_offset, post_wrap, is_failed);
        } else {
            ESP_LOGW("NVS", "No valid blob found (err=%s)", esp_err_to_name(err));
        }
        nvs_close(nvs);
    } else {
        ESP_LOGE("NVS", "Failed to open NVS for reading");
    }
}

esp_err_t erase_post_state_blob(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS namespace (%s)", esp_err_to_name(err));
        return err;
    }

    // Erase the blob key
    err = nvs_erase_key(nvs, "post_state");
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "No blob to erase (key not found)");
    } else if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to erase blob (%s)", esp_err_to_name(err));
        nvs_close(nvs);
        return err;
    }

    // Commit changes
    err = nvs_commit(nvs);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Commit failed (%s)", esp_err_to_name(err));
    } else {
        ESP_LOGI("NVS", "Blob erased successfully");
    }

    nvs_close(nvs);
    return err;
}


void save_state_if_changed(size_t offset, bool wrapped, bool failed)
{
    load_post_state_blob();

    if( offset != failed_offset || wrapped != post_wrap || failed != is_failed )
    {
        failed_offset = offset;
        post_wrap = wrapped;
        is_failed = failed;
        save_post_state_blob(failed_offset, post_wrap, is_failed);
        printf("State changed - saved to NVS: offset=%zu, wrap=%d, failed=%d\n", failed_offset, post_wrap, is_failed);
    }
    else
    {
        ESP_LOGI("SAVE_STATE", "No change in state - skipping NVS write");
    }
}

#define BACKLOG_BATCH_SIZE 50  /* Increased to 50 to burst post the backlog */
// Posting with wrap handling               [200]
post_status_t try_post_from_failed(size_t start_offset, bool is_wifi_post) 
{
    size_t file_size = get_file_size();
    
    uint8_t success_post_count = 0;
    int batch_count = 0;    /* counts entries posted in this batch */
    static size_t drain_total = 0;   /* persists across batch calls during one drain session */
    static uint8_t lag_retry_count = 0; 
    if (file_size == 0) {
        printf("Log file empty — skipping post.\n");
        return no_failed_data;
    }
    
    //NEW — compare against curr_log_offset, not stat() size
    if (start_offset >= get_curr_log_offset()) {
        ESP_LOGW("TRY_FAILED", "start_offset=%zu >= curr_log_offset=%zu — nothing to drain",
                start_offset, get_curr_log_offset());
        save_state_if_changed(get_curr_log_offset(), false, false);
        return no_failed_data;
    }
    // Only use file_size to catch the genuine orphaned-offset case:
    if (start_offset > file_size && file_size > 0) {
        size_t gap = start_offset - file_size;
        static uint8_t lag_retry_count = 0;
        if (gap <= MAX_LINE_LEN && lag_retry_count < 3) {
            lag_retry_count++;
            ESP_LOGW("TRY_FAILED", "start_offset=%zu > file_size=%zu — SPIFFS lag retry %d/3",
                    start_offset, file_size, lag_retry_count);
            return failed_posting_failed_data;
        }
        lag_retry_count = 0;
        start_offset = file_size > MAX_LINE_LEN ? file_size - MAX_LINE_LEN : 0;
    }

    lag_retry_count = 0;  /* successful seek — reset counter */

    char line[MAX_LINE_LEN];
    size_t post_offset = start_offset;
            // [200] + LOG_X = [300]

    load_post_state_blob(); // restore failed_offset & post_wrap & is_failed
    bool wrapped_once = post_wrap; // restore wrap state if continuing

    FILE *fp = fopen(log_file_path, "r");
    if (!fp) {
        ESP_LOGE("TRY_FAILED", "fopen failed — backlog preserved at offset %zu, retrying next cycle", start_offset);
        return failed_posting_failed_data;
    }

    
    fseek(fp, post_offset, SEEK_SET); //fp = [200] - addrs of log in file

    // Realign if we started mid-line (not at start of a log)   - {
    int c;
    // at the beginning of a fresh drain (when we're at the failed_offset start):
    if (post_offset == start_offset) {
        drain_total = 0;   /* new drain session starting from top */
    }
    // ✅ NEW — seek directly to start_offset, try fgets from there.
    // If the line is malformed, advance by one full entry size and retry.
    // Never scan forward byte-by-byte past a line boundary.

    fseek(fp, post_offset, SEEK_SET);

    /* Realign: read lines until we find a valid JSON start.
    Don't scan byte-by-byte — that can land mid-entry on a '{' inside data.
    Instead, read line-by-line from failed_offset and skip any that don't
    start with '{'. This preserves line boundaries. */
    while (1) {
        long line_start = ftell(fp);
        if ((size_t)line_start >= curr_log_offset) {
            /* Past write head — nothing valid to drain */
            fclose(fp);
            save_state_if_changed(curr_log_offset, false, false);
            return no_failed_data;
        }
        if (!fgets(line, sizeof(line), fp)) {
            fclose(fp);
            save_state_if_changed(curr_log_offset, false, false);
            return no_failed_data;
        }
        if (line[0] == '{') {
            /* Valid line start found — rewind to line_start for the main loop */
            fseek(fp, (long)line_start, SEEK_SET);
            post_offset = (size_t)line_start;
            break;
        }
        /* Line doesn't start with '{' — skip it and continue */
        ESP_LOGW("TRY_FAILED", "Realign: skipping non-JSON line at offset %zu", (size_t)line_start);
    }

    /* Remove the old post_offset = ftell(fp) and the "File Ptr before get" 
    prints,
    and remove the first fgets() call that happens BEFORE the while(1) loop
     —
    the realign above already positioned fp correctly. */

    // NEW — just check position, don't consume the line
    if (post_offset >= curr_log_offset) {
        fclose(fp);
        ESP_LOGW("TRY_FAILED", "No backlog entries to post (post_offset=%zu == curr=%zu)", 
                post_offset, curr_log_offset);
        save_state_if_changed(curr_log_offset, false, false);
        return no_failed_data;
    }

    /* Read the first line for the main loop to process */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return no_failed_data;
    }

    printf("Starting post from offset: %zu (last log offset: %zu)\n", start_offset, curr_log_offset);

    while (1) {

        /* Post at most BACKLOG_BATCH_SIZE entries per call. This lets the main
       loop return after each batch so collect_and_log() can fire on time
       and write the current cycle's SPIFFS record. Without this cap the
       while(1) blocks for minutes and new sensor data is never written
       during backlog recovery. */
        
        if (post_offset == curr_log_offset) 
        {
            ESP_LOGW("TRY_FAILED","LOGS MATCH - Ending Post Attempts");
            break;
        }

        /* Batch limit reached — save progress and yield to main loop */
        if (batch_count >= BACKLOG_BATCH_SIZE) {
            save_state_if_changed(post_offset, wrapped_once, true);
            fclose(fp);
            ESP_LOGW("TRY_FAILED", "Batch of %d posted — yielding to main loop, more backlog remains", BACKLOG_BATCH_SIZE);
            return backlog_drain_in_progress;
        }
        // Live mode check — fires mid-batch the moment config changes
        // is_wifi_post=true but config now says GSM → WiFi batch must stop
        // is_wifi_post=false but config now says WiFi → GSM batch must stop
        if (is_wifi_post != get_wifi_mode_enabled()) {
            save_state_if_changed(post_offset, wrapped_once, true);
            fclose(fp);
            ESP_LOGW("TRY_FAILED", "%s batch aborted mid-entry — mode switched to %s, offset saved %zu",
                     is_wifi_post ? "WiFi" : "GSM",
                     get_wifi_mode_enabled() ? "WiFi" : "GSM",
                     post_offset);
            return backlog_drain_in_progress;
        }

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';
        
        /* Check for {} verify the integrity of log */
        if (line[0] != '{' || line[strlen(line) - 1] != '}') {
            ESP_LOGW("TRY_FAILED", "Skipping malformed log at offset %zu", post_offset);
            /* MUST advance file pointer before continuing, otherwise the loop
               re-reads the same broken line forever (infinite loop bug). */
            post_offset = ftell(fp);
            if (post_offset >= curr_log_offset) {
                ESP_LOGW("TRY_FAILED", "Malformed entry at EOF — drain complete at %zu", post_offset);
                break;
            }
            if (!fgets(line, sizeof(line), fp)) {
                break;
            }
            continue;
        }

        printf("Trying to Post: [%zu] %.*s\n", post_offset, 17, line);

        esp_err_t post_result;
        if( is_wifi_post )
        {
            post_result = http_send_json(POST_URL, line, HTTP_METHOD_POST);
        }
        else
        {
            post_result = gsm_post_data(line);
        }

        // if (fake_post_fail(line) == ESP_FAIL )
         if (post_result != ESP_OK) 
        {
            // printf("Saving failed offset: %zu (wrap=%d)\n", post_offset, wrapped_once);
            ESP_LOGE("TRY_FAILED", "Post FAILED (err=0x%x) at offset %zu — saving state",
                     (unsigned)post_result, post_offset);
            save_state_if_changed(post_offset, wrapped_once, true);
            fclose(fp);
            return failed_posting_failed_data;
        }
        else{
            ESP_LOGW("TRY_FAILED", "Post success %.*s", 17, line);
            if (is_wifi_post) {
                http_send_json(SECONDARY_URL, line, HTTP_METHOD_POST);
            } else {
                gsm_post_secondary(line);
            }

            // Advance FIRST, then checkpoint — so NVS always holds NEXT record's offset
            post_offset = ftell(fp);

            // Use curr_log_offset (live RAM) not stale file_size for end-of-drain check
            if (!wrapped_once) {
                if (post_offset >= curr_log_offset) {
                    ESP_LOGW("TRY_FAILED", "Reached write head — drain complete");
                    break;
                }
            } else {
                size_t live_file_size = get_file_size();
                if (post_offset >= live_file_size) {
                    post_offset = 0;
                    wrapped_once = true;
                    ESP_LOGW("TRY_FAILED", "Wrapped around log file, resuming from byte 0");
                    fseek(fp, 0, SEEK_SET);
                }
                //add drain-complete guard for wrapped case too
                if (post_offset >= curr_log_offset) {
                    ESP_LOGW("TRY_FAILED", "Reached write head after wrap — drain complete");
                    break;
                }
            }

            success_post_count++;
            batch_count++;
            drain_total++;
            ESP_LOGW("BACKLOG", "[%d/%d this batch | %zu total drained] offset=%zu → %zu",
                    batch_count, BACKLOG_BATCH_SIZE, drain_total, post_offset, curr_log_offset);
            if( success_post_count >= 5)
            {
                success_post_count = 0;
                ESP_LOGI("TRY_FAILED", "Checkpoint at next offset %zu", post_offset);
                save_state_if_changed(post_offset, wrapped_once, true);
            }
        }

        

        /* Read the next line */
        if (!fgets(line, sizeof(line), fp)) {   //After reading, the file pointer moves forward to the end of the line.
            break;
        }

    }

    fclose(fp);
    // failed_offset = curr_log_offset;
    // post_wrap = false;
    // is_failed = false;
    ESP_LOGW("TRY_FAILED", "ALL FAILED DATA POSTED. Total drained this session: %zu | offset: %zu",
         drain_total, curr_log_offset);
    drain_total = 0;
    save_state_if_changed(curr_log_offset, false, false);
    // save_post_state(failed_offset, post_wrap, is_failed);
    return posted_failed_data;
}



post_status_t check_and_try_failed(bool is_wifi_post)
{
    load_post_state_blob();
    ESP_LOGW("CHECK_TRY", "is_failed=%d failed_offset=%zu curr_log=%zu",
             is_failed, failed_offset, get_curr_log_offset());

    if (is_failed)
    {
        if (failed_offset != get_curr_log_offset())
        {
            ESP_LOGW("CHECK_TRY", "Failed data found, offset: %zu", failed_offset);
            return try_post_from_failed(failed_offset, is_wifi_post);
        }
        else
        {
            /* Offsets matched — nothing left, clear stale flag */
            save_state_if_changed(get_curr_log_offset(), false, false);
            return no_failed_data;
        }
    }

    /* is_failed is already clear, but if failed_offset has drifted behind
       curr_log_offset it means a previous successful post advanced the write
       head without updating NVS (can happen on WiFi path).  Silently sync it
       so the next cycle starts from the correct position and no logs are
       re-posted or silently skipped. */
    if (failed_offset != get_curr_log_offset()) {
        ESP_LOGW("CHECK_TRY", "Stale failed_offset %zu != curr_log %zu — syncing NVS.",
                 failed_offset, get_curr_log_offset());
        save_state_if_changed(get_curr_log_offset(), false, false);
    }

    ESP_LOGW("CHECK_TRY", "No failed data to post.");
    return no_failed_data;
}


void set_failed_offset(size_t failed_off)
{
    size_t actual_file_size = get_file_size();

    /* curr_log_offset now always equals file_size after Fix 1,
     * so failed_off should never exceed file_size. If it does,
     * the log write actually failed — anchor to real EOF. */
    if (actual_file_size > 0 && failed_off > actual_file_size) {
        ESP_LOGW("SET_FAILED",
                 "failed_offset %zu > file_size %zu — clamping to file_size",
                 failed_off, actual_file_size);
        failed_off = actual_file_size;
    }

    is_failed      = true;
    failed_offset  = failed_off;
    save_post_state_blob(failed_offset, post_wrap, is_failed);
    ESP_LOGW("SET_FAILED", "failed_offset set to %zu (file_size=%zu)",
             failed_offset, actual_file_size);
}

size_t get_failed_offset()
{
    load_post_state_blob();
    // if( is_failed )
    return failed_offset;
    // else
    // return 0;

}

bool get_is_failed()
{
    load_post_state_blob();
    return is_failed;
}

bool get_post_wrap()
{
    load_post_state_blob();
    return post_wrap;
}

#define AVG_LOG_ENTRY_BYTES  400   /* measured from your logs: entries are ~363-370 bytes */

size_t get_backlog_count(void)
{
    load_post_state_blob();
    if (!is_failed) {
        return 0;
    }
    size_t curr_off = get_curr_log_offset();
    if (curr_off <= failed_offset) {
        return 0;   /* wrap case — non-trivial, return 0 rather than a wrong number */
    }
    size_t byte_range = curr_off - failed_offset;
    /* Integer divide gives approximate entry count — good enough for display/logging */
    return (byte_range + AVG_LOG_ENTRY_BYTES - 1) / AVG_LOG_ENTRY_BYTES;
}

void set_post_status(size_t failed_off, bool wrap, bool failed)
{
    failed_offset = failed_off;
    post_wrap = wrap;
    is_failed = failed;
    save_post_state_blob(failed_offset, post_wrap, is_failed);
}

/* Erase the post states */
void erase_post_state() {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS for erasing: %s", esp_err_to_name(err));
        return;
    }

    // Erase individual keys
    nvs_erase_key(nvs, KEY_FAILED_INDEX);
    nvs_erase_key(nvs, KEY_WRAP_FLAG);
    nvs_erase_key(nvs, "extra_flag");

    // Commit the changes
    nvs_commit(nvs);
    nvs_close(nvs);

    // Reset cached values
    // extern void reset_post_state_cache();
    // reset_post_state_cache();

    ESP_LOGI("NVS", "Post state keys erased successfully");
}



// Optional: Reset cached values inside save_post_state()
// void reset_post_state_cache() 
// {
//     extern size_t last_index;
//     extern bool last_wrap;
//     extern bool last_extra;

//     last_index = (size_t)-1;
//     last_wrap = false;
//     last_extra = false;
// }

/* GSM 

1. INIT GSM
2. Select Sim
3. Ping
3. PING success ready to post
4. PING FAIL store as logs
5. PING SUCCESS - try posting
6. TRY POST for specific time
7. POST SUCCESS - return with success
8. POST FAIL - entry as logs
9. Also, return signla strength

*/