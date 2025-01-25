#include "esp_camera.h"
#include <WiFi.h>
#include "esp_system.h"

// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "MikroTik-ED936E";
const char *password = "boonofoxboonofox";

void startCameraServer();
void setupLedFlash(int pin);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Debugging: Check reset reason
  esp_reset_reason_t reset_reason = esp_reset_reason();
  Serial.print("Reset reason: ");
  switch (reset_reason) {
    case ESP_RST_POWERON: Serial.println("Power-on reset"); break;
    case ESP_RST_SW: Serial.println("Software reset"); break;
    case ESP_RST_PANIC: Serial.println("Exception/panic reset"); break;
    case ESP_RST_INT_WDT: Serial.println("Interrupt watchdog reset"); break;
    case ESP_RST_TASK_WDT: Serial.println("Task watchdog reset"); break;
    case ESP_RST_BROWNOUT: Serial.println("Brownout reset"); break;
    default: Serial.println("Unknown reset reason"); break;
  }

  // Debugging: Check SCCB I2C Port configuration
  Serial.println("Checking SCCB I2C Port...");
#if CONFIG_SCCB_HARDWARE_I2C_PORT0
  Serial.println("SCCB is using I2C0.");
#elif CONFIG_SCCB_HARDWARE_I2C_PORT1
  Serial.println("SCCB is using I2C1.");
#else
  Serial.println("SCCB port configuration not set.");
#endif

  camera_config_t config;
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
  //config.frame_size = FRAMESIZE_UXGA; 
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_RGB565; 
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // Debugging: Initialize camera and print errors
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
  if (err == ESP_ERR_CAMERA_NOT_DETECTED) {
    Serial.println("Camera not detected. Check the connection.");
  } else if (err == 0x106) { // Replace ESP_ERR_CAMERA_FAILED_TO_INIT with its value
    Serial.println("Camera initialization failed. Check configuration or power supply.");
  }
    return;
  }
  

  sensor_t *s = esp_camera_sensor_get();
  if (s == NULL) {
    Serial.println("Failed to get camera sensor.");
    return;
  }

  // Debugging: Print sensor ID
  Serial.print("Camera sensor PID: ");
  Serial.println(s->id.PID, HEX);

  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void loop() {
  delay(10000); // Do nothing. Everything is handled by the web server.
}

