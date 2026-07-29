#include "settings.h"
#include <sys/time.h>
#include <WiFiUdp.h>

// ЭТИ ПЕРЕМЕННЫЕ НА СТОПРОЦЕНТОВ ВЫЖИВАЮТ В ГЛУБОКОМ СНЕ
RTC_DATA_ATTR time_t lastSyncTimestamp = 0; 
RTC_DATA_ATTR int rtcLastMinute = -1;
RTC_DATA_ATTR double rtcDriftFactor = 0.0;      
RTC_DATA_ATTR bool rtcIsCalibrated = false;     

// ЖЕСТКИЙ ФИКС ЛИНКОВКИ: Физическое создание RTC-переменных грозы происходит только здесь!
RTC_DATA_ATTR int rtcStormDistance = 99; 
RTC_DATA_ATTR bool rtcIsStormAlert = false;

bool isWifiConnected = false;               
bool needFullRefresh = false;                
bool isConfigMode = false; 

String dynamic_ssid;
String dynamic_pass;
int dynamic_gmt_offset;
char generated_ap_ssid[32]; 

GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
WebServer server(80); 

void generateUniqueSSID() {
  uint8_t mac[6]; 
  WiFi.macAddress(mac); 
  uint16_t mac_id = (mac[4] << 8) | mac[5];
  uint16_t unique_id = mac_id ^ 3485; 
  snprintf(generated_ap_ssid, sizeof(generated_ap_ssid), "Clock_%04X", unique_id);
}

time_t getDirectNtpSeconds() {
  WiFiUDP udp;
  if (!udp.begin(2390)) return 0; 
  
  uint8_t ntpPacketBuffer[48];
  memset(ntpPacketBuffer, 0, 48);
  ntpPacketBuffer[0] = 0b11100011; 
  
  IPAddress ntpServerIP;
  if (!WiFi.hostByName(NTP_SERVER, ntpServerIP)) return 0;
  
  udp.beginPacket(ntpServerIP, 123); 
  udp.write(ntpPacketBuffer, 48);
  udp.endPacket();
  
  unsigned long startWait = millis();
  while (udp.parsePacket() == 0) {
    if (millis() - startWait > 1500) return 0;
    delay(10);
  }
  
  udp.read(ntpPacketBuffer, 48);
  
  unsigned long highWord = word(ntpPacketBuffer[40], ntpPacketBuffer[41]);
  unsigned long lowWord = word(ntpPacketBuffer[42], ntpPacketBuffer[43]);
  unsigned long secsSince1900 = highWord << 16 | lowWord;
  
  return secsSince1900 - 2208988800UL;
}

void configureSystemTime() {
  configTime(0, 0, NTP_SERVER);
  
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
  digitalWrite(BOARD_LED, LOW); 
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  esp_sleep_enable_timer_wakeup((uint64_t)secondsToSleep * 1000000ULL);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)AS3935_IRQ_PIN, 1); 
  esp_deep_sleep_start();
}

