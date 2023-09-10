#pragma once

#include "Types.hpp"

#include <esp_types.h>

#include <span>

class VideoCamera {
public:
    VideoCamera();

    ~VideoCamera();

    [[nodiscard]] bool
    ready();

    [[nodiscard]] bool
    captured();

    bool
    setup();

    void
    tearDown();

    [[nodiscard]] bool
    capture();

    [[nodiscard]] std::span<uint8_t>
    frame();

    [[nodiscard]] size_t
    frameWidth() const;

    [[nodiscard]] size_t
    frameHeight() const;

    [[nodiscard]] PictureFormat
    frameFormat() const;

private:
    class Impl;
    Impl* _impl;
};