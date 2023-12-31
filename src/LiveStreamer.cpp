#include "LiveStreamer.hpp"

#include "Config.hpp"

#include <esp_assert.h>
#include <esp_camera.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <img_converters.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <span>
#include <array>

static const char* TAG = "CLS";

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART
    = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

class LiveStreamer::Impl {
public:
    ~Impl()
    {
        stopCaptureFrames();
        deleteFramesQueue();
    }

    bool
    setup()
    {
        if (not createFrameQueue()) {
            ESP_LOGE(TAG, "Unable setup capture queue");
            return false;
        }

        if (not setupVideoCamera()) {
            ESP_LOGE(TAG, "Unable setup video camera");
            return false;
        }

        return true;
    }

    bool
    start(uint16_t port)
    {
        if (not startStreamServer(port)) {
            ESP_LOGE(TAG, "Unable to start stream server handler");
            return false;
        }

        if (not startCaptureFrames()) {
            ESP_LOGE(TAG, "Unable to start capture frame handler");
            return false;
        }

        return true;
    }

private:
    bool
    createFrameQueue()
    {
        assert(_queueHandle == QueueHandle_t{});
        ESP_LOGD(TAG, "Create capture queue with <%d> size", CLS_CAM_FB_COUNT);
        _queueHandle = xQueueCreate(CLS_CAM_FB_COUNT, sizeof(camera_fb_t*));
        if (_queueHandle == nullptr) {
            ESP_LOGE(TAG, "Unable create capture queue");
            return false;
        }
        return true;
    }

    void
    deleteFramesQueue()
    {
        if (_queueHandle) {
            vQueueDelete(_queueHandle);
            _queueHandle = {};
        }
    }

    camera_fb_t*
    popFrame()
    {
        camera_fb_t* frame{};
        xQueueReceive(_queueHandle, &frame, portMAX_DELAY);
        return frame;
    }

    void
    pushFrame(camera_fb_t* frame)
    {
        xQueueSend(_queueHandle, &frame, portMAX_DELAY);
    }

    bool
    startCaptureFrames()
    {
        static const uint32_t kStackSize{4096};

        assert(_captureHandle == TaskHandle_t{});
        return (xTaskCreatePinnedToCore(&staticCaptureFrameHandler,
                                        "CAPTURE",
                                        kStackSize,
                                        this,

                                        tskIDLE_PRIORITY + 1,
                                        &_captureHandle,
                                        0)
                == pdPASS);
    }

    void
    stopCaptureFrames()
    {
        if (_captureHandle) {
            vTaskDelete(_captureHandle);
            _captureHandle = {};
        }
    }

