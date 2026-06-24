#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "logs_handler.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <errno.h>

#include "GSM_handler.h"

#define LOG_TAG "LOG_MANAGER"

// static FILE *log_file = NULL;

// static size_t max_log_file_size = 0xC40000; // 12 MB safe limit
// static size_t max_log_file_size = 0x2C0000; // ~2.75 MB — leaves 190KB headroom on 2.94MB SPIFFS
// static size_t max_log_file_size = 0xB00000; // 11 MB — safe on 12.75 MB partition
static size_t max_log_file_size = 0x800000; // 8 MB safe default until init reads actual
/*ACOM: ~32,700 logs → 22.8 days offline at 1 log/min*/

esp_err_t init_log_system()
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;

    esp_spiffs_info("storage", &total, &used);
    ESP_LOGI(LOG_TAG, "SPIFFS: total=%d, used=%d", total, used);

    /* Set safe limit to 85% of actual usable SPIFFS total.
     * Raw partition size != usable size. SPIFFS metadata overhead
     * consumes ~15-25% of raw space. Writing past usable gives ENOSPC. */
    max_log_file_size = (size_t)(total * 85 / 100);
    ESP_LOGW(LOG_TAG, "max_log_file_size set to %zu bytes (85%% of %zu SPIFFS total)",
             max_log_file_size, total);

    // Create file if not exists
    FILE *f = fopen(log_file_path, "a");
    if (!f)
    {
        ESP_LOGE(LOG_TAG, "Failed to open log file");
        return ESP_FAIL;
    }
    fclose(f);
    return ESP_OK;
}

size_t get_file_size()
{
    struct stat st;
    if (stat(log_file_path, &st) == 0)
    {
        return st.st_size;
    }
    return 0;
}

size_t curr_log_offset = 0;

time_t extract_timestamp(const char *log_line);

void check_last_log_ind() {
    FILE *file = fopen(log_file_path, "r");
    if (!file) {
        ESP_LOGE(LOG_TAG, "Failed to open log file for scanning");
        return;
    }

    size_t file_size = get_file_size();
    if (file_size == 0) {
        ESP_LOGI(LOG_TAG, "Log file is empty, no last log to check");
        fclose(file);
        curr_log_offset = 0; // Reset offset if file is empty
        return;
    }

    /* The file is append-only (until full erase), so the write cursor is EOF.
       Do not infer position from DT ordering; RTC corrections can make DT go
       backward and incorrectly move curr_log_offset to an old location. */
    fclose(file);
    curr_log_offset = file_size;
    ESP_LOGW(LOG_TAG, "Recovered log write offset at EOF: %zu", curr_log_offset);
}

static const char *TAG_LOG_DELETE = "LOG_DELETE";

esp_err_t delete_log_lines_and_check_space(size_t offset, size_t len) {
    FILE *fp = fopen(log_file_path, "r+");
    if (!fp) {
        ESP_LOGE(TAG_LOG_DELETE, "Failed to open log file");
        return ESP_FAIL;
    }

    // Seek to given offset
    fseek(fp, offset, SEEK_SET);

    char line[MAX_LINE_LEN];
    size_t delete_start = offset;
    size_t delete_end = offset;
    int lines_deleted = 0;
    size_t deleted_bytes = 0;

    while (fgets(line, sizeof(line), fp)) {

        delete_end = ftell(fp);  // Mark end after \n
        deleted_bytes = delete_end - delete_start;

        lines_deleted++;

        ESP_LOGI(TAG_LOG_DELETE, "Line %d deleted from %zu to %zu (len: %zu)", lines_deleted, delete_start, delete_end, deleted_bytes);


        if (len <= deleted_bytes ) {
            break;  // New data fits in deleted space
        }
    }

    // Truncate file
    fflush(fp);
    fclose(fp);

    ESP_LOGI(TAG_LOG_DELETE, "Deleted %d lines, total %zu bytes, New OFFSET: %zu", lines_deleted, deleted_bytes, curr_log_offset);
    return ESP_OK;
}


