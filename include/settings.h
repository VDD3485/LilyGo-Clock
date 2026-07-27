#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "time.h"
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

#define BUTTON_BOOT_PIN   39  

// Динамические настройки из Preferences Flash
extern String dynamic_ssid;
extern String dynamic_pass;
extern int dynamic_gmt_offset; 

// НАСТРОЙКИ ПОДКЛЮЧЕНИЯ И СЕТИ (По умолчанию)
#define DEFAULT_SSID          "YourWiFiSSID"
#define DEFAULT_PASSWORD      "YourWiFiPassword"
#define DEFAULT_GMT_OFFSET    3
#define NTP_SERVER            "pool.ntp.org"
#define DAYLIGHT_OFFSET_SEC   0     
#define AP_SSID               "Clock_Config"

// Буфер для автоматического имени точки доступа "Clock_xxxx" (ИСПРАВЛЕНО: тип данных изменен на массив)
extern char generated_ap_ssid[32]; 

// НАСТРОЙКИ ШРИФТОВ U8g2
#define FONT_CLOCK            u8g2_font_logisoso62_tn 
#define FONT_DATE             u8g2_font_unifont_t_cyrillic 
#define FONT_BATTERY          u8g2_font_unifont_t_cyrillic 
#define FONT_WIFI             u8g2_font_unifont_t_cyrillic 

// ТЕКСТОВЫЕ СТРОКИ
#define TEXT_WIFI_STATUS      "WiFi"
extern const char* days[];
extern const char* months[];

// АППАРАТНАЯ РАСПИНОВКА ПЛАТЫ LILYGO T5
#define BOARD_LED     19
#define BAT_ADC_PIN   35  
#define EPD_CS        5
#define EPD_DC        17
#define EPD_RST       16
#define EPD_BUSY      4

// Глобальные переменные (внешние ссылки)
extern unsigned long lastSyncTime;
extern const unsigned long syncInterval;
extern bool isWifiConnected;               
extern bool needFullRefresh;                
extern bool isConfigMode; 

extern GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display;
extern U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
extern WebServer server; 

// Объявления функций
int getBatteryPercentage();
void updateClockDisplay();
void syncTimeNTP();
void initWebServer(); 
void handleWebServer(); 

#endif
