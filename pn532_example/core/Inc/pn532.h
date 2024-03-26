#ifndef PN532_H
#define PN532_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <esp_event.h>
#include <esp_log.h>
#include <driver/i2c.h>

#define pdSECOND pdMS_TO_TICKS(1000)
#define pdHALF_SECOND pdMS_TO_TICKS(500)

#define I2C_MASTER_SCL_IO 18
#define I2C_MASTER_SDA_IO 17
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 1000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

#define PN532_I2C_ADDRESS (0x48 >> 1)

void pn532_example(void*);

#endif // PN532_H