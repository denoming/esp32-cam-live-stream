#include <esp_log.h>

#include "WiFi.hpp"
#include "LiveStreamer.hpp"

static const char* TAG = "CLS";

static WiFi wifi;
static LiveStreamer streamer;

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

    if (not streamer.setup()) {
        ESP_LOGE(TAG, "Unable to initialise live streamer");
        return;
    }

    if (not streamer.start()) {
        ESP_LOGE(TAG, "Unable to start live streamer");
    }
}