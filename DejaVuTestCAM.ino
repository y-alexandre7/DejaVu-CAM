#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// AI Thinker ESP32-CAM pin assignments
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

const char* AP_SSID = "DejaVu TestCAM";
// Change this to your own Wi-Fi password; it must be at least 8 characters.
const char* AP_PASSWORD = "change-this-password";

static httpd_handle_t pageServer = nullptr;
static httpd_handle_t streamServer = nullptr;

static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=123456789000000000000987654321";
static const char* STREAM_BOUNDARY =
    "\r\n--123456789000000000000987654321\r\n";
static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

bool startCamera() {
  camera_config_t config = {};

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_XGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera initialization failed: 0x%x\n", err);
    return false;
  }

  return true;
}

static esp_err_t pageHandler(httpd_req_t* req) {
  static const char page[] = R"HTML(
<!doctype html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-CAM</title>
  <style>
    body { font: 16px sans-serif; max-width: 760px; margin: 24px auto; padding: 0 12px; }
    img { width: 100%; height: auto; background: #222; }
    a { display: inline-block; margin: 12px 0; padding: 12px 16px;
        background: #1769aa; color: white; text-decoration: none; border-radius: 6px; }
  </style>
</head>
<body>
  <h1>ESP32-CAM</h1>
  <img id="live" alt="Live camera stream">
  <p><a href="/capture.jpg">Download photo</a></p>
  <p>To record video, start your phone or computer's screen recorder while this page is open.</p>
  <script>
    document.getElementById("live").src =
      "http://" + location.hostname + ":81/stream";
  </script>
</body>
</html>
)HTML";

  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t photoHandler(httpd_req_t* req) {
  camera_fb_t* frame = esp_camera_fb_get();

  if (frame == nullptr) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(
      req, "Content-Disposition", "attachment; filename=esp32cam.jpg");

  esp_err_t result =
      httpd_resp_send(req, reinterpret_cast<const char*>(frame->buf), frame->len);

  esp_camera_fb_return(frame);
  return result;
}

static esp_err_t streamHandler(httpd_req_t* req) {
  esp_err_t result = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (result != ESP_OK) {
    return result;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    camera_fb_t* frame = esp_camera_fb_get();

    if (frame == nullptr) {
      Serial.println("Could not capture stream frame");
      return ESP_FAIL;
    }

    result = httpd_resp_send_chunk(
        req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));

    char partHeader[64];
    int headerLength = snprintf(
        partHeader, sizeof(partHeader), STREAM_PART,
        static_cast<unsigned int>(frame->len));

    if (result == ESP_OK && headerLength > 0 &&
        headerLength < static_cast<int>(sizeof(partHeader))) {
      result = httpd_resp_send_chunk(req, partHeader, headerLength);
    } else if (result == ESP_OK) {
      result = ESP_FAIL;
    }

    if (result == ESP_OK) {
      result = httpd_resp_send_chunk(
          req, reinterpret_cast<const char*>(frame->buf), frame->len);
    }

    esp_camera_fb_return(frame);

    // A failed send usually means the viewer closed or lost the connection.
    if (result != ESP_OK) {
      break;
    }

    delay(20);
  }

  return result;
}

bool startWebServers() {
  httpd_config_t pageConfig = HTTPD_DEFAULT_CONFIG();
  pageConfig.server_port = 80;
  pageConfig.ctrl_port = 32768;
  pageConfig.max_uri_handlers = 2;

  if (httpd_start(&pageServer, &pageConfig) != ESP_OK) {
    Serial.println("Could not start the web/photo server");
    return false;
  }

  httpd_uri_t rootUri = {};
  rootUri.uri = "/";
  rootUri.method = HTTP_GET;
  rootUri.handler = pageHandler;

  httpd_uri_t photoUri = {};
  photoUri.uri = "/capture.jpg";
  photoUri.method = HTTP_GET;
  photoUri.handler = photoHandler;

  if (httpd_register_uri_handler(pageServer, &rootUri) != ESP_OK ||
      httpd_register_uri_handler(pageServer, &photoUri) != ESP_OK) {
    Serial.println("Could not register the photo page routes");
    return false;
  }

  httpd_config_t streamConfig = HTTPD_DEFAULT_CONFIG();
  streamConfig.server_port = 81;
  streamConfig.ctrl_port = 32769;
  streamConfig.max_uri_handlers = 1;

  if (httpd_start(&streamServer, &streamConfig) != ESP_OK) {
    Serial.println("Could not start the video stream server");
    return false;
  }

  httpd_uri_t streamUri = {};
  streamUri.uri = "/stream";
  streamUri.method = HTTP_GET;
  streamUri.handler = streamHandler;

  if (httpd_register_uri_handler(streamServer, &streamUri) != ESP_OK) {
    Serial.println("Could not register the stream route");
    return false;
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  if (!startCamera()) {
    Serial.println("Check the camera connection and AI Thinker board selection.");
    while (true) {
      delay(1000);
    }
  }

  WiFi.mode(WIFI_AP);

  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("Could not start the Wi-Fi access point.");
    while (true) {
      delay(1000);
    }
  }

  if (!startWebServers()) {
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Camera is ready.");
  Serial.print("Join Wi-Fi: ");
  Serial.println(AP_SSID);
  Serial.print("Open this page: http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("Live stream: http://192.168.4.1:81/stream");
}

void loop() {
  delay(1000);
}