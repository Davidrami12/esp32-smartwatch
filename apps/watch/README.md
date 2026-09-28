# Smartwatch Firmware

Firmware for the Waveshare ESP32-S3 Touch AMOLED 2.06.

## Stack

- ESP-IDF
- LVGL
- C

## Current status

- Display working
- LVGL running
- Custom background
- Basic text rendering

## Build

```bash
idf.py build
```

## Flash

```bash
idf.py -p COM9 flash
```

## Flash and monitor

```bash
idf.py -p COM9 flash monitor
```