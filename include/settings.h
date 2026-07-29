#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <GxEPD2_BW.h>

// ШРИФТЫ ADAFRUIT GFX
#include <Fonts/FreeSansBold42pt7b.h> 
#include <Fonts/FreeSansBold10pt7b.h> 

#define FONT_CLOCK   &FreeSansBold42pt7b
#define FONT_TEXT    &FreeSansBold10pt7b 

// Распиновка LilyGo T5 2.13" Board
#define EPD_CS          5
#define EPD_DC          17
#define EPD_RST         16
#define EPD_BUSY        4
#define BAT_ADC_PIN     35
#define BOARD_LED       19
#define BUTTON_BOOT_PIN 39

// АППАРАТНАЯ КОНФИГУРАЦИЯ ДАТЧИКА ГРОЗЫ AS3935
#define AS3935_IRQ_PIN  34            

// Настройки по умолчанию
#define DEFAULT_SSID        "YOUR_WIFI_SSID"
#define DEFAULT_PASSWORD    "YOUR_WIFI_PASSWORD"
#define DEFAULT_GMT_OFFSET  3 
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

// ГЛОБАЛЬНЫЕ RTC-ПЕРЕМЕННЫЕ ДЛЯ ДАТЧИКА ГРОЗЫ
extern int rtcStormDistance;          
extern bool rtcIsStormAlert;         

extern GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display;
extern WebServer server;

// Прототипы функций внешних модулей
void updateClockDisplay();
void syncTimeNTP();
void initWebServer();
void handleWebServer();

#endif