// // Returns timestamp as time_t, or -1 on failure
// time_t extract_timestamp(const char *log_line) {
//     struct tm tm_time = {0};

//     const char *date_ptr = strstr(log_line, "D:");
//     const char *time_ptr = strstr(log_line, "T:");

//     if (!date_ptr || !time_ptr) {
//         return -1;
//     }

//     // Extract date
//     if (sscanf(date_ptr, "D:%4d-%2d-%2d", &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday) != 3) {
//         return -1;
//     }

//     // Extract time
//     if (sscanf(time_ptr, "T:%2d:%2d:%2d", &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec) != 3) {
//         return -1;
//     }

//     tm_time.tm_year -= 1900; // tm_year is years since 1900
//     tm_time.tm_mon -= 1;     // tm_mon is 0-based

//     time_t timestamp = mktime(&tm_time);
//     return timestamp;
// }

time_t extract_timestamp(const char *log_line) {
    const char *dt_ptr = strstr(log_line, "\"DT\":");
    if (!dt_ptr) {
        return -1;
    }

    time_t timestamp;
    if (sscanf(dt_ptr, "\"DT\":%lld", &timestamp) != 1) {
        return -1;
    }

    return timestamp; // Already a UNIX timestamp
}

/*
   	    1 [0]   LOG_6
        2 [100] LOG_7
        3 [200] LOG_3
        4 [300] LOG_4
        5 [400] LOG_5
                        //	 prev_log_offset      curr_offset
                        //		   |				   |
                        //         v                   V			
                               { [100] +  LOG_7   =  [200] }

        Full wrap around of failed data -
        failed_data = [110] or [200] LOG_2   now overwritten by LOG_7
        update failed offset to next avail valid log
*/

void verify_failed_offset(size_t current_data_len)
{
    size_t failed_offset = get_failed_offset();
	bool failed_flag = get_is_failed();
	bool post_wrap = get_post_wrap();

	if( failed_flag && (failed_offset == curr_log_offset) )
	{
        ESP_LOGW("LOGS_HANDLER","Failed Offset had a wrap, incrementing failed offset");
		set_post_status(curr_log_offset + current_data_len, post_wrap, failed_flag);
	}

}

size_t prev_log_offset = 0;

size_t get_prev_log_offset()
{
    return prev_log_offset;
}


