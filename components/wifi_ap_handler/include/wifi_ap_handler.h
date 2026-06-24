#ifndef WIFI_AP_HANDLER_H
#define WIFI_AP_HANDLER_H

#include <stdbool.h>

#define AP_SSID "ACOM"
#define AP_PASS "12345678"

extern volatile bool ap_mode_active;
extern volatile bool ap_download_done;

void start_ap_mode(void);
void stop_ap_mode(void);

#endif