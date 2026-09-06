/**
 * NEXUS Ocean Monitoring System - ESP32-CAM Firmware
 * Model: AI-Thinker ESP32-CAM (OV2640)
 *
 * Capabilities:
 *  1. Live MJPEG video stream (/stream) & snapshot capture (/capture)
 *  2. WiFi Station mode with auto-fallback to Access Point ("NEXUS-Ocean-Cam")
 *  3. Ingests sensor telemetry from Arduino over UART (U0RXD / GPIO 3)
 *  4. Serves JSON telemetry (/telemetry) with CORS for NEXUS Dashboard
 *  5. Controls high-power flash LED (/flash?state=on|off)
 *  6. Embedded status page (/)
 */

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ==========================================
// 1. NETWORK CONFIGURATION
// ==========================================
// Enter your WiFi credentials here (STA mode):
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Fallback Access Point (AP mode) if WiFi fails:
const char* AP_SSID   = "NEXUS-Ocean-Cam";
const char* AP_PASS   = "nexus12345"; // Min 8 chars

// ==========================================
// 2. AI-THINKER ESP32-CAM PINOUT
// ==========================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_LED_PIN      4

// ==========================================
// 3. TELEMETRY STATE
// ==========================================
struct OceanTelemetry {
  float waterTemp = 0.0;
  int   turbidity = 0;
  float airTemp = 0.0;
  float pressure = 0.0;
  float heading = 0.0;
  bool  flashOn = false;
  unsigned long lastUpdate = 0;
} telemetry;

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ==========================================
// 4. HTTP HANDLERS
// ==========================================

// --- Telemetry Handler (JSON with CORS) ---
static esp_err_t telemetry_handler(httpd_req_t *req) {
  char json[256];
  snprintf(json, sizeof(json),
    "{\"waterTemp\":%.2f,\"turbidity\":%d,\"airTemp\":%.2f,\"pressure\":%.2f,\"heading\":%.1f,\"flashState\":%s,\"uptime\":%lu}",
    telemetry.waterTemp,
    telemetry.turbidity,
    telemetry.airTemp,
    telemetry.pressure,
    telemetry.heading,
    telemetry.flashOn ? "true" : "false",
    millis() / 1000
  );

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, OPTIONS");
  return httpd_resp_send(req, json, strlen(json));
}

// --- Flash Control Handler ---
static esp_err_t flash_handler(httpd_req_t *req) {
  char query[32];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    char param[16];
    if (httpd_query_key_value(query, "state", param, sizeof(param)) == ESP_OK) {
      if (strcmp(param, "on") == 0) {
        digitalWrite(FLASH_LED_PIN, HIGH);
        telemetry.flashOn = true;
      } else if (strcmp(param, "off") == 0) {
        digitalWrite(FLASH_LED_PIN, LOW);
        telemetry.flashOn = false;
      }
    }
  }

  const char* resp = telemetry.flashOn ? "{\"flash\":\"on\"}" : "{\"flash\":\"off\"}";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, resp, strlen(resp));
}

// --- Single Frame Capture Handler ---
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=ocean_capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// --- Live MJPEG Stream Handler ---
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
      break;
    }

    if (fb->format != PIXFORMAT_JPEG) {
      bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
      esp_camera_fb_return(fb);
      fb = NULL;
      if (!jpeg_converted) {
        res = ESP_FAIL;
        break;
      }
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }

    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }

    if (res != ESP_OK) break;
  }
  return res;
}

// --- Status Index Handler ---
static esp_err_t index_handler(httpd_req_t *req) {
  const char* html = 
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>NEXUS Ocean Node Gateway</title>"
    "<style>"
    "body{font-family:sans-serif;background:#020617;color:#f8fafc;padding:20px;text-align:center;}"
    "h1{color:#00f0ff;} a{color:#10b981;text-decoration:none;margin:10px;display:inline-block;}"
    ".box{background:#0f172a;border:1px solid #1e293b;border-radius:12px;padding:20px;max-width:500px;margin:20px auto;}"
    "</style></head><body>"
    "<h1>NEXUS Ocean Node</h1>"
    "<div class='box'>"
    "<p>Camera stream and sensor telemetry gateway active.</p>"
    "<p><a href='/stream' target='_blank'>[ Live MJPEG Stream ]</a></p>"
    "<p><a href='/capture' target='_blank'>[ High-Res Snapshot ]</a></p>"
    "<p><a href='/telemetry' target='_blank'>[ JSON Telemetry ]</a></p>"
    "<p><a href='/flash?state=on'>[ Flash ON ]</a> | <a href='/flash?state=off'>[ Flash OFF ]</a></p>"
    "</div></body></html>";

  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, strlen(html));
}

