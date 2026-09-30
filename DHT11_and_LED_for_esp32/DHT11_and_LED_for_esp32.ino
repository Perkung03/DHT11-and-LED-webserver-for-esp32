#include <WiFi.h>
#include "DHT.h"
#include <Adafruit_Sensor.h>
#include "ESPAsyncWebServer.h"

// ---------- DHT11 ----------
#define DHTPIN 27
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---------- WiFi ----------
const char* ssid = "---";
const char* password = "---";

// ---------- Web server ----------
AsyncWebServer server(80);

// ---------- LEDs ----------
const int LED1_PIN = 26;
const int LED2_PIN = 25;
bool led1On = false;
bool led2On = false;

// ---------- Cached sensor values ----------
float temperature = NAN;
float humidity = NAN;
unsigned long lastRead = 0;
const unsigned long READ_INTERVAL = 2000;

// ---------- Web page ----------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>DHT11 + LED</title>
<style>
  * { box-sizing: border-box; }
  body {
    margin: 0;
    padding: 16px;
    font-family: Arial, sans-serif;
    background: #f2f2f2;
    color: #222;
  }
  h1 {
    text-align: center;
    font-size: 1.6rem;
    margin: 4px 0 16px;
  }
  .board {
    max-width: 900px;
    margin: 0 auto;
    background: #fff;
    border: 3px solid #d9534f;
    border-radius: 16px;
    padding: 20px;
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 24px;
  }
  @media (max-width: 640px) {
    .board { grid-template-columns: 1fr; }
  }
  .col { display: flex; flex-direction: column; gap: 24px; }

  .title {
    font-size: 1.5rem;
    font-weight: bold;
    text-decoration: underline;
    text-decoration-color: #d9534f;
    text-underline-offset: 6px;
    margin-bottom: 6px;
  }
  .gauge { text-align: center; }
  .gauge svg { width: 100%; max-width: 320px; }
  .gauge .value { font-size: 1.8rem; font-weight: bold; margin-top: -6px; }
  .gauge .unit { font-size: 1rem; color: #666; }

  .card {
    border: 2px solid #3b5b9c;
    border-radius: 18px;
    padding: 16px;
  }
  .card .title { display: flex; justify-content: space-between; align-items: center; }
  .dot {
    width: 18px; height: 18px; border-radius: 50%;
    background: #bbb; display: inline-block;
    border: 2px solid #888;
  }
  .dot.on { background: #ffd400; border-color: #c9a800; box-shadow: 0 0 12px #ffd400; }
  .btn {
    display: block;
    width: 100%;
    margin: 14px 0;
    padding: 18px;
    font-size: 1.4rem;
    font-weight: bold;
    background: #fff;
    border: 2px solid #3b5b9c;
    border-radius: 8px;
    cursor: pointer;
  }
  .btn:active { transform: scale(0.98); }
  .btn.on.active  { background: #28a745; color: #fff; border-color: #1e7e34; }
  .btn.off.active { background: #dc3545; color: #fff; border-color: #a71d2a; }
</style>
</head>
<body>
<h1>DHT11 + LED 2 ตัว</h1>
<div id="status" style="text-align:center;color:#d9534f;min-height:1.2em"></div>

<div class="board">

  <!-- LEFT: gauges -->
  <div class="col">

    <div class="gauge">
      <div class="title">Temp</div>
      <svg viewBox="0 0 200 120" id="gTemp"></svg>
      <div class="value"><span id="tVal">--</span> <span class="unit">&deg;C</span></div>
    </div>

    <div class="gauge">
      <div class="title">Hum</div>
      <svg viewBox="0 0 200 120" id="gHum"></svg>
      <div class="value"><span id="hVal">--</span> <span class="unit">%</span></div>
    </div>

  </div>

  <!-- RIGHT: LEDs -->
  <div class="col">

    <div class="card">
      <div class="title">LED 1 <span class="dot" id="dot1"></span></div>
      <button class="btn on"  id="l1on"  onclick="setLed(1,1)">ON</button>
      <button class="btn off" id="l1off" onclick="setLed(1,0)">OFF</button>
    </div>

    <div class="card">
      <div class="title">LED 2 <span class="dot" id="dot2"></span></div>
      <button class="btn on"  id="l2on"  onclick="setLed(2,1)">ON</button>
      <button class="btn off" id="l2off" onclick="setLed(2,0)">OFF</button>
    </div>

  </div>
</div>

<script>
  // Build a semicircle gauge inside an <svg>
  function buildGauge(svgId, max, step, color) {
    var svg = document.getElementById(svgId);
    var cx = 100, cy = 100, r = 80;
    var html = "";

    // background arc
    html += '<path d="M20 100 A80 80 0 0 1 180 100" fill="none" stroke="#ddd" stroke-width="12" stroke-linecap="round"/>';
    // value arc
    html += '<path id="' + svgId + 'Arc" d="M20 100 A80 80 0 0 1 180 100" fill="none" stroke="' + color +
            '" stroke-width="12" stroke-linecap="round" pathLength="100" stroke-dasharray="0 100" style="transition: stroke-dasharray .6s;"/>';

    // tick marks
    for (var v = 0; v <= max; v += step) {
      var a = Math.PI * (1 - v / max);
      var x1 = cx + (r - 14) * Math.cos(a), y1 = cy - (r - 14) * Math.sin(a);
      var x2 = cx + (r - 4)  * Math.cos(a), y2 = cy - (r - 4)  * Math.sin(a);
      html += '<line x1="' + x1 + '" y1="' + y1 + '" x2="' + x2 + '" y2="' + y2 + '" stroke="#d9534f" stroke-width="2"/>';
    }

    // needle
    html += '<g id="' + svgId + 'Needle" style="transform-origin:100px 100px; transform:rotate(-90deg); transition: transform .6s;">' +
            '<line x1="100" y1="100" x2="100" y2="34" stroke="#3b5b9c" stroke-width="3" stroke-linecap="round"/></g>';
    html += '<circle cx="100" cy="100" r="6" fill="#3b5b9c"/>';

    // min / max labels
    html += '<text x="20" y="116" text-anchor="middle" font-size="12" fill="#444">0</text>';
    html += '<text x="180" y="116" text-anchor="middle" font-size="12" fill="#444">' + max + '</text>';

    svg.innerHTML = html;
  }

  function setGauge(svgId, value, max) {
    var pct = Math.max(0, Math.min(1, value / max));
    document.getElementById(svgId + 'Needle').style.transform = 'rotate(' + (-90 + pct * 180) + 'deg)';
    document.getElementById(svgId + 'Arc').setAttribute('stroke-dasharray', (pct * 100) + ' 100');
  }

  buildGauge('gTemp', 60, 5, '#059e8a');
  buildGauge('gHum', 100, 10, '#00add6');

  function showLed(n, on) {
    document.getElementById('dot' + n).className = 'dot' + (on ? ' on' : '');
    document.getElementById('l' + n + 'on').className  = 'btn on'  + (on ? ' active' : '');
    document.getElementById('l' + n + 'off').className = 'btn off' + (on ? '' : ' active');
  }

  function setLed(n, on) {
    fetch('/led' + n + '/' + (on ? 'on' : 'off'), { cache: 'no-store' })
      .then(function (r) {
        if (!r.ok) throw new Error('HTTP ' + r.status);
        showLed(n, on);
      })
      .catch(function (e) {
        document.getElementById('status').textContent = 'LED error: ' + e;
      });
  }

  function update() {
    fetch('/data', { cache: 'no-store' })
      .then(function (r) {
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return r.json();
      })
      .then(function (d) {
        document.getElementById('status').textContent = '';
        if (d.t !== null) {
          document.getElementById('tVal').textContent = d.t.toFixed(1);
          setGauge('gTemp', d.t, 60);
        }
        if (d.h !== null) {
          document.getElementById('hVal').textContent = d.h.toFixed(0);
          setGauge('gHum', d.h, 100);
        }
        showLed(1, d.led1 === 1);
        showLed(2, d.led2 === 1);
      })
      .catch(function (e) {
        document.getElementById('status').textContent = 'Update error: ' + e;
      });
  }

  update();
  setInterval(update, 2000);
</script>
</body>
</html>
)rawliteral";

// ---------- Helpers ----------
String buildJson() {
  String json = "{";
  json += "\"t\":" + (isnan(temperature) ? String("null") : String(temperature, 1)) + ",";
  json += "\"h\":" + (isnan(humidity) ? String("null") : String(humidity, 1)) + ",";
  json += "\"led1\":" + String(led1On ? 1 : 0) + ",";
  json += "\"led2\":" + String(led2On ? 1 : 0);
  json += "}";
  return json;
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nBooting sketch...");

  dht.begin();

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("\nFailed, status = %d\n", WiFi.status());
  } else {
    Serial.print("\nIP address: ");
    Serial.println(WiFi.localIP());
  }

  // Main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // Sensor + LED data (JSON)
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", buildJson());
  });

  // LED 1 (GPIO 26)
  server.on("/led1/on", HTTP_GET, [](AsyncWebServerRequest *request) {
    led1On = true;
    digitalWrite(LED1_PIN, HIGH);
    Serial.println("LED 1 ON");
    request->send(200, "text/plain", "OK");
  });
  server.on("/led1/off", HTTP_GET, [](AsyncWebServerRequest *request) {
    led1On = false;
    digitalWrite(LED1_PIN, LOW);
    Serial.println("LED 1 OFF");
    request->send(200, "text/plain", "OK");
  });

  // LED 2 (GPIO 25)
  server.on("/led2/on", HTTP_GET, [](AsyncWebServerRequest *request) {
    led2On = true;
    digitalWrite(LED2_PIN, HIGH);
    Serial.println("LED 2 ON");
    request->send(200, "text/plain", "OK");
  });
  server.on("/led2/off", HTTP_GET, [](AsyncWebServerRequest *request) {
    led2On = false;
    digitalWrite(LED2_PIN, LOW);
    Serial.println("LED 2 OFF");
    request->send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("HTTP server started.");
}

// ---------- Loop ----------
void loop() {
  if (millis() - lastRead >= READ_INTERVAL) {
    lastRead = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      temperature = t;
      humidity = h;
      Serial.printf("Temperature: %.1f C, Humidity: %.0f %%\n", t, h);
    }
  }
}
