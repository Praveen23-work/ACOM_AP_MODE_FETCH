void func(void);

#ifndef GSM_handler
#define GSM_handler

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"


// #define the_URL "http://safewaternetwork.in/chlorinationv2/chlorination.jsp\r\n"
#define failed_URL "http://safewaternetwork.in/chlorinationv2/bulkdata.jsp\r\n"

// #define the_URL "https://gkcsq4xz1k.execute-api.ap-south-1.amazonaws.com/default/sendTelemetryData\r\n"
#define the_URL "http://safewaternetwork.in:8091/v1/acom/monitoring/siteData\r\n"


#define AIRTEL_APN "AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\"\r\n"
#define JIO_APN "AT+QICSGP=1,1,\"jionet\",\"\",\"\"\r\n"

typedef enum {
    no_failed_data,
    posted_failed_data,
    backlog_drain_in_progress,   /* batch done, more backlog entries remain */
    failed_posting_failed_data
} post_status_t;

typedef enum {
    CMD_AT,
    CMD_CFUN_R,
    CMD_CFUN_W,
    CMD_CMEE,
    CMD_GSN,
    CMD_CPIN,
    CMD_CREG,
    CMD_SIG_STREN,
    CMD_init_MAX, // init cmd ends here

    CMD_QICSGP,
    QIACT_W,
    QIACT_R,
    QHTTPCFG_cid,
    QHTTPCFG_reqh,
    QHTTPCFG_resph,
    QHTTP_sslctxid,
    QHTTP_sslversion,
    Q_PING,
    CMD_start_internet_MAX, // Internet settings end here

    CMD_QHTTPURL,
    SET_URL,

    QHTTPPOST_len,
    POST_data,
    CMD_POST_DATA_MAX,

    CMD_QHTTPURL_failed,
    SET_URL_failed,

    CMD_QGPS,
    QGPS_turn_on,
    get_QGPS_LOC,


    CMD_MAX
} modem_cmd_t;



typedef struct {
    const char *cmd;         // AT command string
    uint32_t timeout_ms;     // Timeout for response
    const char *expected;    // Expected response string
    bool expect_response;    // Whether to check for response
    uint8_t retries;         // How many retries if response check fails
} modem_cmd_info_t;



static modem_cmd_info_t modem_cmds[CMD_MAX] = 
{
    [CMD_AT]   =   { "AT\r\n",          4000,  "OK",     true,  3 },
    [CMD_CFUN_R] = { "AT+CFUN?\r\n",    6000,  "+CFUN: 1",     true,  2 },
    [CMD_CFUN_W] = { "AT+CFUN=1,1\r\n", 4000,  "OK",     true,  3 },
    [CMD_CMEE] = { "AT+CMEE=1\r\n",     4000,  "OK",     true,  1 },
    [CMD_GSN]  = { "AT+GSN\r\n",        4000,  "OK",     true,  2 },
    [CMD_CPIN] = { "AT+CPIN?\r\n",      4000,  "READY",  true,  2 },
    [CMD_CREG] = { "AT+CREG?\r\n",      4000,  "OK",     false, 1 }, // No response check
    [CMD_SIG_STREN] = { "AT+CSQ?\r\n",  4000,  "OK",     true,  4 },
    
    [CMD_QICSGP]     = { "AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\"\r\n",  5000,    "OK",     true,  4 },
    [QIACT_W]        = { "AT+QIACT=1\r\n",                                  5000,    "OK",     true,  4 },
    [QIACT_R]        = { "AT+QIACT?\r\n",                                   5000,    "OK",     true,  4 },
    [QHTTPCFG_cid]   = { "AT+QHTTPCFG=\"contextid\",1\r\n",                 5000,    "OK",     true,  2 },
    [QHTTPCFG_reqh]  = { "AT+QHTTPCFG=\"requestheader\",1\r\n",             4000,    "OK",     true,  2 },
    [QHTTPCFG_resph] = { "AT+QHTTPCFG=\"responseheader\",1\r\n",            4000,    "OK",     true,  2 },
    [QHTTP_sslctxid] = { "AT+QHTTPCFG=\"sslctxid\",1\r\n",                  3000,    "OK",     true,  2 },
    [QHTTP_sslversion] = { "AT+QHTTPCFG=\"sslversion\",1,4\r\n",            3000,    "OK",     true,  2 },
    [Q_PING]         = { "AT+QPING=1,\"www.google.com\"\r\n",               15000,   "OK",     true,  1 },

    [CMD_QHTTPURL]   = { "AT+QHTTPURL=58,60\r\n",  10000, "CONNECT", true,  4 }, // Runtime set
    [SET_URL]        = { the_URL,                  10000, "OK",      true,  4 },

    [QHTTPPOST_len]  = { "Len\r\n",    15000,   "CONNECT",      true,  3 }, // Runtime set
    [POST_data]      = { "Data\r\n",   15000,  "+QHTTPPOST:",   true,  2 }, // Runtime set

    [CMD_QHTTPURL_failed]   = { "AT+QHTTPURL=54,60\r\n",  4000,  "CONNECT", true,  1 }, // Must set in runtime
    [SET_URL_failed]        = { failed_URL,               4000,  "OK",      true,  1 }, 

    [CMD_QGPS]      = { "AT+QGPS?\r\n",     5000,  "+QGPS: 1",  true, 2 },
    [QGPS_turn_on]  = { "AT+QGPS=1\r\n",    5000,  "OK",        true, 4 },
    [get_QGPS_LOC]  = { "AT+QGPSLOC=2\r\n", 5000,  "+QGPSLOC:", true,  3 },
    
};


static size_t failed_index = 0;
static bool post_wrap = false;
extern esp_err_t uart_write_modem(const char *cmd, const char *resp, int timeout);

esp_err_t gsm_at();
typedef void (*gps_callback_t)(void);

esp_err_t get_gps_location(double *latitude, double *longitude, gps_callback_t callback);

esp_err_t gsm_init(char *imei);

esp_err_t start_internet(bool sim);
int gsm_signal_strength();
esp_err_t configurl(void);
esp_err_t gsm_post_data(const char *datasend);
void gsm_post_secondary(const char *datasend);  // Fire-and-forget to secondary server
int extract_imei(const char *response, char *imei_out);

bool try_post_all_logs(void);
bool get_wifi_mode_enabled(void);

void set_failed_offset(size_t failed_off);
post_status_t check_and_try_failed(bool is_wifi_post);
post_status_t try_post_from_failed(size_t start_offset, bool is_wifi_post);

esp_err_t gsm_get_network_time(int *year, int *mon, int *day,
                                int *hour, int *min, int *sec, int *wday);

esp_err_t GSM_sleep(void);
esp_err_t GSM_wake(void);
size_t get_failed_offset();
void reset_post_state_cache();
void erase_post_state();
esp_err_t erase_post_state_blob(void);
bool get_is_failed();
bool get_post_wrap();
void set_post_status(size_t failed_off, bool wrap, bool failed);
size_t get_backlog_count(void);

/*  FAKE DATA POST FOR LOGS TEST */
void print_posted_logs(void) ;
void clear_posted_logs(void);
esp_err_t fake_post_fail(char *line);

#endif