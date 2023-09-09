#pragma once

class WiFi {
public:
    WiFi();

    ~WiFi();

    bool
    setup();

    bool
    connect();

private:
    class WiFiImpl* _impl;
};