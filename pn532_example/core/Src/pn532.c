#include "pn532.h"

static const char LTAG[] = "PN_532";

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

static esp_err_t pn532_wake_up()
{
    esp_err_t response;
    uint8_t wake_up_command[] = {0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x03, 0xFD, 0xD4, 0x14, 0x01, 0x17, 0x00};
    response = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, wake_up_command, sizeof(wake_up_command), pdSECOND * 10);
    if (response == ESP_OK)
    {
        ESP_LOGI(LTAG, "PN532 Wake Up Successfully!");

        for (int i = 0; i < 3; i++)
        {
            uint8_t uid[15];
            response = i2c_master_read_from_device(I2C_MASTER_NUM,
                                               PN532_I2C_ADDRESS,
                                               uid,
                                               sizeof(uid),
                                               pdSECOND);

            if (response == ESP_OK)
            {
                printf("Received Command: ");
                for (int i = 0; i < sizeof(uid); i++)
                {
                    printf("%02X ", uid[i]);
                }
                printf("\n");
            }
            else
            {
                ESP_LOGE(LTAG, "Error reading UID\n");
                return response;
            }
            vTaskDelay(pdHALF_SECOND);
        }

        return response;

    }else
    {
        ESP_LOGE(LTAG, "Error waking up: %s", esp_err_to_name(response));
        return response;
    }

}

static esp_err_t pn532_rf_config()
{
    esp_err_t response;
    uint8_t rf_config_command[] = {0x00, 0x00, 0xFF, 0x06, 0xFA, 0xD4, 0x32, 0x05, 0xFF, 0xFF, 0x02, 0xF5, 0x00};
    response = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, rf_config_command, sizeof(rf_config_command), pdSECOND * 10);
    if (response == ESP_OK)
    {
        ESP_LOGI(LTAG, "RF Config Command Sent Successfully!");
        for (int i = 0; i < 3; i++)
        {
            uint8_t uid[15];
            response = i2c_master_read_from_device(I2C_MASTER_NUM,
                                               PN532_I2C_ADDRESS,
                                               uid,
                                               sizeof(uid),
                                               pdSECOND);

            if (response == ESP_OK)
            {
                printf("Received Command: ");
                for (int i = 0; i < sizeof(uid); i++)
                {
                    printf("%02X ", uid[i]);
                }
                printf("\n");
            }
            else
            {
                ESP_LOGE(LTAG, "Error reading UID\n");
                return response;
            }
            vTaskDelay(pdHALF_SECOND);
        }

        return response;

    }else
    {
        ESP_LOGE(LTAG, "Error sending RF Config command: %s", esp_err_to_name(response));
        return response;
    }

}

void configuring_pn532() {
    esp_err_t response;

    while (1)
    {
        response = pn532_wake_up();
        if (response == ESP_OK)
        {
            while (1)
            {
                response = pn532_rf_config();
                if (response == ESP_OK)
                {
                    break;
                }
            }
            break;
        }
    }
}

void pn532_example(void *)
{
    i2c_master_init();
    esp_err_t response;
    uint8_t uid[22];
    
    configuring_pn532();

    uint8_t in_list_passive_target_command[] = {0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x01, 0x00, 0xE1, 0x00};
    response = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND * 10);
    if (response == ESP_OK)
    {
        ESP_LOGI(LTAG, "In List Passive Target Command Sent Successfully!");
    }
    else
    {
        ESP_LOGE(LTAG, "Error sending In List Passive Target command: %s", esp_err_to_name(response));
        return;
    }
        
    while (1)
    {
        response = i2c_master_read_from_device(I2C_MASTER_NUM,
                                               PN532_I2C_ADDRESS,
                                               uid,
                                               sizeof(uid),
                                               pdSECOND);

        if (response == ESP_OK)
        {
            uint8_t jewel_tag_detected[] = {0xD5, 0x4B, 0x01};
            uint8_t jewel_tag_released[] = {0xD5, 0x4B, 0x00};

            bool jewel_tag_present = true, jewel_tag_not_present = true;

            for (int i = 6; i < 9; i++)
            {
                if (uid[i] != jewel_tag_detected[i - 6])
                {
                    jewel_tag_present = false;
                }
                else if (uid[i] == jewel_tag_released[i - 6])
                {
                    jewel_tag_not_present = false;
                }

                if (!jewel_tag_present && !jewel_tag_not_present)
                {
                    break;
                }
            }

            if (jewel_tag_present)
            {
                ESP_LOGI(LTAG, "Jewel Tag Detected!");

                char uid_string[26];
                uid_string[0] = '\0';
                printf("Received UID: ");
                for (int i = 10; i < sizeof(uid); i++) {
                    printf("%02X ", uid[i]);
                    sprintf(uid_string + strlen(uid_string), "%02X ", uid[i]);
                }
                printf("\n");

                tx_task();

                char* received_data;
                received_data = rx_task();
                if (received_data != NULL)
                {
                    printf("Received data: %s\n", received_data);
                    size_t len =  strlen("UID: ") + strlen(uid_string) + sizeof('\n') + strlen("QR:\n") + strlen(received_data);
                    char* concatenated_data = (char*)malloc(len * sizeof(char));
                    strcpy(concatenated_data, "UID: ");
                    strcat(concatenated_data, uid_string);
                    strcat(concatenated_data, "\n");
                    strcat(concatenated_data, "\n");
                    strcat(concatenated_data, "QR:\n");
                    strcat(concatenated_data, received_data);
                    printf("%s\n", concatenated_data);
                    mqtt_publish_message("topic", concatenated_data);
                    free(concatenated_data);
                    free(received_data);
                }
                else
                {
                    printf("No data received!\n");
                }

                response = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND * 10);
            }
            else if (!jewel_tag_not_present)
            {
                ESP_LOGI(LTAG, "Jewel Tag Released!");
                response = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND * 10);
            }
        }
        else
        {
            ESP_LOGE(LTAG, "Error reading UID!\n");

            configuring_pn532();   
            response = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND * 10);
        }

        vTaskDelay(pdScanTimeout);

    }
}