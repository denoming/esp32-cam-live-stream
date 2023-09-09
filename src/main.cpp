#include <esp_log.h>

#include "WiFi.hpp"

static const char* TAG = "CLS";

static WiFi wifi;

extern "C" void
app_main(void)
{
    if (not wifi.setup()) {
        ESP_LOGE(TAG, "Unable to setup WiFi");
        return;
    }

    if (not wifi.connect()) {
        ESP_LOGE(TAG, "Unable to connect to WiFi AP: %s", CONFIG_CLS_ESP_WIFI_SSID);
        return;
    }

    ESP_LOGI(TAG, "Connected to WiFi AP: %s", CONFIG_CLS_ESP_WIFI_SSID);
}