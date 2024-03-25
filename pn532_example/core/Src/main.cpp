#include "main.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include <esp_event.h>
#include <esp_log.h>
#include <driver/i2c.h>

#define pdSECOND pdMS_TO_TICKS(1000)
#define pdHALF_SECOND pdMS_TO_TICKS(500)
namespace
{
    const char LTAG[] = "Main";
    Main my_main;
}

#define I2C_MASTER_SCL_IO 18
#define I2C_MASTER_SDA_IO 17
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 1000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

#define PN532_I2C_ADDRESS (0x48 >> 1)

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = I2C_MASTER_FREQ_HZ,
        },
        .clk_flags = 0,
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);

    return i2c_driver_install(I2C_MASTER_NUM,
                conf.mode, I2C_MASTER_RX_BUF_DISABLE,
                I2C_MASTER_TX_BUF_DISABLE, 0);
}

void pn532_example(void*) {
    i2c_master_init();

    uint8_t wake_up_command[] = {0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x03, 0xFD, 0xD4, 0x14, 0x01, 0x17, 0x00};
    
    esp_err_t ret;

    ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, wake_up_command, sizeof(wake_up_command), pdSECOND*10);
    if (ret == ESP_OK) {
        ESP_LOGI(LTAG, "PN532 Wake Up Successfully!");
    } else {
        ESP_LOGE(LTAG, "Error waking up: %s", esp_err_to_name(ret));
        return;
    }

    for(int i = 0; i < 3; i++) {
        uint8_t uid[10];
        ret = i2c_master_read_from_device(I2C_MASTER_NUM,
                PN532_I2C_ADDRESS,
                uid,
                sizeof(uid),
                pdSECOND);
        if (ret == ESP_OK) {
            printf("Received UID: ");
            for (int i = 0; i < sizeof(uid); i++) {
                printf("%02X ", uid[i]);
            }
            printf("\n");
        } else {
            ESP_LOGE(LTAG, "Error reading UID\n");
        }

        vTaskDelay(pdSECOND);
    }

    uint8_t rf_config_command[] = {0x00, 0x00, 0xFF, 0x06, 0xFA, 0xD4, 0x32, 0x05, 0xFF, 0xFF, 0x02, 0xF5, 0x00};
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, rf_config_command, sizeof(rf_config_command), pdSECOND*10);
    if (ret == ESP_OK) {
        ESP_LOGI(LTAG, "RF Config Command Sent Successfully!");
    } else {
        ESP_LOGE(LTAG, "Error sending RF Config command: %s", esp_err_to_name(ret));
        // Handle error
        return;
    }

    for(int i = 0; i < 3; i++) {
        uint8_t uid[13];
        ret = i2c_master_read_from_device(I2C_MASTER_NUM,
                PN532_I2C_ADDRESS,
                uid,
                sizeof(uid),
                pdSECOND);
        if (ret == ESP_OK) {
            printf("Received UID: ");
            for (int i = 0; i < sizeof(uid); i++) {
                printf("%02X ", uid[i]);
            }
            printf("\n");
        } else {
            ESP_LOGE(LTAG, "Error reading UID\n");
        }

        vTaskDelay(pdSECOND);
    }


    uint8_t in_list_passive_target_command[] = {0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x01, 0x00, 0xE1, 0x00};
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND*10);
    if (ret == ESP_OK) {
        ESP_LOGI(LTAG, "In List Passive Target Command Sent Successfully!\n");
    } else {
        ESP_LOGE(LTAG, "Error sending In List Passive Target command: %s\n", esp_err_to_name(ret));
        // Handle error
        return;
    }
    
    while (1) {
    uint8_t uid[20];
    ret = i2c_master_read_from_device(I2C_MASTER_NUM,
                PN532_I2C_ADDRESS,
                uid,
                sizeof(uid),
                pdSECOND);
    if (ret == ESP_OK) {
        // printf("UID: ");
        // for (int i = 0; i < sizeof(uid); i++) {
        //     printf("%02X ", uid[i]);
        // }
        // printf("\n");


        // Compare the UID with the expected UID
        uint8_t expected_uid[] = {0x01, 0x00, 0x00, 0xFF, 0x0F, 0xF1, 0xD5, 0x4B, 0x01};
        uint8_t expected_uid1[] = {0x01, 0x00, 0x00, 0xFF, 0x0F, 0xF1, 0xD5, 0x4B, 0x00};
        bool match = true, match1 = true;
        for (int i = 0; i < 8; i++) {
            if (uid[i] != expected_uid[i]) {
                match = false;
                break;
            }
        }
        for (int i = 0; i < 8; i++)
        {
            if (uid[i] == expected_uid1[i])
            {
                match1 = false;
                break;
            }   
        }
        
        if (match) {
             ESP_LOGI(LTAG, "Card is detected!");
                printf("Received UID: ");
                for (int i = 6; i < sizeof(uid); i++) {
                    printf("%02X ", uid[i]);
                }
                printf("\n");
            uint8_t in_list_passive_target_command[] = {0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x01, 0x00, 0xE1, 0x00};
            ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND*10);
            // if (ret == ESP_OK) {
            //         ESP_LOGI(LTAG, "In List Passive Target Command Sent Successfully!\n");
            // } else {
            //         ESP_LOGE(LTAG, "Error sending In List Passive Target command: %s\n", esp_err_to_name(ret));
            //         // Handle error
            // }
        }else if (match1)
        {
            ESP_LOGI(LTAG, "Card released!");
            uint8_t in_list_passive_target_command[] = {0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x01, 0x00, 0xE1, 0x00};
            ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND*10);
        }
        
    } else {
        printf("Error reading UID\n");
    }

    vTaskDelay(pdHALF_SECOND);
}
   
}


extern "C" void app_main(void)
{
    ESP_LOGI(LTAG, "Creating default event loop");
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(my_main.setup());

    while (true)
    {
        my_main.loop();
    }
}

Main::Main()
{
}

esp_err_t Main::setup(void)
{
    ESP_LOGI(LTAG, "Setup!");

    xTaskCreate(pn532_example, "pn532_example", 4096, NULL, 5, NULL);

    // write_i2c(PN532_I2C_ADDRESS, 0x32, 0x01);

    // ESP_ERROR_CHECK(i2c_master_init());

    ESP_LOGI(LTAG, "Setup completed!");

    return ESP_OK;
}

void Main::loop(void)
{
    vTaskDelay(pdSECOND);
}
