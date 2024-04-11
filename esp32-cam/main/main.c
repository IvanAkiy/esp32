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

static const char *TAG = "Camera:";

#define ECHO_TEST_TXD (GPIO_NUM_1)
#define ECHO_TEST_RXD (GPIO_NUM_3)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      UART_NUM_0
#define ECHO_UART_BAUD_RATE     115200


#define BUF_SIZE (128)
volatile bool suspend_processing_task = false;
const char *special_message = "camera";


#define LED_GPIO_PIN 4 // Define the LED pin on ESP32 AI Thinker module

#define IMG_WIDTH 240
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

    .xclk_freq_hz = 10000000, //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_240X240,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 7, //0-63 lower number means higher quality
    .fb_count = 1,       //if more than one, i2s runs in continuous mode. Use only with JPEG
    .grab_mode = CAMERA_GRAB_LATEST
};

TaskHandle_t processing_task_handle = NULL;
static int count = 0;
QueueHandle_t processing_queue;

void print_uint8_ascii(uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        ESP_LOGI(TAG, "%c", data[i]);
    }
}

static uint8_t rgb565_to_grayscale(const uint8_t *img);
static void rgb565_to_grayscale_buf(const uint8_t *src, uint8_t *dst, int qr_width, int qr_height);
static void processing_task(void *arg);
static void main_task(void *arg);
static esp_err_t init_camera();

void app_main(void)
{
    gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT); // Set the LED pin as an output
    xTaskCreatePinnedToCore(&main_task, "main", 4096, NULL, 5, NULL, 0);
}

static void main_task(void *arg)
{
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

    
    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags)); 
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config)); 
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

#if CONFIG_UART_ISR_IN_IRAM 
    intr_alloc_flags = ESP_INTR_FLAG_IRAM; 
