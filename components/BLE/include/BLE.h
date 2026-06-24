
#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include <stdbool.h>

#define UUID_CONFIG_CHAR 0xDEAD
#define UUID_STATUS_CHAR 0xBEEF
#define UUID_LOG_CHAR    0xCAFE


void ble_init(void);
void ble_notify_status(void);  // Call periodically or on trigger
void ble_send_config(void);    // Called on connect
void ble_process_received_config(const char *json_str);
void ble_process_log_command(const char *cmd_json);
void ble_app_advertise();

void start_BLE(const char *device_name);
void set_ble_name(const char *name);
void status_notify(const char *message);
const char *read_ble(void);
void stop_ble(void);
bool is_ble_connected_();
bool is_ble_active(void);
void set_ble_data(const char *data);
void status_notify_chunks(const char *message);
bool is_ble_rx_done(void);
const char *read_ble_rx_buffer(void);

void reset_ble_rx_buffer(void);
void status_notify_fast(const char *message);
void ble_notify_drain(void);

#endif
