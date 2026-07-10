#include "settings.h"

unsigned long configStartTime = 0;
const unsigned long configTimeout = 300000; 

// ИСПРАВЛЕНО: Числа в объекте IPAddress должны разделяться запятыми!
IPAddress local_IP(192, 168, 111, 1);
IPAddress gateway(192, 168, 111, 1);
IPAddress subnet(255, 255, 255, 0);

const char HTML_CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Настройки Часов LILYGO</title>
    <style>
        body { font-family: Arial, sans-serif; background: #f4f4f9; padding: 20px; text-align: center; }
        .card { background: white; padding: 30px; border-radius: 12px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); display: inline-block; text-align: left; max-width: 400px; width: 100%; }
        h2 { color: #333; text-align: center; }
        label { font-weight: bold; display: block; margin-top: 15px; color: #555; }
        input[type="text"], input[type="password"], select { width: 100%; padding: 10px; margin-top: 5px; box-sizing: border-box; border: 1px solid #ccc; border-radius: 6px; font-size: 14px; }
        input[type="submit"] { width: 100%; background: #4CAF50; color: white; padding: 12px; border: none; border-radius: 6px; margin-top: 25px; cursor: pointer; font-size: 16px; }
        input[type="submit"]:hover { background: #45a049; }
    </style>
</head>
<body>
    <div class="card">
        <h2>Настройка Параметров</h2>
        <form action="/save" method="POST">
            <label for="ssid">Имя Wi-Fi сети (SSID):</label>
            <input type="text" id="ssid" name="ssid" placeholder="Введите SSID" required>
            
            <label for="password">Пароль сети:</label>
            <input type="password" id="password" name="password" placeholder="Введите пароль">
            
            <label for="gmt">Часовой пояс (GMT):</label>
            <select id="gmt" name="gmt">
                <option value="-12">GMT -12</option> <option value="-11">GMT -11</option> <option value="-10">GMT -10</option>
                <option value="-9">GMT -9</option>   <option value="-8">GMT -8</option>   <option value="-7">GMT -7</option>
                <option value="-6">GMT -6</option>   <option value="-5">GMT -5</option>   <option value="-4">GMT -4</option>
                <option value="-3">GMT -3</option>   <option value="-2">GMT -2</option>   <option value="-1">GMT -1</option>
                <option value="0">GMT +0</option>    <option value="1">GMT +1</option>    <option value="2">GMT +2</option>
                <option value="3" selected>GMT +3 (Москва)</option>                       <option value="4">GMT +4</option>
                <option value="5">GMT +5</option>    <option value="6">GMT +6</option>    <option value="7">GMT +7</option>
                <option value="8">GMT +8</option>    <option value="9">GMT +9</option>    <option value="10">GMT +10</option>
                <option value="11">GMT +11</option>  <option value="12">GMT +12</option>
            </select>
            
            <input type="submit" value="Сохранить и перезагрузить">
        </form>
    </div>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", HTML_CONFIG_PAGE);
}

void handleSave() {
  if (server.hasArg("ssid")) {
    String req_ssid = server.arg("ssid");
    String req_pass = server.arg("password");
    int req_gmt = server.arg("gmt").toInt();

    Preferences prefs;
    prefs.begin("clock_settings", false);
    prefs.putString("wifi_ssid", req_ssid);
    prefs.putString("wifi_pass", req_pass);
    prefs.putInt("gmt_offset", req_gmt);
    prefs.end();

    String html = "<html><head><meta charset='UTF-8'></head><body style='font-family:Arial; text-align:center; padding-top:50px;'>";
    html += "<h3>Настройки успешно сохранены!</h3><p>Часы перезагружаются...</p></body></html>";
    server.send(200, "text/html", html);
    
    delay(2000);
    ESP.restart(); 
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void initWebServer() {
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(generated_ap_ssid); // ИСПРАВЛЕНО (убран &)
  
  Serial.print("Точка доступа активна: ");
  Serial.println(generated_ap_ssid); // ИСПРАВЛЕНО (убран &)
  Serial.print("IP адрес: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  
  configStartTime = millis(); 
  updateClockDisplay(); 
}

void handleWebServer() {
  server.handleClient();
  if (WiFi.softAPgetStationNum() > 0) {
    configStartTime = millis(); 
  }
  if (millis() - configStartTime >= configTimeout) {
    ESP.restart(); 
  }
}
