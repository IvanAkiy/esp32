#include "uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_11)
#define RXD_PIN (GPIO_NUM_12)

void init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

void tx_task()
{
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    sendData(TX_TASK_TAG, "1");
}



// char* rx_task()
// {
//     static const char *RX_TASK_TAG = "RX_TASK";
//     esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
//     uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE);
    
//     // if (data == NULL) {
//     //     ESP_LOGE(RX_TASK_TAG, "Failed to allocate memory for data buffer");
//     //     return NULL; // Return NULL to indicate failure
//     // }
    
//     // // memset(data, 0, RX_BUF_SIZE); // Ensure buffer is null-terminated
    
//     int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE - 1, 1000 / portTICK_PERIOD_MS);
//     if (rxBytes > 0) {
//         data[rxBytes] = '\0'; // Null-terminate the received data
//         ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
//         ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
//     } else {
//         free(data);
//         return NULL; // Return NULL if no data is received
//     }
    
//     return (char*)data;
// }


char* rx_task()
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 3000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            // ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            // ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
            return (char*)data;
        }else{
            return NULL;
        }
    }
    free(data);

}
