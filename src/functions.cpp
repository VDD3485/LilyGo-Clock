#include "settings.h"

const char* days[] = {"ВС", "ПН", "ВТ", "СР", "ЧТ", "ПТ", "СБ"};
const char* months[] = {"ЯНВ", "ФЕВ", "МАР", "АПР", "МАЙ", "ИЮН", "ИЮЛ", "АВГ", "СЕН", "ОКТ", "НОЯ", "ДЕК"};

int getBatteryPercentage() {
  uint32_t raw_adc = 0;
  for (int i = 0; i < 10; i++) { raw_adc += analogRead(BAT_ADC_PIN); delay(2); }
  raw_adc /= 10;
  float v_bat = ((float)raw_adc / 4095.0) * 2.0 * 3.3 * 1.1; 
  int percent = (v_bat - 3.2) / (4.2 - 3.2) * 100.0;
  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;
  return percent;
}

void updateClockDisplay() {
  display.setPartialWindow(0, 0, display.width(), display.height());

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);       
    u8g2Fonts.setForegroundColor(GxEPD_BLACK); 
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    
    if (isConfigMode) {
      u8g2Fonts.setFont(FONT_DATE);
      u8g2Fonts.setCursor(10, 25);  u8g2Fonts.print("РЕЖИМ НАСТРОЙКИ Wi-Fi");
      u8g2Fonts.setCursor(10, 50);  u8g2Fonts.print("Подключитесь к сети:");
      u8g2Fonts.setCursor(10, 70);  u8g2Fonts.print(generated_ap_ssid); 
      u8g2Fonts.setCursor(10, 105); u8g2Fonts.print("Перейдите на: 192.168.111.1");
    } else {
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) continue;

      char timeString[16];
      char dateString[64]; 
      char batString[16];
      
      sprintf(timeString, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
      sprintf(dateString, "%s, %d %s %d", days[timeinfo.tm_wday], timeinfo.tm_mday, months[timeinfo.tm_mon], timeinfo.tm_year + 1900);
      sprintf(batString, "%d%%", getBatteryPercentage());

      // Правая граница для выравнивания текста (с отступом от края 250)
      int16_t rightEdgeX = 244; 

      // 1. Уровень заряда батареи (Шрифт cu12, аккуратный и ровный)
      u8g2Fonts.setFont(FONT_BATTERY); 
      int16_t batWidth = u8g2Fonts.getUTF8Width(batString);
      u8g2Fonts.setCursor(rightEdgeX - batWidth, 20); 
      u8g2Fonts.print(batString);
      
      // 2. Надпись WiFi выравнивается по правой линии под батареей
      if (isWifiConnected) {
        u8g2Fonts.setFont(FONT_WIFI);
        int16_t wifiWidth = u8g2Fonts.getUTF8Width(TEXT_WIFI_STATUS);
        u8g2Fonts.setCursor(rightEdgeX - wifiWidth, 40); 
        u8g2Fonts.print(TEXT_WIFI_STATUS);
      }

      // 3. ОГРОМНЫЕ ЧАСЫ (выравнивание по левому краю X=10)
      u8g2Fonts.setFont(FONT_CLOCK); 
      u8g2Fonts.setCursor(10, 72); 
      u8g2Fonts.print(timeString);
      
      // 4. ИСПРАВЛЕНО: Отрисовка ОГРОМНОЙ ДАТЫ (Чистый вывод без искусственного сдвига пикселей)
      u8g2Fonts.setFont(FONT_DATE); 
      u8g2Fonts.setCursor(10, 118); // Опущено до упора вниз, чтобы текст стоял идеально
      u8g2Fonts.print(dateString);
    }
    
  } while (display.nextPage());

  display.powerOff(); 
}

void syncTimeNTP() {
  Serial.println("Starting NTP Sync...");
  isWifiConnected = true;
  updateClockDisplay();
  delay(500); 

  WiFi.begin(dynamic_ssid.c_str(), dynamic_pass.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { delay(500); Serial.print("."); attempts++; }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! Syncing time...");
    
    long calculated_gmt_sec = (long)dynamic_gmt_offset * 3600L;
    configTime(calculated_gmt_sec, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    struct tm timeinfo;
    for (int i = 0; i < 10; i++) {
      if (getLocalTime(&timeinfo)) { Serial.println("Time synchronized successfully!"); break; }
      delay(500);
    }
    
    isWifiConnected = true; 
    updateClockDisplay();
    delay(4000); 
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    isConfigMode = false;
  } else {
    Serial.println("\nWi-Fi не найден. Запуск веб-настроек...");
    WiFi.disconnect();
    initWebServer(); 
    isConfigMode = true;
  }

  isWifiConnected = false;
  updateClockDisplay();
}
