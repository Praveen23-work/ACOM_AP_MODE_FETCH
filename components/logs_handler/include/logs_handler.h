#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"

#define NVS_NAMESPACE "log_state"
#define KEY_FAILED_INDEX "failed_idx"
#define KEY_WRAP_FLAG "wrap_flag"

extern size_t curr_log_offset;
#define MAX_LINE_LEN 512
/**
 * @brief Initialize SPIFFS and scan log file to find last log write position
 *        and initialize internal state for efficient future writes.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t init_log_system(void);

/**
 * @brief Write a new log entry. Handles log rotation automatically when needed.
 *
 * @param log_data A null-terminated string representing the log entry.
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t write_log_entry(const char *log_data);

size_t get_prev_log_offset();

/**
 * @brief Read and print all log entries.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
void print_all_logs(void);
void print_all_logs_timestamp();

/**
 * @brief Delete all logs by erasing the log file.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
void erase_logs(void);

/**
 * @brief Extract timestamp from a log entry string.
 *
 * @param log_entry Full log string.
 * @param timestamp_out Buffer to store extracted timestamp.
 * @param max_len Length of timestamp_out buffer.
 * @return true on successful extraction, false otherwise.
 */
// bool extract_timestamp(const char *log_entry, char *timestamp_out, size_t max_len);
size_t get_file_size();


void format_spiffs(void);

/**
 * @brief Check the last log index and update internal state.
 */
void check_last_log_ind(void);

size_t get_curr_log_offset();

static const char *log_file_path = "/spiffs/logs.txt";

// void send_all_logs();

#endif // LOG_MANAGER_H
