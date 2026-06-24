#ifndef SUPPROT_H
#define SUPPORT_H

#define HIGH 1
#define LOW 0

// #define DEVICE_ID "ACOM_D3-CYAM_OFC"
#define DEVICE_TYPE "ACOM"
#define DEVICE_ID "ACOM_NEW"

#define DATA_PIN   26  // DS
#define LATCH_PIN  32  // STCP
#define CLOCK_PIN  27  // SHCP

// Define all the pinouts
#define IO_RS485_en   0    // Q0
#define IO_12V  1         // Q1
#define Lamp 2          // Q2
#define SEN_RS485_en 3    // Q3
#define IO_RS485_REDE 4  // Q4
#define SEN_RS485_REDE 5  // Q5
#define GSM_en 6            // Q6
#define LED 7               // Q7

extern uint8_t shift_reg_state;

void write_SR(uint8_t pin_bit, uint8_t level);
void delay_ms(uint32_t ms);

#endif