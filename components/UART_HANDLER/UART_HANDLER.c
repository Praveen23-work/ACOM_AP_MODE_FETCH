
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "UART_HANDLER.h"
#include "string.h"
#include "ota_handler.h"

uint8_t UART1_rx_frame_len = 1;
uint8_t UART2_rx_frame_len = 1;

uint8_t *UART1_data = NULL;
uint8_t *UART2_data = NULL;

 bool UART1_rx_done = false;
 bool UART2_rx_done = false;

void UART2_init(void) 
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    // We won't use a buffer for sending data.
    //uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 0, 20, &uart1_queue, 0);
    uart_driver_install(UART_PORT2, RX_BUF_SIZE * 2, 0, 0, NULL, 0);   // BUF_SIZE * 10
   // uart_driver_install(UART_NUM_1, 1024 * 2, 1024 * 2, 10, NULL, 0);
 //   ESP_ERROR_CHECK(uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 0, 0, NULL, UART_INTR_ALLOC_FLAG_DMA));
    uart_param_config(UART_PORT2, &uart_config);
    uart_set_pin(UART_PORT2, TXD_PIN_2, RXD_PIN_2, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    gpio_set_pull_mode(RXD_PIN_2, GPIO_PULLUP_ONLY); // internal pull-up 
    gpio_set_pull_mode(TXD_PIN_2, GPIO_FLOATING); // don't pull TX so it
}


void UART1_init(void) 
{
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        // .source_clk = UART_SCLK_APB,     // Higher Clock Rate | Default
        .source_clk = UART_SCLK_REF_TICK,   // Better Baud Rate accuracy
    };
    // We won't use a buffer for sending data.
    //uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 0, 20, &uart1_queue, 0);
    uart_driver_install(UART_PORT1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);   // BUF_SIZE * 10
   // uart_driver_install(UART_NUM_1, 1024 * 2, 1024 * 2, 10, NULL, 0);
 //   ESP_ERROR_CHECK(uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 0, 0, NULL, UART_INTR_ALLOC_FLAG_DMA));
    uart_param_config(UART_PORT1, &uart_config);
    uart_set_pin(UART_PORT1, TXD_PIN_1, RXD_PIN_1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // uart_flush_input(UART_PORT1);
}

void UART0_init(void) 
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    // We won't use a buffer for sending data.
    //uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 0, 20, &uart1_queue, 0);
    uart_driver_install(UART_PORT0, RX_BUF_SIZE * 2, 0, 0, NULL, 0);   // BUF_SIZE * 10
   // uart_driver_install(UART_NUM_1, 1024 * 2, 1024 * 2, 10, NULL, 0);
 //   ESP_ERROR_CHECK(uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 0, 0, NULL, UART_INTR_ALLOC_FLAG_DMA));
    uart_param_config(UART_PORT0, &uart_config);
    uart_set_pin(UART_PORT0, TXD_PIN_0, RXD_PIN_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}



