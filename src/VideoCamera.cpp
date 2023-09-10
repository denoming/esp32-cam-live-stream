#include "VideoCamera.hpp"

#include "Config.hpp"

#include <esp_assert.h>
#include <esp_camera.h>
#include <esp_log.h>

#include <span>

static const char* TAG = "CLS";

class VideoCamera::Impl {
public:
    [[nodiscard]] bool
    ready()
    {
        return _ready;
    }

    [[nodiscard]] bool
    captured()
    {
        return (_frameBuffer != nullptr);
    }

    bool
    setup()
    {
        if (_ready) {
            ESP_LOGW(TAG, "Camera is already setup");
            return true;
        }

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
            _ready = false;
            return false;
        } else {
            _ready = true;
            return true;
        }
    }

    void
    tearDown()
    {
        if (_ready) {
            if (esp_camera_deinit() != ESP_OK) {
                ESP_LOGE(TAG, "Unable to de-initialise video camera");
            }
        }
    }

    bool
    capture()
    {
        if (_frameBuffer != nullptr) {
            esp_camera_fb_return(_frameBuffer);
        }
        _frameBuffer = esp_camera_fb_get();
        return captured();
    }

    const camera_fb_t&
    buffer()
    {
        assert(_frameBuffer != nullptr);
        return *_frameBuffer;
    }

private:
    bool _ready{};
    camera_fb_t* _frameBuffer{};
};

VideoCamera::VideoCamera()
{
    static Impl impl;
    _impl = &impl;
}

VideoCamera::~VideoCamera()
{
    _impl = nullptr;
}

bool
VideoCamera::ready()
{
    assert(_impl != nullptr);
    return _impl->ready();
}

bool
VideoCamera::captured()
{
    assert(_impl != nullptr);
    return _impl->captured();
}

bool
VideoCamera::setup()
{
    assert(_impl != nullptr);
    return _impl->setup();
}

void
VideoCamera::tearDown()
{
    assert(_impl != nullptr);
    return _impl->tearDown();
}

bool
VideoCamera::capture()
{
    assert(_impl != nullptr);
    return _impl->capture();
}

std::span<uint8_t>
VideoCamera::frame()
{
    assert(_impl != nullptr && _impl->captured());
    const auto& buffer = _impl->buffer();
    return {buffer.buf, buffer.len};
}

size_t
VideoCamera::frameWidth() const
{
    assert(_impl != nullptr && _impl->captured());
    return _impl->buffer().width;
}

size_t
VideoCamera::frameHeight() const
{
    assert(_impl != nullptr && _impl->captured());
    return _impl->buffer().height;
}

PictureFormat
VideoCamera::frameFormat() const
{
    assert(_impl != nullptr && _impl->captured());
    return static_cast<PictureFormat>(_impl->buffer().format);
}
