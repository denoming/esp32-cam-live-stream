#include <esp_log.h>

#include "WiFi.hpp"
#include "VideoCamera.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "CLS";

static WiFi wifi;
static VideoCamera videoCamera;

extern "C" void
app_main(void)
{
    if (not wifi.setup()) {
        ESP_LOGE(TAG, "Unable to setup WiFi");
        return;
    }

    if (wifi.connect()) {
        ESP_LOGI(TAG, "Connected to WiFi AP: %s", CONFIG_CLS_WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Unable to connect to WiFi AP: %s", CONFIG_CLS_WIFI_SSID);
    }

    if (not videoCamera.setup()) {
        ESP_LOGE(TAG, "Unable to initialize video camera");
        return;
    }

    if (videoCamera.ready()) {
        while (true) {
            if (videoCamera.capture()) {
                auto frame = videoCamera.frame();
                ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", frame.size());
            } else {
                break;
            };
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}