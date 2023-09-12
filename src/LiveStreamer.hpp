#pragma once

#include "Types.hpp"

#include <esp_types.h>

class LiveStreamer {
public:
    LiveStreamer();

    ~LiveStreamer();

    bool
    setup();

    bool
    start(uint16_t port = 80);

private:
    class Impl;
    Impl* _impl;
};