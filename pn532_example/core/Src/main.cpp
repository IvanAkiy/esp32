#include "main.h"

#define pdHALF_SECOND pdMS_TO_TICKS(500)
#define pdSECOND pdMS_TO_TICKS(1000)

namespace
{
    const char PN_TAG[] = "PN532";
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

    esp_err_t ret;

    uint8_t wake_up_command[] = {0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x03, 0xFD, 0xD4, 0x14, 0x01, 0x17, 0x00};
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, wake_up_command, sizeof(wake_up_command), pdSECOND*10);
    if (ret == ESP_OK) {
        ESP_LOGI(PN_TAG, "PN532 Wake Up Successfully!");
    } else {
        ESP_LOGE(PN_TAG, "Error waking up: %s", esp_err_to_name(ret));
        return;
    }

    // for(int i = 0; i < 3; i++) {
    //     uint8_t uid[10];
    //     ret = i2c_master_read_from_device(I2C_MASTER_NUM,
    //             PN532_I2C_ADDRESS,
    //             uid,
    //             sizeof(uid),
    //             pdSECOND);
    //     if (ret == ESP_OK) {
    //         printf("Received UID: ");
    //         for (int i = 0; i < sizeof(uid); i++) {
    //             printf("%02X ", uid[i]);
    //         }
    //         printf("\n");
    //     } else {
    //         ESP_LOGE(PN_TAG, "Error reading UID\n");
    //     }

    //     vTaskDelay(pdSECOND);
    // }

    uint8_t rf_config_command[] = {0x00, 0x00, 0xFF, 0x06, 0xFA, 0xD4, 0x32, 0x05, 0xFF, 0xFF, 0x02, 0xF5, 0x00};
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, rf_config_command, sizeof(rf_config_command), pdSECOND*10);
    if (ret == ESP_OK) {
        ESP_LOGI(PN_TAG, "RF Config Command Sent Successfully!");
    } else {
        ESP_LOGE(PN_TAG, "Error sending RF Config command: %s", esp_err_to_name(ret));
        return;
    }

    // for(int i = 0; i < 3; i++) {
    //     uint8_t uid[13];
    //     ret = i2c_master_read_from_device(I2C_MASTER_NUM,
    //             PN532_I2C_ADDRESS,
    //             uid,
    //             sizeof(uid),
    //             pdSECOND);
    //     if (ret == ESP_OK) {
    //         printf("Received UID: ");
    //         for (int i = 0; i < sizeof(uid); i++) {
    //             printf("%02X ", uid[i]);
    //         }
    //         printf("\n");
    //     } else {
    //         ESP_LOGE(PN_TAG, "Error reading UID\n");
    //     }

    //     vTaskDelay(pdSECOND);
    // }


    uint8_t in_list_passive_target_command[] = {0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x01, 0x04, 0xE1, 0x00};
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND*10);
    if (ret == ESP_OK) {
        ESP_LOGI(PN_TAG, "In List Passive Target Command Sent Successfully!\n");
    } else {
        ESP_LOGE(PN_TAG, "Error sending In List Passive Target command: %s\n", esp_err_to_name(ret));
        return;
    }
    
    while (1) {
        uint8_t uid[23];
        ret = i2c_master_read_from_device(I2C_MASTER_NUM,
                    PN532_I2C_ADDRESS,
                    uid,
                    sizeof(uid),
                    pdSECOND);
        
        if (ret == ESP_OK) { 
            printf("UID: ");
            for (int i = 5; i < sizeof(uid); i++) {
                printf("%02X ", uid[i]);
            }
            printf("\n");          
            
            uint8_t detected_uid[] = {0x01, 0x00, 0x00, 0xFF, 0x0F, 0xF1, 0xD5, 0x4B, 0x01};
            uint8_t expected_uid1[] = {0x01, 0x00, 0x00, 0xFF, 0x0F, 0xF1, 0xD5, 0x4B, 0x00};
            bool match_detected = true, match1 = true;
            for (int i = 0; i < 8; i++) {
                if (uid[i] != detected_uid[i]) {
                    match_detected = false;
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
            
            if (match_detected) {
                ESP_LOGI(PN_TAG, "Card detected!");
                

                uint8_t in_list_passive_target_command[] = {0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x01, 0x00, 0xE1, 0x00};
                ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND*10);
            }
            else if (match1)
            {
                ESP_LOGI(PN_TAG, "Card is released!");

                uint8_t in_list_passive_target_command[] = {0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x01, 0x00, 0xE1, 0x00};
                ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND*10);
            }
            
        } else {
            printf("Error reading UID\n");
        }
        
        
    //     if (ret == ESP_OK) {
    //         printf("UID: ");
    //         for (int i = 0; i < sizeof(uid); i++) {
    //             printf("%02X ", uid[i]);
    //         }
    //         printf("\n");
    //         // Compare the UID with the expected UID
    //         uint8_t detected_uid[] = {0x01, 0x00, 0x00, 0xFF, 0x0F, 0xF1, 0xD5, 0x4B, 0x01};
    //         uint8_t expected_uid1[] = {0x01, 0x00, 0x00, 0xFF, 0x0F, 0xF1, 0xD5, 0x4B, 0x00};
    //         bool match_detected = true, match1 = true;
    //         for (int i = 0; i < 8; i++) {
    //             if (uid[i] != detected_uid[i]) {
    //                 match_detected = false;
    //                 break;
    //             }
    //         }
    //         for (int i = 0; i < 8; i++)
    //         {
    //             if (uid[i] == expected_uid1[i])
    //             {
    //                 match1 = false;
    //                 break;
    //             }   
    //         }
            
    //         if (match_detected) {
    //         uint8_t in_list_passive_target_command[] = {0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x01, 0x00, 0xE1, 0x00};
    //             ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND*10);
    //             // if (ret == ESP_OK) {
    //             //         ESP_LOGI(PN_TAG, "In List Passive Target Command Sent Successfully!\n");
    //             // } else {
    //             //         ESP_LOGE(PN_TAG, "Error sending In List Passive Target command: %s\n", esp_err_to_name(ret));
    //             //         // Handle error
    //             // }
    //         }else if (match1)
    //         {
    //             uint8_t in_list_passive_target_command[] = {0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x01, 0x00, 0xE1, 0x00};
    //             ret = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND*10);
    //         }
            
    //     } else {
    //         printf("Error reading UID\n");
    //     }

        vTaskDelay(pdSECOND);
    }
}

extern "C" void app_main(void) 
{
    ESP_LOGI(PN_TAG, "Creating default event loop");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(my_main.setup());
}

Main::Main() {
}

esp_err_t Main::setup(void) 
{
    ESP_LOGI(PN_TAG, "Setup!");
    xTaskCreate(pn532_example, "pn532_example", 4096, NULL, 5, NULL);
    ESP_LOGI(PN_TAG, "Setup completed!");

    return ESP_OK;
}

void Main::loop(void) 
{
    vTaskDelay(pdSECOND);
}