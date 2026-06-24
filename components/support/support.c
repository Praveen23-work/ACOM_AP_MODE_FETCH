#include <stdio.h>
#include "support.h"
#include "driver/gpio.h"
#include "support.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint8_t shift_reg_state = 0;  // Holds current 8-bit register state

void shiftOut(uint8_t val) {
  uint8_t i;
        // gpio_set_level(CLOCK_PIN, 0);
        // gpio_set_level(DATA_PIN, (value >> i) & 0x01);
        // gpio_set_level(CLOCK_PIN, 1);
	gpio_set_level(LATCH_PIN, 0);  // Begin
  for (i = 0; i < 8; i++) {
    //   digitalWrite(dataPin, !!(val & (1 << (7 - i))));
	  gpio_set_level(DATA_PIN, !!(val & (1 << (7 - i))));

    // digitalWrite(clockPin, HIGH);
	gpio_set_level(CLOCK_PIN, 1);
    // digitalWrite(clockPin, LOW);
	gpio_set_level(CLOCK_PIN, 0);
  }
  gpio_set_level(LATCH_PIN, 1);  // Stop
}

void write_SR_all(uint8_t value)
{
	shift_reg_state = shift_reg_state | value;
    // gpio_set_level(LATCH_PIN, 0);  // Begin
    // for (int i = 7; i >= 0; i--) {
    //     gpio_set_level(CLOCK_PIN, 0);
    //     gpio_set_level(DATA_PIN, (value >> i) & 0x01);
    //     gpio_set_level(CLOCK_PIN, 1);
    // }
    // gpio_set_level(LATCH_PIN, 1);  // Latch output
	shiftOut(shift_reg_state);
}

void write_SR(uint8_t pin_bit, uint8_t level)
{
    if (level == HIGH) {
        shift_reg_state |= (1 << pin_bit);
    } else {
        shift_reg_state &= ~(1 << pin_bit);
    }
    // write_SR_all(shift_reg_state);
	shiftOut(shift_reg_state);
	// print_binary(shift_reg_state);
}