// ==========================================
// 5. WEB SERVER INITIALIZATION
// ==========================================
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;

  httpd_uri_t index_uri     = { .uri = "/",          .method = HTTP_GET, .handler = index_handler,     .user_ctx = NULL };
  httpd_uri_t telemetry_uri = { .uri = "/telemetry",  .method = HTTP_GET, .handler = telemetry_handler, .user_ctx = NULL };
  httpd_uri_t capture_uri   = { .uri = "/capture",    .method = HTTP_GET, .handler = capture_handler,   .user_ctx = NULL };
  httpd_uri_t flash_uri     = { .uri = "/flash",      .method = HTTP_GET, .handler = flash_handler,     .user_ctx = NULL };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &telemetry_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &flash_uri);
    Serial.println(F("[HTTP] Control server started on port 80"));
  }

  // Stream on dedicated port (81) to avoid blocking control calls
  config.server_port = 81;
  config.ctrl_port = 32769;
  httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println(F("[HTTP] Video stream server started on port 81"));
  }
}

// ==========================================
// 6. SERIAL TELEMETRY PARSER
// ==========================================
void parseArduinoSerialLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line.startsWith("Turbidity ADC:")) {
    telemetry.turbidity = line.substring(14).toInt();
    telemetry.lastUpdate = millis();
  } else if (line.startsWith("Water Temp:")) {
    telemetry.waterTemp = line.substring(11).toFloat();
    telemetry.lastUpdate = millis();
  } else if (line.startsWith("Air Temp:")) {
    telemetry.airTemp = line.substring(9).toFloat();
    telemetry.lastUpdate = millis();
  } else if (line.startsWith("Air Pressure:")) {
    telemetry.pressure = line.substring(13).toFloat();
    telemetry.lastUpdate = millis();
  } else if (line.startsWith("Compass Heading:")) {
    telemetry.heading = line.substring(16).toFloat();
    telemetry.lastUpdate = millis();
  }
}

// ==========================================
// 7. SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(9600); // Connected to Arduino TX (D1)
  Serial.setRxBufferSize(512);

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  Serial.println(F("\n=== NEXUS OCEAN ESP32-CAM NODE INIT ==="));

  // --- Camera Configuration ---
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Frame size & quality config
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;  // 640x480 for smooth stream
    config.jpeg_quality = 12;             // 10-63 (lower = higher quality)
    config.fb_count     = 2;
  } else {
    config.frame_size   = FRAMESIZE_QVGA; // 320x240 fallback
    config.jpeg_quality = 14;
    config.fb_count     = 1;
  }

  // Camera Init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAMERA] Camera init failed with error 0x%x\n", err);
  } else {
    Serial.println(F("[CAMERA] Camera initialized successfully."));
    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
      s->set_vflip(s, 1);    // Invert vertically if mounted upside down
      s->set_brightness(s, 1);
    }
  }

  // --- Network Connection (STA with AP Fallback) ---
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.printf("[WIFI] Connecting to %s", WIFI_SSID);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("\n[WIFI] Connected! IP Address: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("\n[WIFI] STA connection failed. Launching Access Point..."));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print(F("[WIFI] AP Started! SSID: "));
    Serial.println(AP_SSID);
    Serial.print(F("[WIFI] AP Gateway IP: "));
    Serial.println(WiFi.softAPIP()); // Default: 192.168.4.1
  }

  // Start HTTP servers
  startCameraServer();

  Serial.println(F("=== SYSTEM READY ==="));
}

String serialBuffer = "";

void loop() {
  // Process incoming telemetry lines from Arduino
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      parseArduinoSerialLine(serialBuffer);
      serialBuffer = "";
    } else if (c != '\r') {
      serialBuffer += c;
      if (serialBuffer.length() > 256) {
        serialBuffer = ""; // Prevent buffer overflow
      }
    }
  }

  delay(5);
}