    bool
    startStreamServer(uint16_t port)
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = port;
        config.stack_size = 5120;
        httpd_uri_t streamUri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = staticStreamServerHandler,
            .user_ctx = this,
        };

        ESP_LOGD(TAG, "Starting HTTP server on <%d> port", port);
        esp_err_t error = httpd_start(&_streamHandle, &config);
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Unable to start HTTPd server: <%s>", esp_err_to_name(error));
            return false;
        }

        error = httpd_register_uri_handler(_streamHandle, &streamUri);
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Unable to register HTTPd handler: <%s>", esp_err_to_name(error));
            return false;
        }

        return true;
    }

    esp_err_t
    handleStreamRequest(httpd_req_t* req)
    {
        esp_err_t error = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Unable to set HTTP response type: <%s>", esp_err_to_name(error));
            return error;
        }

        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_set_hdr(req, "X-Framerate", "60");

        std::array<char, 128> buffer = {};
        while (true) {
            if (camera_fb_t* frame = popFrame(); frame != nullptr) {
                error = sendStreamResponse(req, frame, buffer);
                esp_camera_fb_return(frame);
                if (error != ESP_OK) {
                    break;
                }
            }
        }

        return error;
    }

    static esp_err_t
    sendStreamResponse(httpd_req_t* req, camera_fb_t* frame, std::span<char> buffer)
    {
        // Prepare JPEG picture
        uint8_t* jpeg{};
        size_t jpegSize{};
        if (frame->format == PIXFORMAT_JPEG) {
            jpeg = frame->buf;
            jpegSize = frame->len;
        } else {
            if (not frame2jpg(frame, CLS_CAM_JPEG_QUALITY, &jpeg, &jpegSize)) {
                ESP_LOGE(TAG, "Unable to convert frame into JPEG");
                return ESP_FAIL;
            }
        }

        // Send HTTP response
        static const auto kBoundaryLength = ssize_t(strlen(STREAM_BOUNDARY));
        esp_err_t error = httpd_resp_send_chunk(req, STREAM_BOUNDARY, kBoundaryLength);
        if (error == ESP_OK) {
            ssize_t hlen = snprintf(buffer.data(),
                                    buffer.size(),
                                    STREAM_PART,
                                    jpegSize,
                                    frame->timestamp.tv_sec,
                                    frame->timestamp.tv_usec);
            error = httpd_resp_send_chunk(req, buffer.data(), hlen);
        }
        if (error == ESP_OK) {
            error = httpd_resp_send_chunk(req, (const char*) jpeg, ssize_t(jpegSize));
        }

        // Free memory
        if (frame->format != PIXFORMAT_JPEG) {
            free(jpeg);
            jpeg = nullptr;
            jpegSize = 0;
        }

        return error;
    }

    [[noreturn]] void
    captureFrameHandler()
    {
        camera_fb_t* frame;
        while (true) {
            if (frame = esp_camera_fb_get(); frame != nullptr) {
                pushFrame(frame);
            }
        }
    }

    static bool
    setupVideoCamera()
    {
        camera_config_t config = {
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

        if (config.pixel_format != PIXFORMAT_JPEG) {
            ESP_LOGW(TAG, "Using CLS_CAM_PIXEL_FORMAT_JPEG pixel format is recommended");
        }
        if (config.fb_count > 1 and config.grab_mode != CAMERA_GRAB_LATEST) {
            ESP_LOGW(TAG, "Using CLS_CAM_GRAB_MODE_LATEST grab mode is recommended");
        }

        if (esp_camera_init(&config) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to initialize video camera");
            return false;
        }

        sensor_t* sensor = esp_camera_sensor_get();
        assert(sensor != nullptr);
        sensor->set_vflip(sensor, 1);
        switch (sensor->id.PID) {
        case OV3660_PID:
            sensor->set_saturation(sensor, -2);
            sensor->set_vflip(sensor, 1);
            break;
        case OV2640_PID:
            sensor->set_vflip(sensor, 1);
            break;
        case GC0308_PID:
            sensor->set_hmirror(sensor, 0);
            break;
        case GC032A_PID:
            sensor->set_vflip(sensor, 1);
            break;
        }

        return true;
    }

private:
    static esp_err_t
    staticStreamServerHandler(httpd_req_t* req)
    {
        Impl* self = static_cast<Impl*>(req->user_ctx);
        assert(self != nullptr);
        return self->handleStreamRequest(req);
    }

    static void
    staticCaptureFrameHandler(void* data)
    {
        Impl* self = static_cast<Impl*>(data);
        assert(self != nullptr);
        self->captureFrameHandler();
    }

private:
    QueueHandle_t _queueHandle{};
    httpd_handle_t _streamHandle{};
    TaskHandle_t _captureHandle{};
};

LiveStreamer::LiveStreamer()
{
    static Impl impl;
    _impl = &impl;
}

LiveStreamer::~LiveStreamer()
{
    _impl = nullptr;
}

bool
LiveStreamer::setup()
{
    assert(_impl);
    return _impl->setup();
}

bool
LiveStreamer::start(uint16_t port)
{
    assert(_impl);
    return _impl->start(port);
}