#endif


    init_camera();
    
    // The queue for passing camera frames to the processing task
    processing_queue = xQueueCreate(1, sizeof(camera_fb_t *));
    assert(processing_queue);
    camera_fb_t *pic;

    // The main loop waiting for rx messages, and decoding qr code
    
    while(1){
        gpio_set_level(LED_GPIO_PIN, 0); // Turn off the LED
        uint8_t *data = (uint8_t *) malloc(BUF_SIZE); 

        // checking for the message from rx
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, data,  (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        // if (len > 0) {
        if(strstr((char *)data, special_message) != NULL){
            // ESP_LOGI(TAG, "After reading from rx");
            // print_uint8_ascii(data, BUF_SIZE);
            if (count == 0)
            {
                int task_created = xTaskCreate(&processing_task, "processing", 35000, processing_queue, tskIDLE_PRIORITY, &processing_task_handle);
                if (task_created != pdPASS)
                {   
                    ESP_LOGI(TAG, "Failed to create task!");
                }
                count++;
            } else
            {
                vTaskResume(processing_task_handle);
            }
            
            ESP_LOGI(TAG, "Processing task started");
            // ESP_LOGI(TAG, "len %d", len);
            // loop to get frames from the camera
            while (1) {

                pic = esp_camera_fb_get();
                if (pic == NULL) {
                    ESP_LOGE(TAG, "Get frame failed");
                    continue;
                }

                int res = xQueueSend(processing_queue, &pic, pdMS_TO_TICKS(10));

                if (res == pdFAIL) {
                    esp_camera_fb_return(pic);
                }

                if(suspend_processing_task) {
                    gpio_set_level(LED_GPIO_PIN, 1); // Turn on the LED
                    esp_camera_fb_return(pic);
                    suspend_processing_task = false;
                    len = 0;   
                    vTaskSuspend(processing_task_handle);
                    BaseType_t reset_processing_queue = xQueueReset(processing_queue);
                    if (reset_processing_queue == pdPASS)
                    {
                        ESP_LOGI(TAG, "Processing_queue reset successful");
                        // Check if the queue is empty
                        UBaseType_t items_waiting = uxQueueMessagesWaiting(processing_queue);

                        if (items_waiting == 0) {
                            ESP_LOGI(TAG, "Processing_queue is empty after reset");
                        } else {
                            ESP_LOGI(TAG, "Processing_queue is not empty after reset, items waiting: %u", items_waiting);
                        }

                    }else{
                        ESP_LOGE(TAG, "Processing_queue reset failed");
                    }
                        break;
                    }
            }
        }
        free(data);
        uart_flush_input(ECHO_UART_PORT_NUM);
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

    ESP_LOGI(TAG, "Processing task ready");
    
    while (1) {
        uint8_t *qr_buf = quirc_begin(qr, NULL, NULL);

        // Get the next frame from the queue
        int res = xQueueReceive(input_queue, &pic, portMAX_DELAY);
        assert(res == pdPASS);

        int64_t t_start = esp_timer_get_time();
        // Convert the frame to grayscale. We could have asked the camera for a grayscale frame,
        rgb565_to_grayscale_buf(pic->buf, qr_buf, pic->width, pic->height);

        // Return the frame buffer to the camera driver ASAP to avoid DMA errors
        esp_camera_fb_return(pic);


        // Process the frame. This step find the corners of the QR code (capstones)
        quirc_end(qr);
        int64_t t_end_find = esp_timer_get_time();
        int count = quirc_count(qr);
        quirc_decode_error_t err = QUIRC_ERROR_DATA_UNDERFLOW;
        int time_find_ms = (int)(t_end_find - t_start) / 1000;
        ESP_LOGI(TAG, "QR count: %d   Heap: %d  Stack free: %d  time: %d ms",
                 count, heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                 uxTaskGetStackHighWaterMark(NULL), time_find_ms);
            
        // If a QR code was detected, try to decode it:
        for (int i = 0; i < count; i++) {
            struct quirc_code code = {};
            struct quirc_data qr_data = {};
            // Extract raw QR code binary data (values of black/white modules)
            quirc_extract(qr, i, &code);


            // Decode the raw data. This step also performs error correction.
            err = quirc_decode(&code, &qr_data);
            int64_t t_end = esp_timer_get_time();
            int time_decode_ms = (int)(t_end - t_end_find) / 1000;
            ESP_LOGI(TAG, "Decoded in %d ms", time_decode_ms);
            if (err != 0) {
                ESP_LOGE(TAG, "QR err: %s", quirc_strerror(err));
            } else {
                ESP_LOGI(TAG, "QR Data: %s bytes: '%d'", qr_data.payload, qr_data.payload_len);
                char uart_buffer[BUF_SIZE];
                memcpy(uart_buffer, qr_data.payload, qr_data.payload_len);
                int some = uart_write_bytes(ECHO_UART_PORT_NUM, uart_buffer, qr_data.payload_len);
                suspend_processing_task = true;
                // memset(qr_data.payload, 0, qr_data.payload_len);
                // vTaskDelay(pdMS_TO_TICKS(500));
                // Clear the camera frame queue

                for (int i = 0; i < uxQueueMessagesWaiting(input_queue); i++)
                {
                    xQueueReceive(input_queue, &pic, portMAX_DELAY);
                    esp_camera_fb_return(pic);
                }
                ESP_LOGI(TAG, "Input_queue reset successful");
    
                memset(uart_buffer, 0, sizeof(uart_buffer));
                memset(qr_buf, 0, qr_width * qr_height);
                pic = NULL;
            }
        }
        // vTaskDelay(pdMS_TO_TICKS(100)); // Adjust the delay as needed

    }
}

// Helper functions to convert an RGB565 image to grayscale
typedef union {
    uint16_t val;
    struct {
        uint16_t b: 5;
        uint16_t g: 6;
        uint16_t r: 5;
    };
} rgb565_t;

static uint8_t rgb565_to_grayscale(const uint8_t *img)
{
    uint16_t *img_16 = (uint16_t *) img;
    rgb565_t rgb = {.val = __builtin_bswap16(*img_16)};
    uint16_t val = (rgb.r * 8 + rgb.g * 4 + rgb.b * 8) / 3;
    return (uint8_t) MIN(255, val);
}

static void rgb565_to_grayscale_buf(const uint8_t *src, uint8_t *dst, int qr_width, int qr_height)
{
    for (size_t y = 0; y < qr_height; y++) {
        for (size_t x = 0; x < qr_width; x++) {
            dst[y * qr_width + x] = rgb565_to_grayscale(&src[(y * qr_width + x) * 2]);
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
