#ifndef UART_H
#define UART_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#define TXD_PIN (GPIO_NUM_11)
#define RXD_PIN (GPIO_NUM_12)

void init(void);
int sendData(const char* logName, const char* data);
void tx_task();
char* rx_task();

#endif