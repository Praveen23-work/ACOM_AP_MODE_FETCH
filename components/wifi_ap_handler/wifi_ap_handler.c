#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "wifi_ap_handler.h"
#include "logs_handler.h"
#include "cJSON.h"

// #define MAX_CSV_FIELDS 40
// #define MAX_FIELD_NAME_LEN 16
static const char *const CSV_FIELDS[] = {
    "DT", "Device", "D_type", "IMEI", "FR_v", "Lat", "Lon", "Bat", "Sol",
    "PSR", "PH", "TDS", "TPW", "CLO_PC", "CLO_NC", "WGT", "TBD",
    "FLOW_SW", "SW2", "UFM-T", "UFM1F", "UFM1V", "UFM2F", "UFM2V",
    "TPA", "FLW", 
};
#define CSV_FIELD_COUNT (sizeof(CSV_FIELDS) / sizeof(CSV_FIELDS[0]))

static const char *TAG = "AP_HANDLER";

volatile bool ap_mode_active = false;
volatile bool ap_download_done = false;

static httpd_handle_t server = NULL;

/* Simple HTML page with a download button */
static const char index_html[] =
"<!DOCTYPE html><html><head><title>ACOM Log Download</title></head>"
"<body style='font-family:sans-serif;text-align:center;margin-top:50px;'>"
"<h2>ACOM Device - Log Download</h2>"
"<p>Tap below to download the log file.</p>"
"<a href=\"/download\"><button style='padding:15px 30px;font-size:18px;'>Download Logs</button></a>"
"</body></html>";

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
}

/* Chunked file streaming to avoid loading 8MB into RAM */
#define DL_CHUNK_SIZE 4096

static void ap_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        /* Any client disconnect from the AP — schedule restart.
         * This covers: browser closed, phone moved away, download complete +
         * browser navigated away, and the explicit post-download case.
         * ap_download_done flag also triggers restart from main loop for the
         * clean-download path.  Either path leads to esp_restart(). */
        ESP_LOGW("AP_HANDLER", "Client disconnected from AP — scheduling restart");
        ap_download_done = true;   /* main loop sees this and calls esp_restart() */
    }
}

static esp_err_t download_get_handler(httpd_req_t *req)
{
    FILE *f = fopen(log_file_path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open log file for download");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"logs.csv\"");

    char line[512];
    char out_buf[1024];
    esp_err_t res = ESP_OK;

    /* Write fixed header row first */
    {
        int off = 0;
        for (size_t i = 0; i < CSV_FIELD_COUNT; i++) {
            off += snprintf(out_buf + off, sizeof(out_buf) - off, "%s%s",
                             CSV_FIELDS[i], (i < CSV_FIELD_COUNT - 1) ? "," : "\n");
        }
        if (httpd_resp_send_chunk(req, out_buf, off) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0 || line[0] != '{') continue;  /* skip malformed/blank lines */

        cJSON *root = cJSON_Parse(line);
        if (!root) {
            ESP_LOGW(TAG, "Skipping unparsable line during CSV export");
            continue;
        }

        int off = 0;
        for (size_t i = 0; i < CSV_FIELD_COUNT; i++) {
            cJSON *val = cJSON_GetObjectItemCaseSensitive(root, CSV_FIELDS[i]);
            char field_str[64] = "";

            if (val) {
                if (cJSON_IsNumber(val)) {
                    if (val->valuedouble == (long long)val->valuedouble) {
                        snprintf(field_str, sizeof(field_str), "%lld", (long long)val->valuedouble);
                    } else {
                        snprintf(field_str, sizeof(field_str), "%.6f", val->valuedouble);
                    }
                } else if (cJSON_IsString(val)) {
                    if (strchr(val->valuestring, ',')) {
                        snprintf(field_str, sizeof(field_str), "\"%s\"", val->valuestring);
                    } else {
                        snprintf(field_str, sizeof(field_str), "%s", val->valuestring);
                    }
                } else if (cJSON_IsBool(val)) {
                    snprintf(field_str, sizeof(field_str), "%s", cJSON_IsTrue(val) ? "1" : "0");
                }
                /* else: leave empty for null/unsupported types */
            }
            /* if val is NULL (key missing in this entry, e.g. old log lacking FLOW_SW),
               field_str stays "" — column still present, cell just empty */

            off += snprintf(out_buf + off, sizeof(out_buf) - off, "%s%s",
                             field_str, (i < CSV_FIELD_COUNT - 1) ? "," : "\n");

            if (off > (int)sizeof(out_buf) - 80) {
                if (httpd_resp_send_chunk(req, out_buf, off) != ESP_OK) {
                    res = ESP_FAIL;
                    break;
                }
                off = 0;
            }
        }

        cJSON_Delete(root);

        if (res != ESP_OK) break;

        if (off > 0) {
            if (httpd_resp_send_chunk(req, out_buf, off) != ESP_OK) {
                /* EPIPE / ECONNRESET — client closed the connection.
                 * The file was being transferred; treat this as "done"
                 * so the device restarts rather than hanging in AP mode
                 * forever waiting for a clean transfer that never comes. */
                ESP_LOGW(TAG, "Client disconnected mid-transfer — scheduling restart anyway");
                fclose(f);
                ap_download_done = true;
                return ESP_FAIL;
            }
        }
    }

    /* Terminal chunk — ignore send error here too (client may already be gone) */
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(f);

    ESP_LOGW(TAG, "CSV download complete — scheduling restart");
    ap_download_done = true;
    return ESP_OK;
}

static httpd_handle_t start_webserver(void);

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;     /* file I/O needs more stack than default 4096 */
    config.max_uri_handlers = 4;

    httpd_handle_t srv = NULL;
    if (httpd_start(&srv, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return NULL;
    }

    httpd_uri_t index_uri = {
        .uri = "/", .method = HTTP_GET, .handler = index_get_handler
    };
    httpd_uri_t download_uri = {
        .uri = "/download", .method = HTTP_GET, .handler = download_get_handler
    };

    httpd_register_uri_handler(srv, &index_uri);
    httpd_register_uri_handler(srv, &download_uri);

    return srv;
}


void start_ap_mode(void)
{
    ESP_LOGW(TAG, "Starting SoftAP mode: SSID=%s PASS=%s", AP_SSID, AP_PASS);

    ap_mode_active   = true;
    ap_download_done = false;

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    /* Register station-disconnect event so ANY client leaving triggers restart */
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED,
                                ap_wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len       = strlen(AP_SSID),
            .channel        = 1,
            .max_connection = 2,
            .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.ap.ssid,     AP_SSID, sizeof(wifi_config.ap.ssid));
    strncpy((char *)wifi_config.ap.password, AP_PASS,  sizeof(wifi_config.ap.password));

    if (strlen(AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();

    server = start_webserver();
    ESP_LOGW(TAG, "AP started. Connect to SSID '%s', open http://192.168.4.1", AP_SSID);
}

void stop_ap_mode(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED,
                                  ap_wifi_event_handler);
    esp_wifi_stop();
    ap_mode_active = false;
}