esp_err_t write_log_entry(const char *new_log)
{
    size_t new_log_len = strlen(new_log) + 1; // +1 for '\n'
    size_t file_size   = get_file_size();

    ESP_LOGI(LOG_TAG, "Log entry length: %d, current file size: %d",
             new_log_len, file_size);

    /* Check against dynamic limit set in init_log_system() */
    if (file_size + new_log_len >= max_log_file_size)
    {
        ESP_LOGW(LOG_TAG, "Log file at limit (%zu >= %zu) — erasing logs and restarting.",
                 file_size, max_log_file_size);
        erase_logs();
        set_post_status(0, false, false);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    FILE *f = fopen(log_file_path, "r+b");
    if (!f)
    {
        f = fopen(log_file_path, "wb");
        if (!f)
        {
            ESP_LOGE(LOG_TAG, "fopen failed (both r+b and wb) errno=%d — log NOT written", errno);
            return ESP_FAIL;
        }
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        ESP_LOGE(LOG_TAG, "fseek to EOF failed errno=%d", errno);
        fclose(f);
        return ESP_FAIL;
    }

    size_t to_write = strlen(new_log);
    size_t wrote    = fwrite(new_log, 1, to_write, f);
    if (wrote != to_write)
    {
        ESP_LOGE(LOG_TAG, "fwrite body failed: wrote=%d expected=%d errno=%d",
                 wrote, to_write, errno);
        fclose(f);

        if (errno == ENOSPC)
        {
            /* SPIFFS hit its physical wall — metadata overhead consumed
             * more space than expected. format_spiffs() does a full
             * low-level erase of every block, which erase_logs() cannot
             * do. After format the limit is recalculated on next boot. */
            ESP_LOGE(LOG_TAG, "ENOSPC — formatting SPIFFS and restarting");
            format_spiffs();
            set_post_status(0, false, false);
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        }
        return ESP_FAIL;
    }

    if (fwrite("\n", 1, 1, f) != 1)
    {
        ESP_LOGE(LOG_TAG, "fwrite newline failed errno=%d", errno);
        fclose(f);
        return ESP_FAIL;
    }

    if (fflush(f) != 0)
    {
        ESP_LOGE(LOG_TAG, "fflush failed errno=%d", errno);
        fclose(f);
        return ESP_FAIL;
    }

    fclose(f);

    size_t new_size = get_file_size();
    if (new_size <= file_size)
    {
        ESP_LOGE(LOG_TAG,
                 "SPIFFS size did not advance (before=%zu after=%zu) — "
                 "formatting and restarting",
                 file_size, new_size);
        format_spiffs();
        set_post_status(0, false, false);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    prev_log_offset = curr_log_offset;
    curr_log_offset = new_size;

    ESP_LOGI(LOG_TAG, "prev_log_offset: %zu | curr_log_offset: %zu | file_size: %zu",
             prev_log_offset, curr_log_offset, new_size);
    return ESP_OK;
}

void print_all_logs()
{
    FILE *log_file = fopen(log_file_path, "r");
    if (!log_file)
    {
        ESP_LOGE(LOG_TAG, "Unable to open log file for reading");
        return;
    }

    ESP_LOGW("print_all_logs","Printing all logs");
    char line[256];
    while (fgets(line, sizeof(line), log_file))
    {
        printf("%s", line);
    }
    fclose(log_file);
}


void print_all_logs_timestamp()
{
    FILE *log_file = fopen(log_file_path, "r");
    if (!log_file)
    {
        ESP_LOGE(LOG_TAG, "Unable to open log file for reading");
        return;
    }

    size_t off = get_failed_offset();
    bool failed = get_is_failed();

    ESP_LOGE("print_all_logs", "failed_offset = %d | is_failed %d",off,failed);

    ESP_LOGW("print_all_logs", "Printing all logs");
    char line[400];
    uint32_t count = 0;

    while (1) {
        long offset = ftell(log_file);   // <-- position before reading
        if (!fgets(line, sizeof(line), log_file)) {
            break; // EOF
        }

        // Look for "DT": field
        char *dt_ptr = strstr(line, "\"DT\":");
        if (dt_ptr) {
            long timestamp = strtol(dt_ptr + 5, NULL, 10);
            printf("[%lu] [%ld] %ld\n", count, offset, timestamp);
        } else {
            printf("[%lu] [%ld] (no timestamp)\n", count, offset);
        }

        count++;
    }

    fclose(log_file);
}



void erase_logs(void)
{
    // After format_spiffs(), files don't exist yet — "w" creates them.
    // Use "wb" to avoid any append-mode SPIFFS quirks.
    FILE *f = fopen(log_file_path, "wb");
    if (f)
    {
        fclose(f);
        curr_log_offset = 0;
        prev_log_offset = 0;
        ESP_LOGI("SPIFFS", "Log file cleared: %s", log_file_path);
    }
    else
    {
        // Only warn, not error — file may not exist yet after format
        ESP_LOGW("SPIFFS", "Could not open %s for clearing (errno=%d) — "
                 "may not exist yet after format, will be created on next write",
                 log_file_path, errno);
        curr_log_offset = 0;
        prev_log_offset = 0;
    }
}


void format_spiffs(void) {
    esp_err_t ret = esp_spiffs_format("storage"); // NULL for default partition
    if (ret == ESP_OK) {
        ESP_LOGI("SPIFFS", "SPIFFS partition formatted successfully");
    } else {
        ESP_LOGE("SPIFFS", "Failed to format SPIFFS partition: %s", esp_err_to_name(ret));
    }
}

size_t get_curr_log_offset()
{
    return curr_log_offset;
} 

void set_max_log_size(size_t max_size)
{
    max_log_file_size = max_size;
} 

size_t get_max_log_size()
{
    return max_log_file_size;
}

void update_last_log_offset()
{
    curr_log_offset = get_file_size();
}