#include <stdint.h>
#include <stdio.h>
#include <sys/param.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include "quirc.h"
#include "esp_camera.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <string.h>

#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"



static const char *TAG = "Camera";
#define MOUNT_POINT "/sdcard"

#define ECHO_TEST_TXD (GPIO_NUM_1)
#define ECHO_TEST_RXD (GPIO_NUM_3)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      UART_NUM_0
#define ECHO_UART_BAUD_RATE     115200


#define BUF_SIZE (128)
volatile bool suspend_processing_task = false;
volatile bool clear_queue = false;

const char *special_message = "camera";

#define IMG_WIDTH 320
#define IMG_HEIGHT 240

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22


static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 13000000, //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_GRAYSCALE, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 13, //0-63 lower number means higher quality
    .fb_count = 1,      //if more than one, i2s runs in continuous mode. Use only with JPEG
    // .fb_location = CAMERA_FB_IN_PSRAM
    .grab_mode = CAMERA_GRAB_LATEST
};

TaskHandle_t processing_task_handle = NULL;
static int count = 0;
QueueHandle_t processing_queue;

//static void rgb565_to_grayscale_buf(const uint8_t *src, uint8_t *dst, int qr_width, int qr_height);
static void processing_task(void *arg);
static void main_task(void *arg);
static esp_err_t init_camera();
static esp_err_t init_sdcard();
static void init_uart();
static void write_to_sdcard(camera_fb_t *pic, char decode_status);

void app_main(void)
{
    init_uart();
    init_camera();
    init_sdcard();
    //gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT); // Set the LED pin as an output
    xTaskCreatePinnedToCore(&main_task, "main", 3072, NULL, 5, NULL, 0);
}

static void main_task(void *arg)
{
    // The queue for passing camera frames to the processing task
    processing_queue = xQueueCreate(1, sizeof(camera_fb_t *));
    assert(processing_queue);
    camera_fb_t *pic;

    int task_created = xTaskCreatePinnedToCore(&processing_task, "processing", 24500, processing_queue, 5, &processing_task_handle,1);
    if (task_created != pdPASS)
    {   
        ESP_LOGI(TAG, "Failed to create task!");
    }
    // The main loop waiting for rx messages, and decoding qr code
    while(1){

            while (1) {
                
                    pic = esp_camera_fb_get();

                    if (pic == NULL) {
                        ESP_LOGE(TAG, "Get frame failed");
                    }
                    ESP_LOGI(TAG, "After sendins  Heap: %d  Stack free: %d  ",
                            heap_caps_get_free_size(MALLOC_CAP_DEFAULT), uxTaskGetStackHighWaterMark(NULL));
                    // Sending frame to the procesing task queue
                    int res = xQueueSend(processing_queue, &pic, pdMS_TO_TICKS(10));
                    if (res == pdFAIL) {
                        esp_camera_fb_return(pic);
                    }
                    
                    write_to_sdcard(pic, 'k');

            }
                
        }
}


