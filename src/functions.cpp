#include "settings.h"

extern bool rtcIsCalibrated;

const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

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

void drawBatteryIcon(int x, int y, int width, int height, int percentage) {
  display.drawRect(x, y, width, height, GxEPD_BLACK);
  
  int protrusionHeight = height / 3;
  int protrusionY = y + (height - protrusionHeight) / 2;
  display.fillRect(x + width, protrusionY, 2, protrusionHeight, GxEPD_BLACK);
  
  int maxFillWidth = width - 4;
  int fillWidth = (maxFillWidth * percentage) / 100;
  
  if (fillWidth > 0) {
    display.fillRect(x + 2, y + 2, fillWidth, height - 4, GxEPD_BLACK);
  }
}

void updateClockDisplay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Ошибка получения времени для дисплея");
    return;
  }

  int currentBatteryPercent = getBatteryPercentage();

  char timeString[16];
  char dateString[64]; 
  char batString[16]; 

  snprintf(timeString, sizeof(timeString), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  snprintf(dateString, sizeof(dateString), "%s, %d %s %d", days[timeinfo.tm_wday], timeinfo.tm_mday, months[timeinfo.tm_mon], timeinfo.tm_year + 1900);
  snprintf(batString, sizeof(batString), "%d%%", currentBatteryPercent);

  if (needFullRefresh) {
    display.setFullWindow();
    needFullRefresh = false;
  } else {
    display.setPartialWindow(0, 0, display.width(), display.height());
  }

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);       
    display.setTextColor(GxEPD_BLACK);
    
    if (isConfigMode) {
      display.setFont(NULL); 
      display.setTextSize(2);
      display.setCursor(10, 15); display.print("WIFI CONFIG MODE");
      display.setTextSize(1);
      display.setCursor(10, 50); display.print("Connect to AP: ");
      display.setCursor(10, 65); display.print(generated_ap_ssid);
      display.setCursor(10, 95); display.print("URL: 192.168.111.1");
    } 
    // ЭКРАН СТАРТОВОЙ КАЛИБРОВКИ
    else if (!rtcIsCalibrated) {
      display.setFont(FONT_TEXT);
      
      // ИСПРАВЛЕНО: Масштаб уменьшен до 1. Шрифт стал тонким и гладким
      display.setTextSize(1);
      
      // Отрисовка двух ровных и аккуратных сервисных строк по центру экрана
      display.setCursor(15, 50);  display.print("Calibrating RTC...");
      display.setCursor(15, 80);  display.print("Please wait 60 seconds");
    } 
    // ОБЫЧНЫЙ РЕЖИМ РАБОТЫ ЧАСОВ
    else {
      int16_t rightEdgeX = 244; 

      drawBatteryIcon(rightEdgeX - 24, 6, 20, 11, currentBatteryPercent);
      
      display.setFont(NULL); 
      display.setTextSize(1);
      
      int16_t batTextWidth = strlen(batString) * 6;
      int16_t batTextX = (rightEdgeX - 24) + (20 / 2) - (batTextWidth / 2) + 1; 
      
      display.setCursor(batTextX, 6 + 11 + 4); 
      display.print(batString);
      
      if (isWifiConnected) {
        display.setFont(FONT_TEXT);
        display.setCursor(10, 20); 
        display.print("WiFi");
      }

      display.setFont(FONT_CLOCK); 
      display.setTextSize(1);   
      display.setCursor(4, 84); 
      display.print(timeString);
      
      display.setFont(FONT_TEXT);
      display.setTextSize(1); 
      display.setCursor(10, 110); 
      display.print(dateString);
    }
    
  } while (display.nextPage());

  display.powerOff(); 
}

void syncTimeNTP() {
  Serial.println("Starting NTP Sync...");
  isWifiConnected = false; 

  WiFi.begin(dynamic_ssid.c_str(), dynamic_pass.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { delay(500); Serial.print("."); attempts++; }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! Syncing time...");
    isWifiConnected = true; 
    
    long calculated_gmt_sec = (long)dynamic_gmt_offset * 3600L;
    configTime(calculated_gmt_sec, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    struct tm timeinfo;
    for (int i = 0; i < 10; i++) {
      if (getLocalTime(&timeinfo)) { 
        Serial.println("Time synchronized successfully!"); 
        break; 
      }
      delay(500);
    }
  } else {
    Serial.println("\nНе удалось подключиться к роутеру.");
  }
}
