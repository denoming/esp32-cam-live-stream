# ESP32-CAM Live Stream

## Dependencies

* Platform.io toolkit
* ESP-IDF (ver. 5.1)
* ESP32 Camera Driver (git submodule)

## Build

* Clone repository

```shell
$ git clone <url>
$ cd esp32-cam-live-stream
$ git submodule update --init --recursive
```
* Generate configuration

```shell
$ cd <project-folder>
$ pio run -t menuconfig

# Camera live stream options
#  -> WiFi options
#  -> Video camera options
```

* Build

```shell
$ pio run -e esp32cam -v
```

* Upload

```shell
$ pio run -e esp32cam -t upload
```

## Camera options

* XCLK frequency (Hz) = 10MHz - 20MHz 
* LEDC timer = 0 (by default)
* LEDC channel = 0 (by default)
* Pixel format = JPEG (JPEG is preferable value)
* Frame size: QVGA - SVGA (depends on used camera module)
* JPEG quality: 0 - 63 (lower is better quality)
* Number of frames: 2 (if > 1, camera will work in continuous mode)
* Frame location: PSRAM (by default)
* Grab mode: latest (if number of frames >1 then the latest mode should be used)
* Pins config:

| Pin   | Wrover Kit | ESP-EYE | AI-THINKER |
|-------|------------|---------|------------|
| PWDN  | -1         | -1      | 32         |
| RESET | -1         | -1      | -1         |
| XCLK  | 21         | 4       | 0          |
| SIOD  | 26         | 18      | 26         |
| SIOC  | 27         | 23      | 27         |
| D7    | 35         | 36      | 35         |
| D6    | 34         | 37      | 34         |
| D5    | 39         | 38      | 39         |
| D4    | 36         | 39      | 36         |
| D3    | 19         | 35      | 21         |
| D2    | 18         | 14      | 19         |
| D1    | 5          | 13      | 18         |
| D0    | 4          | 34      | 5          |
| VSYNC | 25         | 5       | 25         |
| HREF  | 23         | 27      | 23         |
| PCLK  | 22         | 25      | 22         |

### Observations

Options affecting performance:
* XCLK frequency (the higher value the more FPS?)
* Pixel format (capturing RAW formats loads handling heavily)
* Frame size (the more frame size the more data to handle at a time)
* JPEG quality (the more quality the highest CPU affecting)
