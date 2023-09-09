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
    class WiFiModule* _module;
};