# ESP32-CAM Live Stream

## Dependencies

* esp32-cam driver

## Build

* Clone repository
```shell
$ git clone <url>
$ git submodule update --init --recursive
```
* Generate configuration
```shell
* cd <project-folder>
* pio run -t menuconfig
# Camera driver configuration: "Component config" -> "Camera configuration"
```
* Build
