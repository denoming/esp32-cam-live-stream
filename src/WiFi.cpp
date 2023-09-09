#include "WiFi.hpp"

#include <nvs_flash.h>

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_assert.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <functional>

static const char* TAG = "CLS";

#if CONFIG_CLS_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define CLS_ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define CLS_H2E_IDENTIFIER ""
#elif CONFIG_CLS_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define CLS_ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define CLS_H2E_IDENTIFIER CONFIG_CLS_ESP_WIFI_PW_ID
#elif CONFIG_CLS_ESP_WPA3_SAE_PWE_BOTH
#define CLS_ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define CLS_H2E_IDENTIFIER CONFIG_CLS_ESP_WIFI_PW_ID
#endif

#if CONFIG_CLS_ESP_WIFI_AUTH_OPEN
#define CLS_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_CLS_ESP_WIFI_AUTH_WEP
#define CLS_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_CLS_ESP_WIFI_AUTH_WPA_PSK
#define CLS_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_CLS_ESP_WIFI_AUTH_WPA2_PSK
#define CLS_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_CLS_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define CLS_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_CLS_ESP_WIFI_AUTH_WPA3_PSK
#define CLS_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_CLS_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define CLS_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_CLS_ESP_WIFI_AUTH_WAPI_PSK
#define CLS_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/**
 * The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we was unable to connect after the maximum amount of retries
 */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

class WiFiModule {
public:
    WiFiModule()
        : _eventGroup{xEventGroupCreate()}
    {
    }

    bool
    setup()
    {
        esp_err_t rv = nvs_flash_init();
        if (rv == ESP_ERR_NVS_NO_FREE_PAGES || rv == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            rv = nvs_flash_init();
        }
        if (rv != ESP_OK) {
            ESP_LOGE(TAG, "Unable to initialize NVS flash");
            return false;
        }

        if (esp_netif_init() != ESP_OK) {
            ESP_LOGE(TAG, "Unable to initialize network interface");
            return false;
        }

        if (esp_event_loop_create_default() != ESP_OK) {
            ESP_LOGE(TAG, "Unable to create event loop");
            return false;
        }

        auto* netIf = esp_netif_create_default_wifi_sta();
        assert(netIf != nullptr);

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        if (esp_wifi_init(&cfg) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to initialize WiFi");
            esp_netif_destroy_default_wifi(netIf);
            return false;
        }

        esp_event_handler_instance_t instanceAnyId;
        if (esp_event_handler_instance_register(
                WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, this, &instanceAnyId)
            != ESP_OK) {
            ESP_LOGE(TAG, "Unable to register event handler (WiFi)");
            return false;
        }
        esp_event_handler_instance_t instanceGotIp;
        if (esp_event_handler_instance_register(
                IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHandler, this, &instanceGotIp)
            != ESP_OK) {
            ESP_LOGE(TAG, "Unable to register event handler (IP)");
            return false;
        }

        wifi_config_t wifiConfig = {
                    .sta = {
                        .ssid = CONFIG_CLS_ESP_WIFI_SSID,
                        .password = CONFIG_CLS_ESP_WIFI_PASSWORD,
                        .threshold = {
                            .authmode = CLS_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD
                        },
                        .sae_pwe_h2e = CLS_ESP_WIFI_SAE_MODE,
                        .sae_h2e_identifier = CLS_H2E_IDENTIFIER,
                    },
                };

        if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to set WiFi mode");
            return false;
        }

        if (esp_wifi_set_config(WIFI_IF_STA, &wifiConfig) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to set WiFi config");
            return false;
        }

        return true;
    }

    bool
    connect()
    {
        if (esp_wifi_start() != ESP_OK) {
            ESP_LOGE(TAG, "Unable to start WiFi in STA mode");
            return false;
        }

        ESP_LOGI(TAG, "Connect to AP SSID: %s", CONFIG_CLS_ESP_WIFI_SSID);

        /**
         * Waiting until
         * - the connection is established (WIFI_CONNECTED_BIT)
         * - connection failed for the maximum number of re-tries (WIFI_FAIL_BIT)
         */
        EventBits_t bits = xEventGroupWaitBits(
            _eventGroup, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to AP SSID: %s", CONFIG_CLS_ESP_WIFI_SSID);
            return true;
        }
        if (bits & WIFI_FAIL_BIT) {
            ESP_LOGI(TAG, "Unable to connect to SSID: %s", CONFIG_CLS_ESP_WIFI_SSID);
            return true;
        }

        ESP_LOGE(TAG, "Unexpected event");
        return false;
    }

private:
    void
    onWiFiEvent(int32_t id, void* /*eventData*/)
    {
        switch (id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if (_retryCnt < CONFIG_CLS_ESP_MAXIMUM_RETRY) {
                esp_wifi_connect();
                _retryCnt++;
                ESP_LOGI(TAG, "Retry to connect to the AP");
            } else {
                xEventGroupSetBits(_eventGroup, WIFI_FAIL_BIT);
            }
            break;
        default:
            break;
        }
    }

    void
    onIpEvent(int32_t id, void* eventData)
    {
        if (id == IP_EVENT_STA_GOT_IP) {
            auto* event = static_cast<ip_event_got_ip_t*>(eventData);
            ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            _retryCnt = 0;
            xEventGroupSetBits(_eventGroup, WIFI_CONNECTED_BIT);
        }
    }

    static void
    eventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData)
    {
        auto* const self = static_cast<WiFiModule*>(arg);
        assert(self != nullptr);

        if (eventBase == WIFI_EVENT) {
            self->onWiFiEvent(eventId, eventData);
        } else if (eventBase == IP_EVENT) {
            self->onIpEvent(eventId, eventData);
        }

        /* Not interesting events */
    }

private:
    EventGroupHandle_t _eventGroup{};
    std::size_t _retryCnt{};
};

WiFi::WiFi()
{
    static WiFiModule module;
    _module = &module;
}

WiFi::~WiFi()
{
    _module = nullptr;
}

bool
WiFi::setup()
{
    assert(_module);
    return _module->setup();
}

bool
WiFi::connect()
{
    assert(_module);
    return _module->connect();
}