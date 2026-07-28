#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

// ПОДКЛЮЧЕНИЕ НАВИНЫХ ШРИФТОВ ADAFRUIT GFX
#include <Fonts/FreeSansBold42pt7b.h> // ИСПРАВЛЕНО: Новый гигантский сглаженный шрифт для часов
#include <Fonts/ofont_ru_Vremena_Medium_Italic10pt7b.h>       // Стандартный шрифт для даты и статус-бара

#define FONT_CLOCK   &FreeSansBold42pt7b
#define FONT_TEXT    &ofont_ru_Vremena_Medium_Italic10pt7b

// Распиновка LilyGo T5 2.13" Board
#define EPD_CS          5
#define EPD_DC          17
#define EPD_RST         16
#define EPD_BUSY        4
#define BAT_ADC_PIN     35
#define BOARD_LED       19
#define BUTTON_BOOT_PIN 39

// Настройки по умолчанию
#define DEFAULT_SSID        "YOUR_WIFI_SSID"
#define DEFAULT_PASSWORD    "YOUR_WIFI_PASSWORD"
#define DEFAULT_GMT_OFFSET  3 // +3 (Москва)
#define DAYLIGHT_OFFSET_SEC 0
#define NTP_SERVER          "pool.ntp.org"
#define TEXT_WIFI_STATUS    "WiFi"

// Глобальные переменные (флаги и объекты управления)
extern bool isWifiConnected;
extern bool needFullRefresh;
extern bool isConfigMode;
extern String dynamic_ssid;
extern String dynamic_pass;
extern int dynamic_gmt_offset;
extern char generated_ap_ssid[32];

extern GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display;
extern WebServer server;

// Прототипы функций внешних модулей
void updateClockDisplay();
void syncTimeNTP();
void initWebServer();
void handleWebServer();

#endif
