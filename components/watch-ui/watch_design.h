#pragma once
#include "lvgl.h"

// One fixed AMOLED design, not a theme engine. Layout dimensions target 410 x 502.
namespace watch_design {
constexpr uint32_t Black = 0x000000, Surface = 0x080808, Border = 0x383838;
constexpr uint32_t Primary = 0xF2F2F2, Secondary = 0xA0A0A0, Pressed = 0x242424;
constexpr uint32_t Connected = Primary, Disabled = Secondary, Warning = 0xFF6655;
constexpr uint32_t Bluetooth = 0x67A9FF, Activity = 0xFFAA55;
constexpr uint32_t Wifi = 0x74DCC8, Settings = Secondary;
constexpr uint32_t Brightness = 0xFFE59A, Timeout = 0xB8A1FF;
constexpr uint32_t Timer = 0xFF887A, Stopwatch = 0x68D9EF, Weather = 0xFFD166;
constexpr uint32_t WeatherRain = 0x8ACFFF, WeatherCloudy = 0xC4CBD3;
constexpr uint32_t WeatherStorm = 0xB8A1FF, WeatherWind = 0xA5DDD3;
constexpr uint32_t BatteryHigh = 0x32CD32, BatteryMedium = 0xFFD166;
constexpr uint32_t BatteryLow = 0xFFAA55, BatteryCritical = 0xFF6655;
constexpr int Unit = 8, Margin = 32, ContentWidth = 346;
constexpr int Radius = 12, BorderWidth = 1, TouchHeight = 56;
constexpr int HeaderY = 32, BackInset = 24, TransitionMs = 160;
constexpr int IconSize = 32, IconStroke = 2;
constexpr int TileWidth = 160, TileHeight = 132, TileOffset = 88;
constexpr int TileFirstY = 112, TileSecondY = 260;
constexpr int ClockY = 112, ClockWidth = 310, ClockHeight = 84;
constexpr int HomeGap = 32, DateHeight = 24;
constexpr int DateY = ClockY + ClockHeight + HomeGap;
constexpr int WeatherY = DateY + DateHeight + HomeGap, WeatherHeight = 112;
constexpr int ActivityCountY = 98, HistoryY = 200, HistoryRow = 28;
inline const lv_font_t *body() { return &lv_font_montserrat_18; }
inline const lv_font_t *caption() { return &lv_font_montserrat_14; }
inline const lv_font_t *title() { return &lv_font_montserrat_26; }
inline const lv_font_t *number() { return &lv_font_montserrat_48; }
}
