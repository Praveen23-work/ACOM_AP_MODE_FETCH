#ifndef UART_HANDL
#define UART_HANDL

// // #include "esp_system.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define TXD_PIN_0 (GPIO_NUM_1)
#define RXD_PIN_0 (GPIO_NUM_3)

#define TXD_PIN_1 (GPIO_NUM_5)
#define RXD_PIN_1 (GPIO_NUM_4)

#define TXD_PIN_2 (GPIO_NUM_17)
#define RXD_PIN_2 (GPIO_NUM_16)

#define UART_PORT0 UART_NUM_0
#define UART_PORT1 UART_NUM_1
#define UART_PORT2 UART_NUM_2

static const int RX_BUF_SIZE = 2048;

extern uint8_t *UART1_data;
extern uint8_t *UART2_data;

void UART0_init(void);
void UART1_init(void);
void UART2_init(void);

void start_UART0_task(void);
void start_UART1_task(void);
void start_UART2_task(void);
void stop_UART2_task(void);
void clear_contentuart1_buff();

void write_UART0(const char *data);
// void write_UART1(const char *data);
void write_UART1(uint8_t *data, int len);
// void write_UART2(uint8_t *data, int len);
void write_UART2(const char *data);
// void write_UART2(const char *data);

extern bool UART1_rx_done;
extern bool UART2_rx_done;

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t uart2_response_semaphore;

/* For MODBUS */
void set_UART1_rx_frame_len(uint8_t len);
void set_UART2_rx_frame_len(uint8_t len);

esp_err_t get_UART1_data(uint8_t *data, uint16_t timeout_ms);

esp_err_t get_UART2_data(uint8_t *data, uint16_t timeout_ms, const char *expected_str);

#endif