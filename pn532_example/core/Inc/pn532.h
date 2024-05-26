#ifndef PN532_H
#define PN532_H

#include <mqtt_pn.h>
#include <uart.h>
#include <freertos/timers.h>
#include <driver/i2c.h>

#define pdSECOND pdMS_TO_TICKS(1000)
#define pdHALF_SECOND pdMS_TO_TICKS(500)
#define pdScanTimeout pdMS_TO_TICKS(50)
#define pdTX_TASK pdMS_TO_TICKS(370)
#define pdTX_TASK_2 pdMS_TO_TICKS(100)


#define I2C_MASTER_SCL_IO 18
#define I2C_MASTER_SDA_IO 17
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 10000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define PN532_I2C_ADDRESS (0x48 >> 1)

void pn532_example(void *);

#endif // PN532_H