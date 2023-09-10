#include <esp_log.h>

#include "WiFi.hpp"
#include "VideoCamera.hpp"

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

    if (videoCamera.setup()) {
        ESP_LOGI(TAG, "Video camera has been initialized");
    } else {
        ESP_LOGE(TAG, "Unable to initialize video camera");
    }
}