static void UART0_task(void *arg)
{
	uint16_t Rx_buff_size = 1024; // Size of the RX buffer
    // Allocate buffer with one extra byte for null terminator
    uint8_t *data = (uint8_t *) malloc(RX_BUF_SIZE + 1);
    if (!data) {
        ESP_LOGE("UART0", "Failed to allocate memory for UART RX buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI("UART0", "Stack left: %d", uxTaskGetStackHighWaterMark(NULL));

    while (1) 
    {
        // Read data from the UART
        int len = uart_read_bytes(UART_PORT0, data, Rx_buff_size, 20 / portTICK_PERIOD_MS);

        if (len > 0 && len < Rx_buff_size + 1) 
        {
            data[len] = '\0'; // Safe: buffer is RX_BUF_SIZE+1
            // ESP_LOGI("UART2_rx", "Recv str: %s", (char *) data);
			printf("UART0 Received: %s\n", (char *) data);
		}
    }
}



static void UART1_task(void *arg)
{
	uint16_t Rx_buff_size = 1024; // Size of the RX buffer
    // Allocate buffer with one extra byte for null terminator
     UART1_data = (uint8_t *) malloc(RX_BUF_SIZE + 1);
    if (!UART1_data) {
        ESP_LOGE("UART1", "Failed to allocate memory for UART RX buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI("UART1", "Stack left: %d", uxTaskGetStackHighWaterMark(NULL));



    while (1) 
    {
        // Read data from the UART
        // int len = uart_read_bytes(UART_PORT1, UART1_data, Rx_buff_size, 20 / portTICK_PERIOD_MS);
        int  len = uart_read_bytes(UART_PORT1, UART1_data, UART1_rx_frame_len, 20 / portTICK_PERIOD_MS);
        
        if (len >= UART1_rx_frame_len && len < Rx_buff_size + 1) 
        {
            // UART1_data[len] = '\0'; // Safe: buffer is RX_BUF_SIZE+1
			// printf("UART1 Received: %s\n", (char *) UART1_data);

            // printf("len: %d, Rx_buff_size: %d, UART1_rx_frame_len: %d", len, Rx_buff_size, UART1_rx_frame_len);
            // printf("Rxd UART 1:");
            // for( int i = 0; i < len; i++)
            // {
            //     printf(" 0x%02X", UART1_data[i]);
            //     // printf(" %c", UART2_data[i]);
            // }
            // printf("\n");
            ESP_LOG_BUFFER_HEX("UART1 RX", UART1_data, len);
            ESP_LOGI("UART1 RX"," Rxd Valid frame");
            UART1_rx_done = true; // Set the flag to indicate data is ready
		}
        else if ( len >  0)
        {
            ESP_LOGW("UART1 RX"," Rxd Invalid frame len");
            printf("Len: %d, Rx_buff_size: %d, UART1_rx_frame_len: %d\n", len, Rx_buff_size, UART1_rx_frame_len);
            printf("Rxd1 raw of len: %d\n",len);
            // for( int i = 0; i < len; i++)
            // {
            //     printf(" 0x%02X", UART1_data[i]);
            // }
            ESP_LOG_BUFFER_HEX("UART1 RX", UART1_data, len);

            // printf("\n");
        }
    }
}


SemaphoreHandle_t uart2_response_semaphore = NULL;

static void UART2_task(void *arg)
{
    //  uart2_response_semaphore = xSemaphoreCreateBinary();
    // if (uart2_response_semaphore == NULL) {
    //     ESP_LOGE("APP", "Failed to create UART2 semaphore");
    // }
    
	uint16_t Rx_buff_size = 1024; // Size of the RX buffer
    // Allocate buffer with one extra byte for null terminator
    UART2_data = (uint8_t *) malloc(RX_BUF_SIZE + 1);

    if (!UART2_data) {
        ESP_LOGE("UART2", "Failed to allocate memory for UART RX buffer");
        vTaskDelete(NULL);
        return;
    }

    bool connected = false;          // Flag: CONNECT received

    const uint8_t seq_connect[] = "CONNECT\r\n";
    const uint8_t seq_ok[]      = "\r\nOK\r\n";

    // Declare this static outside or before the loop
    static int last_progress = 0;

    ESP_LOGI("UART2", "Stack left: %d", uxTaskGetStackHighWaterMark(NULL));
 /*  
    while (1) 
    {
        // if (xSemaphoreTake(uart2_response_semaphore, portMAX_DELAY)) {
        
        // Read data from the UART
        int len = uart_read_bytes(UART_PORT2, UART2_data, Rx_buff_size, 20 / portTICK_PERIOD_MS);
        
        if (len > UART2_rx_frame_len && len < Rx_buff_size + 1) 
        {
            UART2_data[len] = '\0';     // Safe: buffer is RX_BUF_SIZE+1
            // printf("UART2 Received: %s\n", (char *) UART2_data);
            // ESP_LOGI("UART2 RX"," Rxd Valid frame");
            // printf("Rxd Arr 2:");
            // for( int i = 0; i < len; i++)
            // {
            //     printf(" 0x%02X", UART2_data[i]);
            //     // printf(" %c", UART2_data[i]);
            // }
            // printf("\n");

           

            UART2_rx_done = true; // Set the flag to indicate data is ready
            // ESP_LOGI("UART2_rx", "Recv str: %s", (char *) data);
			// printf("UART2 Received: %s\n", (char *) data);
		}
        // xSemaphoreGive(uart2_response_semaphore);
    // }
    

    }
*/
       while(1)
    {
        int len = uart_read_bytes(UART_PORT2, UART2_data, Rx_buff_size, 20 / portTICK_PERIOD_MS);
        

        if(len <= 0) continue;
        // uart_rxd_bytes+= len;

        // --- OTA inactive mode ---
        if(!OTA_active)
        {
            if (len > UART2_rx_frame_len && len < Rx_buff_size + 1)
            {
                UART2_data[len] = '\0';
                UART2_rx_done = true;
                // ESP_LOGI("UART2_rx", "Received: %s", (char*)UART2_data);
            }

            continue;
        }

        // --- OTA active mode ---
        int index_connect = -1;
        if(!connected)
        {
            // Check for CONNECT\r\n
            index_connect = find_subsequence(UART2_data, len, seq_connect, sizeof(seq_connect)-1);
            if(index_connect != -1)
            {
                total_ota_received = 0;
                connected = true;
                ESP_LOGI("OTA_UART", "CONNECT received, start OTA data");

                // Send leftover bytes after CONNECT
                size_t leftover_len = len - (index_connect + sizeof(seq_connect)-1);
                if(leftover_len > 0)
                {
                    ota_chunk_t chunk;
                    chunk.len = leftover_len;
                    memcpy(chunk.data, &UART2_data[index_connect + sizeof(seq_connect)-1], leftover_len);
                    // xQueueSend(ota_queue, &chunk, portMAX_DELAY);
                    // In UART2_task, after xQueueSend:
                    if (xQueueSend(ota_queue, &chunk, pdMS_TO_TICKS(200)) != pdTRUE) {
                        ESP_LOGE("OTA_UART", "OTA queue still full after 200ms! Lost chunk of %d bytes", chunk.len);
                        // Optionally handle lost data here
                    }
                    total_ota_received += leftover_len;
                }
                continue;
            }
        }

        // --- Revised OTA active mode with size limit ---
       if (connected)
        {

            // if( remaining_bytes <= 5000)
            {
                int index_ok = find_subsequence(UART2_data, len, seq_ok, sizeof(seq_ok)-1);
                if(index_ok != -1)
                {
                    // How many bytes we still need
                    size_t remaining_bytes = expected_ota_file_size - total_ota_received;

                    if( remaining_bytes <= 2048)
                    {
                        // Send all bytes before OK sequence
                        if(index_ok > 0)
                        {
                            ota_chunk_t chunk;
                            chunk.len = index_ok;
                            memcpy(chunk.data, UART2_data, index_ok);
                            // xQueueSend(ota_queue, &chunk, portMAX_DELAY);
                            // In UART2_task, after xQueueSend:
                            if (xQueueSend(ota_queue, &chunk, pdMS_TO_TICKS(200)) != pdTRUE) {
                                ESP_LOGE("OTA_UART", "OTA queue still full after 200ms! Lost chunk of %d bytes", chunk.len);
                                // Optionally handle lost data here
                            }
                            printf("Ok chunk size =  %d\n",index_ok);
                            total_ota_received += index_ok;
                        }

                        ESP_LOGI("OTA_UART", "Full OTA file received! Total bytes: %d", total_ota_received);
                        ota_rx_success = true; // OTA data rx complete
                        connected = false;  // reset for next OTA if needed
                        continue;
                    }
                    printf("FOUND the ok sequence when rxd size = %d\n",total_ota_received+len);
                    
                }

            }
            
             // No OK sequence, send full buffer as chunk
            ota_chunk_t chunk;
            chunk.len = len;
            memcpy(chunk.data, UART2_data, len);
            // xQueueSend(ota_queue, &chunk, portMAX_DELAY);
            // In UART2_task, after xQueueSend:
            if (xQueueSend(ota_queue, &chunk, pdMS_TO_TICKS(200)) != pdTRUE) {
                ESP_LOGE("OTA_UART", "OTA queue still full after 200ms! Lost chunk of %d bytes", chunk.len);
                // Optionally handle lost data here
            }
            total_ota_received += len;

                            // Inside the loop, after updating total_ota_received:
                int progress = (total_ota_received * 100) / expected_ota_file_size;
                if (progress >= last_progress + 25) {
                    last_progress = progress - (progress % 25);  // align to 25% steps
                    ESP_LOGI("OTA_UART", "OTA progress: %d%%", last_progress);
                    if(last_progress != 100 ) continue;
                }
        } 


    }
}


void clear_contentuart1_buff()
{
    if(UART1_data)
    {
        memset(UART1_data, 0, RX_BUF_SIZE + 1);
    }
     UART1_rx_done = false;
}



void start_UART0_task(void)
{
    // Create a task for UART0
    xTaskCreate(UART0_task, "UART0_task", 2048, NULL, 10, NULL);
}

void start_UART1_task(void)
{
    // Create a task for UART1
    xTaskCreate(UART1_task, "UART1_task", 4096, NULL, 10, NULL);
}

TaskHandle_t uart2_task_handle = NULL;

void start_UART2_task(void)
{
    // Create a task for UART2
    if (uart2_task_handle == NULL)   //Avoids recreating already running task
    xTaskCreate(UART2_task, "UART2_task", 4096, NULL, 10, &uart2_task_handle);
}


void stop_UART2_task(void)
{
    if (uart2_task_handle != NULL)
    {
        vTaskDelete(uart2_task_handle);
        uart2_task_handle = NULL;  // reset handle
    }
}




void write_UART0(const char *data)
{
    if (data) {
        uart_write_bytes(UART_PORT0, data, strlen(data));
    } else {
        ESP_LOGE("UART0", "Attempted to write NULL data");
    }
}

// void write_UART1(const char *data)
// {
//     if (data) {
//         uart_write_bytes(UART_PORT1, data, strlen(data));
//     } else {
//         ESP_LOGE("UART1", "Attempted to write NULL data");
//     }
// }

void write_UART1(uint8_t *data, int len)
{
    if (data) 
    {
        int len_tx = uart_write_bytes(UART_PORT1, data, len);
        if( ! (len_tx > 0))
        {
            ESP_LOGE("write_UART1","Failed to send data");
        }
    } else {
        ESP_LOGE("UART1", "Attempted to write NULL data");
    }
}

// void write_UART2(uint8_t *data, int len)
// {
//     if (data) 
//     {
//         uart_write_bytes(UART_PORT2, data, len);
//     } else {
//         ESP_LOGE("UART2", "Attempted to write NULL data");
//     }
// }

void write_UART2(const char *data)
{
    if (data) {
        uart_write_bytes(UART_PORT2, data, strlen(data));
    } else {
        ESP_LOGE("UART2", "Attempted to write NULL data");
    }
}

// void write_UART2(const char *data)
// {
//     if (data) {
//         uart_write_bytes(UART_PORT2, data, strlen(data));
//     } else {
//         ESP_LOGE("UART2", "Attempted to write NULL data");
//     }
// }



void set_UART1_rx_frame_len(uint8_t len)
{
    UART1_rx_frame_len = len;
}

void set_UART2_rx_frame_len(uint8_t len)
{
    UART2_rx_frame_len = len;
    // ESP_LOGE("UART2", "UART2_frame_len set to - %d", UART2_rx_frame_len);
}


esp_err_t get_UART1_data(uint8_t *data, uint16_t timeout_ms)
{
    if( !UART1_data || !data)
    {
        ESP_LOGE("UART1", "Rxd Invalid arguments");
        return ESP_FAIL;
    }

    // while( (timeout_ms > 0 ) && !UART1_rx_done ) 
    uint16_t waited = 0;
     waited = 0;
    while (waited < timeout_ms) 
    {
        
        if (UART1_rx_done) {
            memcpy(data, UART1_data, UART1_rx_frame_len);
            UART1_rx_done = false; // Reset the done flag
            // ESP_LOGE("UART1", "Copied %d bytes from UART1 data", UART1_rx_frame_len);
            return ESP_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Wait for data to be ready
        waited += 10;
    }

    // vTaskDelay(pdMS_TO_TICKS(timeout_ms));
    return ESP_FAIL;
}



esp_err_t get_UART2_data(uint8_t *data, uint16_t timeout_ms, const char *expected_str)
{
    if (!UART2_data || !data || !expected_str) {
        ESP_LOGE("UART2", "FUNC RXD Invalid arguments");
        return ESP_FAIL;
    }

    uint16_t waited = 0;

    while (waited < timeout_ms) 
    {
        if (UART2_rx_done) 
        {
            // Match correctly: search expected_str inside UART data
            if (strstr((const char *)UART2_data, expected_str))  
            {
                memcpy(data, UART2_data, strlen((const char *)UART2_data) + 1);
                UART2_rx_done = false; // Reset flag
                return ESP_OK;
            }
            else if (strstr((const char *)UART2_data, "ERROR"))  
            {
                printf("UART2 Received: %s\n", (char *) UART2_data);
                UART2_rx_done = false;
                return ESP_FAIL;
            }
            else
            {
                ESP_LOGI("GET_UART2", "String not matched yet. Got: %s", UART2_data);
            }
            
            UART2_rx_done = false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        waited += 10;
    }

    return ESP_FAIL;
}


// esp_err_t get_UART2_data(uint8_t *data, uint16_t timeout_ms, const char *expected_str)
// {
//     if( !UART2_data || !data)
//     {
//         ESP_LOGE("UART2", "Rxd Invalid arguments");
//         return ESP_FAIL;
//     }

//     while (timeout_ms > 0 && !UART2_rx_done) 
//     {
//         vTaskDelay(pdMS_TO_TICKS(10)); // Wait for data to be ready
//         timeout_ms -= 10;

//         if (UART2_rx_done) 
//         {
//             // if( strstr( expected_str, (const char *) UART2_data) )
//             if ( strstr((const char *)UART2_data, expected_str) )
//             {
//                 memcpy(data, UART2_data, strlen( (const char *) UART2_data));
//                 UART2_rx_done = false; // Reset the done flag
//                 // ESP_LOGE("UART2", "Copied %d bytes from UART1 data", UART2_rx_frame_len);
//                 return ESP_OK;
//             }
//             else
//             {
//                 ESP_LOGI("GET_UART","String not matched yet");
//             }
//             UART2_rx_done = false;
//         }
//     }



//     return ESP_FAIL;
// }

