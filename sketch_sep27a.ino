#include <WiFi.h>
#include <DHT.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
const char* ssid = "Galaxy A34 5G";  
const char* password = "123123123";   

#define DHTPIN 23     
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
const char* telegramToken = "7551836601:AAFJ_WyFzfMF378cIJJfkHwAP1X-JcUPDR0";  
const char* chat_id = "1561559292";  
String serverName = "api.telegram.org";

WiFiClientSecure client; // Güvenli bağlantılar oluşturmak için kullanılan istemci

// Web sunucusu
WebServer server(80);

// Verileri tutacak değişkenler
float temperature;
float humidity;

// Zamanlayıcı için değişken
unsigned long lastTime = 0;
unsigned long timerDelay = 2000; // 2 saniyede bir veriyi güncelle

// Veri analizi için değişkenler
float tempSum = 0;
float humSum = 0;
float tempMax = -100; // Başlangıçta çok düşük bir değer
float tempMin = 100;  // Başlangıçta çok yüksek bir değer
float humMax = -100;  // Başlangıçta çok düşük bir değer
float humMin = 100;   // Başlangıçta çok yüksek bir değer
int dataCount = 0;

// HTML sayfası
const char* index_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 Sıcaklık ve Nem</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; }
    h1 { color: #333; }
    p { font-size: 1.5em; }
  </style>
</head>
<body>
  <h1>ESP32 Sıcaklık ve Nem Değerleri</h1>
  <p>Sıcaklık: <span id="temperature">...</span> °C</p>
  <p>Nem: <span id="humidity">...</span> %</p>
  <h2>Veri Analizi</h2>
  <p>Ortalama Sıcaklık: <span id="avgTemperature">...</span> °C</p>
  <p>Ortalama Nem: <span id="avgHumidity">...</span> %</p>
  <p>Max Sıcaklık: <span id="maxTemperature">...</span> °C</p>
  <p>Min Sıcaklık: <span id="minTemperature">...</span> °C</p>
  <p>Max Nem: <span id="maxHumidity">...</span> %</p>
  <p>Min Nem: <span id="minHumidity">...</span> %</p>

  <script>
    async function getData() {
      const response = await fetch('/data');
      const json = await response.json();
      document.getElementById('temperature').innerText = json.temperature;
      document.getElementById('humidity').innerText = json.humidity;
      document.getElementById('avgTemperature').innerText = json.avgTemperature;
      document.getElementById('avgHumidity').innerText = json.avgHumidity;
      document.getElementById('maxTemperature').innerText = json.maxTemperature;
      document.getElementById('minTemperature').innerText = json.minTemperature;
      document.getElementById('maxHumidity').innerText = json.maxHumidity;
      document.getElementById('minHumidity').innerText = json.minHumidity;
    }
    
    // Her 2 saniyede bir veriyi güncelle
    setInterval(getData, 2000);
  </script>
  
</body>
</html>
)rawliteral";

void setup() {
  // Seri port iletişimi
  Serial.begin(115200);

  // DHT sensörünü başlat
  dht.begin();
  delay(2000);  // Sensörün başlatılması için biraz zaman ver

  // Wi-Fi bağlantısını başlat
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("WiFi'ye bağlanılıyor...");
  }
  Serial.println("WiFi'ye bağlandı!");
  Serial.println(WiFi.localIP());
  client.setInsecure();
  // HTML ana sayfa endpoint'i
  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });

  // JSON veri gösterimi için endpoint ayarla
  server.on("/data", []() {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    
    // Analiz için verileri güncelle
    dataCount++;
    tempSum += temperature;
    humSum += humidity;

    if (temperature > tempMax) tempMax = temperature;
    if (temperature < tempMin) tempMin = temperature;
    if (humidity > humMax) humMax = humidity;
    if (humidity < humMin) humMin = humidity;

    // Ortalamaları hesapla
    float avgTemperature = tempSum / dataCount;
    float avgHumidity = humSum / dataCount;

    // JSON formatında veri gönder
    String json = "{";
    json += "\"temperature\":" + String(temperature) + ","; 
    json += "\"humidity\":" + String(humidity) + ","; 
    json += "\"avgTemperature\":" + String(avgTemperature) + ","; 
    json += "\"avgHumidity\":" + String(avgHumidity) + ","; 
    json += "\"maxTemperature\":" + String(tempMax) + ","; 
    json += "\"minTemperature\":" + String(tempMin) + ","; 
    json += "\"maxHumidity\":" + String(humMax) + ","; 
    json += "\"minHumidity\":" + String(humMin); 
    json += "}";

    server.send(200, "application/json", json);
  });

  // Sunucuyu başlat
  server.begin();
}
void sendTelegramMessage(String message) {
  if (client.connect(serverName.c_str(), 443)) {  // Telegram sunucusuna 443 portundan bağlan
    Serial.println("Telegram sunucusuna başarıyla bağlanıldı.");
    String url = "/bot" + String(telegramToken) + "/sendMessage?chat_id=" + chat_id + "&text=" + message;
    
    // HTTP isteği gönder
    client.println("GET " + url + " HTTP/1.1");
    client.println("Host: " + String(serverName));
    client.println("Connection: close");
    client.println();

    // Yanıtı okuyalım
    while (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);  // Gelen cevabı Serial Monitor'e yazdır
    }
  } else {
    Serial.println("Telegram sunucusuna bağlanılamadı.");
  }
}

void loop() {
  // 2 saniyede bir sıcaklık ve nem verilerini seri port ekranına yazdır
  if (millis() - lastTime > timerDelay) {
    lastTime = millis(); // Zamanlayıcıyı güncelle

    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    // Seri monitörde göster
    Serial.print("Sıcaklık: ");
    Serial.print(temperature);
    Serial.print(" °C ");
    Serial.print("Nem: ");
    Serial.print(humidity);
    Serial.println(" %");

    // Uyarı sistemi
    if (temperature < 10) {
      Serial.println("Uyarı: Sıcaklık 10 °C'nin altına düştü! Bu mahsuller için tehlikeli olabilir!");
       sendTelegramMessage("Sıcaklık tehlikeli seviyelere düştü!");
    }
    if (temperature > 35) {
      Serial.println("Uyarı: Sıcaklık 35 °C'yi geçti!Lütfen havalandırmayı açın!");
       sendTelegramMessage("Sıcaklık tehlikeli seviyelere çıktı lütfen havalandırmayı açın!");
    }
    if (humidity < 40) {
      Serial.println("Uyarı: Nem %40'ın altında.Bu mahsuller için tehlikeli olabilir!");
      sendTelegramMessage("Seranızın içindeki nem miktarı tehlikeli seviyelere düştü!");
    }
    delay(5000);
  }
  
  // Web sunucusu isteğini işlemek için
  server.handleClient();
}
