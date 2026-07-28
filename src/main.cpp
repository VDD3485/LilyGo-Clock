#include "settings.h"
#include <sys/time.h>

// ПЕРЕМЕННЫЕ В RTC ПАМЯТИ
RTC_DATA_ATTR time_t lastSyncTimestamp = 0; 
RTC_DATA_ATTR int rtcLastMinute = -1;
RTC_DATA_ATTR double rtcCalculatedOffsetSec = 0.0; // Сохраненная суточная поправка кварца
RTC_DATA_ATTR bool rtcIsCalibrated = false;       // Флаг калибровки

const unsigned long syncIntervalSec = 86400;    // Суточный интервал — 24 часа

bool isWifiConnected = false;               
bool needFullRefresh = false;                
bool isConfigMode = false; 

String dynamic_ssid;
String dynamic_pass;
int dynamic_gmt_offset;
char generated_ap_ssid[32]; // Массив из 32 байт

GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
WebServer server(80); 

void generateUniqueSSID() {
  uint8_t mac[6]; // Массив из 6 байт под MAC-адрес
  WiFi.macAddress(mac); 
  uint16_t mac_id = (mac[4] << 8) | mac[5];
  uint16_t unique_id = mac_id ^ 3485; 
  snprintf(generated_ap_ssid, sizeof(generated_ap_ssid), "Clock_%04X", unique_id);
}

void configureSystemTime() {
  long base_offset_sec = (long)dynamic_gmt_offset * 3600L;
  long final_offset_sec = base_offset_sec + (long)rtcCalculatedOffsetSec;
  
  configTime(final_offset_sec, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  char tzString[32];
  if (dynamic_gmt_offset >= 0) {
    snprintf(tzString, sizeof(tzString), "GMT-%d", dynamic_gmt_offset);
  } else {
    snprintf(tzString, sizeof(tzString), "GMT+%d", abs(dynamic_gmt_offset));
  }
  setenv("TZ", tzString, 1);
  tzset();
}

void enterDeepSleep(int secondsToSleep) {
  if (isConfigMode) return;

  Serial.print("Уходим в глубокий сон на "); Serial.print(secondsToSleep); Serial.println(" сек.");
  display.powerOff(); 
  
  pinMode(BOARD_LED, OUTPUT);
  digitalWrite(BOARD_LED, HIGH); 
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  esp_sleep_enable_timer_wakeup((uint64_t)secondsToSleep * 1000000ULL);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_BOOT_PIN, 0); 
  esp_deep_sleep_start();
}

void performStartupCalibration() {
  Serial.println("\n>>> ЗАПУСК ПРОЦЕДУРЫ НАЧАЛЬНОЙ КАЛИБРОВКИ КВАРЦА ПО MILLIS (60 сек) <<<");
  
  syncTimeNTP();
  configureSystemTime(); 
  time_t startNtp = time(NULL);
  unsigned long startMillis = millis();

  // Выводим на e-ink заставку калибровки
  needFullRefresh = true;
  isWifiConnected = true; 
  display.init(115200, false);
  display.setRotation(1);
  
  // ИСПРАВЛЕНО: Прямой вызов отрисовки экрана, так как системное время уже валидно после syncTimeNTP()
  updateClockDisplay();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  Serial.println("Точка А зафиксирована. Ждем ровно 60 секунд по millis()...");
  
  while (millis() - startMillis < 60000UL) {
    digitalWrite(BOARD_LED, LOW);  delay(100);
    digitalWrite(BOARD_LED, HIGH); delay(900);
  }
  
  unsigned long endMillis = millis();
  syncTimeNTP(); 
  configureSystemTime(); 
  time_t endNtp = time(NULL);
  
  long realElapsedSec = endNtp - startNtp;
  double internalElapsedSec = (double)(endMillis - startMillis) / 1000.0;
  
  if (realElapsedSec > 10) {
    double driftSeconds = internalElapsedSec - (double)realElapsedSec;
    double driftPerSecond = driftSeconds / internalElapsedSec;
    rtcCalculatedOffsetSec = driftPerSecond * 86400.0;
    rtcIsCalibrated = true;
    lastSyncTimestamp = endNtp;
    
    Serial.println("\n=== ПЕРВИЧНАЯ КАЛИБРОВКА ПО MILLIS ЗАВЕРШЕНА ===");
    Serial.print("Реально прошло (NTP):      "); Serial.print(realElapsedSec); Serial.println(" сек.");
    Serial.print("Таймер миллисекунд выдал:  "); Serial.print(internalElapsedSec, 3); Serial.println(" сек.");
    Serial.print("Вычисленная поправка в сутки: "); Serial.print(rtcCalculatedOffsetSec, 2); Serial.println(" сек.");
    Serial.println("======================================\n");
  }
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  isWifiConnected = false;

  configureSystemTime();

  needFullRefresh = true;
  updateClockDisplay();
}

void setup() {
  Serial.begin(115200);   
  
  pinMode(BOARD_LED, OUTPUT);     digitalWrite(BOARD_LED, HIGH); 
  pinMode(4, OUTPUT);             digitalWrite(4, LOW); 
  pinMode(BUTTON_BOOT_PIN, INPUT_PULLUP);
  
  generateUniqueSSID();

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  Preferences prefs;
  prefs.begin("clock_settings", true); 
  dynamic_ssid = prefs.getString("wifi_ssid", DEFAULT_SSID); 
  dynamic_pass = prefs.getString("wifi_pass", DEFAULT_PASSWORD);
  dynamic_gmt_offset = prefs.getInt("gmt_offset", DEFAULT_GMT_OFFSET); 
  prefs.end();

  configureSystemTime();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || digitalRead(BUTTON_BOOT_PIN) == LOW || dynamic_ssid == DEFAULT_SSID) {
    isConfigMode = true;
    digitalWrite(BOARD_LED, LOW); 
    display.init(115200, false);
    display.setRotation(1);
    needFullRefresh = true; 
    updateClockDisplay(); 
    initWebServer(); 
    return; 
  }

  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED || !rtcIsCalibrated) {
    performStartupCalibration();
  }

  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  bool justSynced = false;

  if (timeinfo.tm_hour == 3 && timeinfo.tm_min == 0) {
    Serial.println("Наступило 3:00 ночи. Плановая суточная синхронизация времени...");
    syncTimeNTP();
    configureSystemTime(); 
    time(&now);
    localtime_r(&now, &timeinfo);
    justSynced = true;
  }

  display.init(115200, false); 
  display.setRotation(1); 
  
  if (timeinfo.tm_min != rtcLastMinute || justSynced) {
    rtcLastMinute = timeinfo.tm_min;
    if (justSynced) needFullRefresh = true;
    updateClockDisplay(); 
  }
  
  int secondsToNextMinute = 60 - timeinfo.tm_sec;
  if (secondsToNextMinute <= 0) secondsToNextMinute = 60;
  
  enterDeepSleep(secondsToNextMinute);
}

void loop() {
  if (isConfigMode) {
    handleWebServer();
    delay(10);
  }
}