void performStartupCalibration() {
  Serial.println("\n>>> ЗАПУСК ПРЯМОЙ КАЛИБРОВКИ КВАРЦА БЕЗ СДВИГА ЯДРА (5 МИНУТ) <<<");
  
  needFullRefresh = true;
  display.init(115200, false);
  display.setRotation(1);
  
  struct timeval tv_init = { .tv_sec = 1770000000, .tv_usec = 0 };
  settimeofday(&tv_init, NULL);
  
  bool oldState = rtcIsCalibrated;
  rtcIsCalibrated = false; 
  updateClockDisplay(); 
  rtcIsCalibrated = oldState;

  WiFi.begin(dynamic_ssid.c_str(), dynamic_pass.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { delay(500); attempts++; }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Не удалось подключиться к роутеру для калибровки.");
    return;
  }
  isWifiConnected = true;

  time_t startNtp = getDirectNtpSeconds();
  unsigned long startMillis = millis();
  
  if (startNtp == 0) {
    Serial.println("Ошибка получения Точки А от NTP. Пропуск.");
    return;
  }
  
  Serial.print("Точка А получена: "); Serial.print(startNtp); Serial.println(" UTC. Начинаем непрерывный отсчет...");

  unsigned long lastDotMillis = startMillis;
  pinMode(BOARD_LED, OUTPUT);
  
  while (millis() - startMillis < 300000UL) { 
    if (millis() - lastDotMillis >= 5000UL) {
      digitalWrite(BOARD_LED, HIGH);  
      delay(100);
      digitalWrite(BOARD_LED, LOW);   
      Serial.print(".");
      lastDotMillis = millis();
    }
    delay(10); 
  }
  Serial.println();
  
  time_t endNtp = getDirectNtpSeconds();
  unsigned long endMillis = millis(); 
  
  if (endNtp == 0) {
    Serial.println("Ошибка получения Точки Б от NTP. Калибровка сорвана.");
    return;
  }

  long realElapsedSec = endNtp - startNtp; 
  double internalElapsedSec = (double)(endMillis - startMillis) / 1000.0; 
  
  if (realElapsedSec > 250 && realElapsedSec < 350) {
    double driftSeconds = internalElapsedSec - (double)realElapsedSec; 
    
    rtcDriftFactor = -(driftSeconds / internalElapsedSec);
    rtcIsCalibrated = true;
    
    struct timeval tv_final = { .tv_sec = endNtp, .tv_usec = 0 };
    settimeofday(&tv_final, NULL);
    lastSyncTimestamp = endNtp;
    
    Serial.println("\n=== UDP АЛГОРИТМ КАЛИБРОВКИ УСПЕШНО ЗАВЕРШЕН ===");
    Serial.print("Реально прошло времени (NTP):   "); Serial.print(realElapsedSec); Serial.println(" сек.");
    Serial.print("Непрерывный таймер насчитал:    "); Serial.print(internalElapsedSec, 3); Serial.println(" сек.");
    Serial.print("Чистая физическая погрешность: "); Serial.print(driftSeconds, 3); Serial.println(" сек.");
    Serial.print("Рассчитанный drift-коэффициент: "); Serial.println(rtcDriftFactor, 8);
    Serial.println("================================================\n");
  } else {
    Serial.print("Сбой дельты. Прошло: "); Serial.println(realElapsedSec);
    rtcIsCalibrated = false;
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
  
  pinMode(BOARD_LED, OUTPUT);     digitalWrite(BOARD_LED, LOW); 
  pinMode(4, OUTPUT);             digitalWrite(4, LOW); 
  pinMode(BUTTON_BOOT_PIN, INPUT_PULLUP);
  pinMode(AS3935_IRQ_PIN, INPUT); 
  
  generateUniqueSSID();

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Фоновое пробуждение по сигналу IRQ от AS3935!");
    
    // Сюда пойдет логика вашей библиотеки чтения AS3935
    rtcIsStormAlert = true;
    rtcStormDistance = 8; // ДЛЯ ТЕСТА: Симулируем грозу на расстоянии 8 км
    
    if (rtcStormDistance > 30) {
      rtcIsStormAlert = false;
    }
    
    enterDeepSleep(60); 
  }

  Preferences prefs;
  prefs.begin("clock_settings", true); 
  dynamic_ssid = prefs.getString("wifi_ssid", DEFAULT_SSID); 
  dynamic_pass = prefs.getString("wifi_pass", DEFAULT_PASSWORD);
  dynamic_gmt_offset = prefs.getInt("gmt_offset", DEFAULT_GMT_OFFSET); 
  prefs.end();

  configureSystemTime();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1 || digitalRead(BUTTON_BOOT_PIN) == LOW || dynamic_ssid == DEFAULT_SSID) {
    isConfigMode = true;
    digitalWrite(BOARD_LED, HIGH); 
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

  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  time_t now = tv_now.tv_sec;
  
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  bool justSynced = false;

  if (rtcIsCalibrated && timeinfo.tm_hour == 3 && timeinfo.tm_min == 0) {
    Serial.println("Наступило 3:00 ночи. Плановое суточное обновление по NTP...");
    syncTimeNTP();
    configureSystemTime(); 
    gettimeofday(&tv_now, NULL);
    now = tv_now.tv_sec;
    lastSyncTimestamp = now;
    justSynced = true;
  }

  time_t calibratedNow = now;
  if (rtcIsCalibrated && lastSyncTimestamp != 0) {
    long elapsedSinceSync = now - lastSyncTimestamp;
    if (elapsedSinceSync > 0) {
      long correctionSec = (long)((double)elapsedSinceSync * rtcDriftFactor);
      calibratedNow = now + correctionSec; 
      
      Serial.println("--- МИНУТНАЯ КОРРЕКЦИЯ ---");
      Serial.print("Прошло с синка:   "); Serial.print(elapsedSinceSync); Serial.println(" сек.");
      Serial.print("Поправка экрана:  "); Serial.print(correctionSec); Serial.println(" сек.");
      Serial.println("--------------------------");
    }
  }

  localtime_r(&calibratedNow, &timeinfo);

  display.init(115200, false); 
  display.setRotation(1); 
  
  if (timeinfo.tm_min != rtcLastMinute || justSynced) {
    rtcLastMinute = timeinfo.tm_min;
    if (justSynced || needFullRefresh) needFullRefresh = true;
    
    struct timeval tv_set = { .tv_sec = calibratedNow, .tv_usec = 0 };
    settimeofday(&tv_set, NULL);
    
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
