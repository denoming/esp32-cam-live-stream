#pragma once

class VideoCamera {
public:
    VideoCamera();

    ~VideoCamera();

    bool
    setup();

private:
    class VideoCameraImpl* _impl;
};