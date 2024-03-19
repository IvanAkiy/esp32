#ifndef MAIN_H
#define MAIN_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <esp_event.h>
#include <esp_log.h>
#include <driver/i2c.h>

class Main final
{
public:
    Main();
    esp_err_t setup();
    void loop();
};

#endif // MAIN_H