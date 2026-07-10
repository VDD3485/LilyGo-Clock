#include "settings.h"

unsigned long lastSyncTime = 0;
const unsigned long syncInterval = 21600000; 
bool isWifiConnected = false;               
bool needFullRefresh = false;                
bool isConfigMode = false; 

String dynamic_ssid;
String dynamic_pass;
int dynamic_gmt_offset;

char generated_ap_ssid[32]; // ИСПРАВЛЕНО

GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
WebServer server(80); 

unsigned long buttonPressStartTime = 0;
bool isButtonPressed = false;

// Функция вычисления уникального суффикса xxxx из MAC и константы 3485
void generateUniqueSSID() {
  uint8_t mac[6];
  WiFi.macAddress(mac); // Считываем 6 байт MAC-адреса железа
  
  // Берем последние 2 байта MAC-адреса для вычисления уникального 16-битного числа
  uint16_t mac_id = (mac[4] << 8) | mac[5];
  uint16_t unique_id = mac_id ^ 3485; // Исключающее ИЛИ с вашей константой
  
  // Записываем результат в глобальную строку generated_ap_ssid
  sprintf(generated_ap_ssid, "Clock_%04X", unique_id);
}

void setup() {
  Serial.begin(115200);
  
  pinMode(BOARD_LED, OUTPUT); digitalWrite(BOARD_LED, LOW);  
  pinMode(4, OUTPUT); digitalWrite(4, LOW); 
  pinMode(BAT_ADC_PIN, INPUT); analogSetAttenuation(ADC_11db); 
  pinMode(BUTTON_BOOT_PIN, INPUT_PULLUP);
  
  // Вычисляем имя точки доступа ДО инициализации веб-сервера
  generateUniqueSSID();

  display.init(115200);
  display.setRotation(1); 
  u8g2Fonts.begin(display); 

  // Читаем настройки из Preferences Flash
  Preferences prefs;
  prefs.begin("clock_settings", true); 
  dynamic_ssid = prefs.getString("wifi_ssid", DEFAULT_SSID); 
  dynamic_pass = prefs.getString("wifi_pass", DEFAULT_PASSWORD);
  dynamic_gmt_offset = prefs.getInt("gmt_offset", DEFAULT_GMT_OFFSET); // По умолчанию +3
  prefs.end();
  
  syncTimeNTP();
  lastSyncTime = millis();
}

void loop() {
  if (digitalRead(BUTTON_BOOT_PIN) == LOW) {
    if (!isButtonPressed) {
      isButtonPressed = true;
      buttonPressStartTime = millis();
    } else {
      if (millis() - buttonPressStartTime >= 8000) {
        Preferences prefs;
        prefs.begin("clock_settings", false);
        prefs.clear(); 
        prefs.end();
        
        dynamic_ssid = DEFAULT_SSID;
        dynamic_pass = DEFAULT_PASSWORD;
        dynamic_gmt_offset = DEFAULT_GMT_OFFSET;
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(500);
        
        isConfigMode = true;
        initWebServer(); 
        isButtonPressed = false;
      }
    }
  } else {
    isButtonPressed = false;
  }

  if (isConfigMode) {
    handleWebServer();
    delay(2);
    return; 
  }

  static int lastMinute = -1;
  struct tm timeinfo;
  
  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_min != lastMinute && !isWifiConnected) {
      lastMinute = timeinfo.tm_min;
      
      if (timeinfo.tm_hour == 3 && timeinfo.tm_min == 0) {
        needFullRefresh = true;
      }
      updateClockDisplay();
    }
  }

  if (millis() - lastSyncTime >= syncInterval) {
    syncTimeNTP();
    lastSyncTime = millis(); 
  }
  delay(100); 
}
