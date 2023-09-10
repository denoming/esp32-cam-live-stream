#include "VideoCamera.hpp"

#include "Config.hpp"

#include <esp_assert.h>
#include <esp_camera.h>
#include <esp_log.h>

static const char* TAG = "CLS";

class VideoCameraImpl {
public:
    bool
    setup()
    {
        camera_config_t cameraConfig = {
            .pin_pwdn = CLS_CAM_PIN_PWDN,
            .pin_reset = CLS_CAM_PIN_RESET,
            .pin_xclk = CLS_CAM_PIN_XCLK,
            .pin_sccb_sda = CLS_CAM_PIN_SIOD,
            .pin_sccb_scl = CLS_CAM_PIN_SIOC,
            .pin_d7 = CLS_CAM_PIN_D7,
            .pin_d6 = CLS_CAM_PIN_D6,
            .pin_d5 = CLS_CAM_PIN_D5,
            .pin_d4 = CLS_CAM_PIN_D4,
            .pin_d3 = CLS_CAM_PIN_D3,
            .pin_d2 = CLS_CAM_PIN_D2,
            .pin_d1 = CLS_CAM_PIN_D1,
            .pin_d0 = CLS_CAM_PIN_D0,
            .pin_vsync = CLS_CAM_PIN_VSYNC,
            .pin_href = CLS_CAM_PIN_HREF,
            .pin_pclk = CLS_CAM_PIN_PCLK,
            .xclk_freq_hz = CLS_CAM_XCLK_FREQ_HZ,
            .ledc_timer = CLS_CAM_LEDC_TIMER,
            .ledc_channel = CLS_CAM_LEDC_CHANNEL,
            .pixel_format = CLS_CAM_PIXEL_FORMAT,
            .frame_size = CLS_CAM_FRAME_SIZE,
            .jpeg_quality = CLS_CAM_JPEG_QUALITY,
            .fb_count = CLS_CAM_FB_COUNT,
            .fb_location = CLS_CAM_FB_LOCATION,
            .grab_mode = CLS_CAM_GRAB_MODE,
        };

        if (esp_camera_init(&cameraConfig) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to initialize camera");
            return false;
        }

        return true;
    }
};

VideoCamera::VideoCamera()
{
    static VideoCameraImpl impl;
    _impl = &impl;
}

VideoCamera::~VideoCamera()
{
    _impl = nullptr;
}

bool
VideoCamera::setup()
{
    assert(_impl != nullptr);
    return _impl->setup();
}