// Processing task: gets an image from the queue, runs QR code detection and recognition.
static void processing_task(void *arg)
{
    struct quirc *qr = quirc_new();
    assert(qr);

    int qr_width = IMG_WIDTH;
    int qr_height = IMG_HEIGHT;
    camera_fb_t *pic;
    if (quirc_resize(qr, qr_width, qr_height) < 0) {
        ESP_LOGE(TAG, "Failed to allocate QR buffer");
        return;
    }
    QueueHandle_t input_queue = (QueueHandle_t) arg;
    
    while (1) {
        if(clear_queue){
            clear_queue=false;
            for (int i = 0; i < uxQueueMessagesWaiting(input_queue); i++)
            {
                xQueueReceive(input_queue, &pic, portMAX_DELAY);
                esp_camera_fb_return(pic);
                pic= NULL;
            }
        }
        uint8_t *qr_buf = quirc_begin(qr, NULL, NULL);
        
        // Get the next frame from the queue
        int res = xQueueReceive(input_queue, &pic, portMAX_DELAY);
        assert(res == pdPASS);

        int64_t t_start = esp_timer_get_time();
        memcpy(qr_buf, pic->buf, pic->height * pic->width);

        // Return the frame buffer to the camera driver ASAP to avoid DMA errors
        esp_camera_fb_return(pic);

        // Process the frame. This step find the corners of the QR code (capstones)
        quirc_end(qr);
        int64_t t_end_find = esp_timer_get_time();
        int count = quirc_count(qr);
        quirc_decode_error_t err = QUIRC_ERROR_DATA_UNDERFLOW;
        int time_find_ms = (int)(t_end_find - t_start) / 1000;
        for (int i = 0; i < count; i++) {
            struct quirc_code code = {};
            struct quirc_data qr_data = {};
            // Extract raw QR code binary data (values of black/white modules)
            quirc_extract(qr, i, &code);
            // Decode the raw data. This step also performs error correction.
            err = quirc_decode(&code, &qr_data);
            int64_t t_end = esp_timer_get_time();
            int time_decode_ms = (int)(t_end - t_end_find) / 1000;
            if (err != 0) {

            } else {
                ESP_LOGW(TAG, "%s*", qr_data.payload);
                uart_flush(ECHO_UART_PORT_NUM);
                uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
                int len = uart_read_bytes(ECHO_UART_PORT_NUM, data,  (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
                if(strstr((char *)data, special_message) != NULL) {
                    uint8_t *uart_buffer = (uint8_t *)malloc(BUF_SIZE);
                    memcpy(uart_buffer, qr_data.payload, qr_data.payload_len);
                    int some = uart_write_bytes(ECHO_UART_PORT_NUM, uart_buffer, qr_data.payload_len);
                    free(uart_buffer);
                }
                
                // Clear the camera frame queue
                for (int i = 0; i < uxQueueMessagesWaiting(input_queue); i++)
                {
                    xQueueReceive(input_queue, &pic, portMAX_DELAY);
                    esp_camera_fb_return(pic);
                }
                memset(qr_buf, 0, qr_width * qr_height);
                free(data);
            }
        }
        
    }
}

static esp_err_t init_camera()
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }
    return ESP_OK;
}

static void init_uart(){
    //Initializing the uart communication
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    int intr_alloc_flags = 0; 

    uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags);
    uart_param_config(ECHO_UART_PORT_NUM, &uart_config);
    uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);

    #if CONFIG_UART_ISR_IN_IRAM 
        intr_alloc_flags = ESP_INTR_FLAG_IRAM; 
    #endif
}

static esp_err_t init_sdcard()
{
  esp_err_t ret = ESP_FAIL;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
  };
  sdmmc_card_t *card;


  const char mount_point[] = MOUNT_POINT;
  ESP_LOGI(TAG, "Initializing SD card");

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  ESP_LOGI(TAG, "Mounting SD card...");
  gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
  gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
  gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
  gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
  gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

  ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

  return ret;
}



static void write_to_sdcard(camera_fb_t *pic, char decode_status) {
    // SAVE IMG TO SD CARD
    static uint64_t counter = 0;
    counter++;
    char *pic_name = malloc(30 + sizeof(int64_t));
    sprintf(pic_name, MOUNT_POINT"/p_%lli_%c.jpg", counter, decode_status);
    size_t jpg_buf_len;
    uint8_t *jpg_buf;
    // Encode raw image to JPEG
    bool jpeg_success = frame2jpg(pic, 80, &jpg_buf, &jpg_buf_len);
    
    FILE *file = fopen(pic_name, "w");
    if (file != NULL)
    {
        if (jpeg_success)
        {
            // Write JPEG data to file
            fwrite(jpg_buf, 1, jpg_buf_len, file);
            free(jpg_buf);
        }
        else
        {
            ESP_LOGE(TAG, "JPEG encoding failed!");
        }
        fclose(file);
    }
    else
    {
        ESP_LOGE(TAG, "Could not open file!");
    }
    free(pic_name);
}
