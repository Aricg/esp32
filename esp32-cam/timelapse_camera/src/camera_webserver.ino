#include "esp_camera.h"
#include <WiFi.h>
#include "esp_system.h"
#include "FS.h"
#include "SD_MMC.h"
#include <time.h>
#include "esp_task_wdt.h"  // For watchdog timer

#ifndef VERTICAL_FLIP
#define VERTICAL_FLIP 0  // Default to false if not defined
#endif

// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"

// ===========================
// WiFi credentials from env
// ===========================
#ifndef WIFI_SSID
#error "WIFI_SSID must be defined via build flags"
#endif
#ifndef WIFI_PASSWORD 
#error "WIFI_PASSWORD must be defined via build flags"
#endif

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;       // adjust if you need local time
const int daylightOffset_sec = 0;   // adjust if you have DST

// Constants for reliability
#define WDT_TIMEOUT_SECONDS 30      // Watchdog timeout in seconds
#define WIFI_RECONNECT_INTERVAL 60000 // Try to reconnect every 60 seconds
#define HEARTBEAT_INTERVAL 300000   // Update heartbeat file every 5 minutes
#define AUTO_RESET_INTERVAL 86400000 // Auto reset every 24 hours (86400000 ms)

// Global variables for reliability
unsigned long lastWifiCheck = 0;
unsigned long lastHeartbeat = 0;
unsigned long startTime = 0;
unsigned long photosCount = 0;

void startCameraServer();
void setupLedFlash(int pin);
void checkWiFiConnection();
void updateHeartbeat();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Initialize watchdog timer
  esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true); // Initialize with timeout and panic if not reset
  esp_task_wdt_add(NULL); // Add current thread to watchdog watch
  
  // Record the start time
  startTime = millis();

  // Print WiFi credentials
  Serial.print("Using WiFi SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("Using WiFi Password: ");
  Serial.println(WIFI_PASSWORD);

  // Initialize SD card - critical for operation
  int sdRetries = 0;
  while(!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card Mount Failed, retrying...");
    delay(1000);
    if(++sdRetries >= 5) {
      Serial.println("SD Card Mount Failed after multiple attempts. Rebooting...");
      ESP.restart();
    }
  }
  Serial.println("SD Card Initialized");
  
  // Create a startup marker file
  File startupFile = SD_MMC.open("/startup.txt", FILE_WRITE);
  if(startupFile) {
    time_t now;
    time(&now);
    startupFile.printf("Camera started at: %s", ctime(&now));
    startupFile.printf("Firmware compiled: %s %s\n", __DATE__, __TIME__);
    startupFile.close();
    Serial.println("Created startup marker file");
  }


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
  // Available resolutions for OV2640:
  // FRAMESIZE_UXGA (1600x1200) - Highest quality, slower
  // FRAMESIZE_SXGA (1280x1024)
  // FRAMESIZE_XGA (1024x768)
  // FRAMESIZE_SVGA (800x600) - Good balance of quality and performance
  // FRAMESIZE_VGA (640x480) - Lower quality, faster
  config.frame_size = FRAMESIZE_SVGA; // 800x600 resolution
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

  // Set vertical flip based on build flag
  s->set_vflip(s, VERTICAL_FLIP == 1);
  Serial.printf("Vertical flip: %s\n", (VERTICAL_FLIP == 1) ? "Enabled" : "Disabled");

  if (s->id.PID == OV3660_PID) {
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  // Connect to WiFi with timeout
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  unsigned long wifiStartTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    esp_task_wdt_reset(); // Reset watchdog while waiting
    
    // If WiFi connection takes too long, continue anyway
    if (millis() - wifiStartTime > 20000) {
      Serial.println("\nWiFi connection timeout. Continuing without WiFi.");
      break;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    // Sync time with NTP
    setupTimeViaNTP();
    
    // Start camera web server only if WiFi is connected
    startCameraServer();
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
  } else {
    Serial.println("\nOperating without WiFi connection");
  }
}


void setupTimeViaNTP() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Wait until time is set, but with a timeout
  time_t now = 0;
  unsigned long ntpStartTime = millis();
  while (now < 1672531200) { // some date in 2023 or later
    delay(500);
    Serial.print(".");
    time(&now);
    esp_task_wdt_reset(); // Reset watchdog while waiting
    
    // If NTP sync takes too long, continue anyway
    if (millis() - ntpStartTime > 10000) {
      Serial.println("\nNTP sync timeout. Using system time.");
      break;
    }
  }
  
  if (now > 1672531200) {
    Serial.println("\nTime is set via NTP!");
  } else {
    Serial.println("\nUsing system time without NTP sync");
  }
}

// 5-second timelapse interval
static const unsigned long TIMELAPSE_INTERVAL_MS = 40000;
static unsigned long lastTimelapse = 0;

void captureAndSaveTimelapse() {
    bool success = false;
    esp_task_wdt_reset(); // Reset watchdog before capture
    
    // 1) Grab a frame
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Timelapse capture failed");
        // If camera fails, try to reset it
        esp_camera_deinit();
        delay(100);
        
        // Reconfigure and reinitialize the camera
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
        config.frame_size = FRAMESIZE_VGA;
        config.pixel_format = PIXFORMAT_RGB565; 
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        
        esp_err_t err = esp_camera_init(&config);
        if (err != ESP_OK) {
            Serial.printf("Camera reinit failed with error 0x%x, will reboot\n", err);
            File errorLog = SD_MMC.open("/camera_errors.txt", FILE_APPEND);
            if (errorLog) {
                time_t now;
                time(&now);
                errorLog.printf("Camera error at %s: 0x%x\n", ctime(&now), err);
                errorLog.close();
            }
            delay(1000);
            ESP.restart();
        }
        
        // Try to get frame again
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Second capture attempt failed, logging error");
            File errorLog = SD_MMC.open("/camera_errors.txt", FILE_APPEND);
            if (errorLog) {
                time_t now;
                time(&now);
                errorLog.printf("Persistent camera failure at %s\n", ctime(&now));
                errorLog.close();
            }
            return;
        }
    }

    // 2) Convert to JPEG if needed
    size_t out_len = 0;
    uint8_t *out_buf = NULL;

    if (fb->format == PIXFORMAT_JPEG) {
        // Already JPEG
        out_buf = fb->buf;
        out_len = fb->len;
    } else {
        // Convert from e.g. RGB565 to JPEG
        bool converted = frame2jpg(fb, 80, &out_buf, &out_len);
        if (!converted) {
            esp_camera_fb_return(fb);
            Serial.println("JPEG conversion failed");
            return;
        }
    }

    // 3) Build a unique filename
    // Use real NTP-based timestamps, if your device has internet & you have set time.
    
    // Now get the real time
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char filename[64];
    strftime(filename, sizeof(filename), "/%Y-%m-%d_%H-%M-%S.jpg", &timeinfo);

    // 4) Write file
    File f = SD_MMC.open(filename, FILE_WRITE);
    if (!f) {
        Serial.printf("Failed to open file for writing: %s\n", filename);
        
        // Try to remount SD card
        SD_MMC.end();
        delay(500);
        if (!SD_MMC.begin("/sdcard", true)) {
            Serial.println("SD remount failed after write error");
            File errorLog = SD_MMC.open("/sd_errors.txt", FILE_APPEND);
            if (errorLog) {
                errorLog.printf("SD write error at %s\n", ctime(&now));
                errorLog.close();
            }
        } else {
            // Try again after remount
            f = SD_MMC.open(filename, FILE_WRITE);
            if (!f) {
                Serial.println("Still can't write to SD after remount");
            }
        }
    }
    
    if (f) {
        f.write(out_buf, out_len);
        f.close();
        success = true;
        photosCount++;
        Serial.printf("Timelapse saved: %s (%u bytes)\n", filename, (unsigned)out_len);
        
        // Also print uptime and stats every 10 photos
        if (photosCount % 10 == 0) {
            unsigned long uptime = millis() / 1000; // seconds
            Serial.printf("System uptime: %u days, %u hours, %u minutes, %u seconds\n", 
                        uptime / 86400, (uptime % 86400) / 3600, 
                        (uptime % 3600) / 60, uptime % 60);
            Serial.printf("Photos taken since boot: %u\n", photosCount);
        }
    }

    // 5) Cleanup
    if (fb->format == PIXFORMAT_JPEG) {
        // Return frame buffer to driver
        esp_camera_fb_return(fb);
    } else {
        // We allocated our own out_buf
        free(out_buf);
        // Also return the original frame buffer
        esp_camera_fb_return(fb);
    }
}



// Check and reconnect WiFi if needed
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost, attempting to reconnect...");
    WiFi.disconnect();
    delay(1000);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Wait up to 10 seconds for reconnection
    unsigned long reconnectStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - reconnectStart < 10000) {
      delay(500);
      Serial.print(".");
      esp_task_wdt_reset(); // Reset watchdog while waiting
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi reconnected");
      
      // Re-sync time
      setupTimeViaNTP();
    } else {
      Serial.println("\nFailed to reconnect WiFi, will try again later");
    }
  }
}

// Update heartbeat file to track last successful operation
void updateHeartbeat() {
  File heartbeat = SD_MMC.open("/heartbeat.txt", FILE_WRITE);
  if (heartbeat) {
    time_t now;
    time(&now);
    
    heartbeat.printf("Last heartbeat: %s", ctime(&now));
    heartbeat.printf("Uptime: %lu seconds\n", millis() / 1000);
    heartbeat.printf("Photos taken: %lu\n", photosCount);
    heartbeat.printf("WiFi status: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    
    heartbeat.close();
  }
}

void loop() {
  // Reset watchdog timer in main loop
  esp_task_wdt_reset();

  // Your timelapse function at regular intervals
  if (millis() - lastTimelapse >= TIMELAPSE_INTERVAL_MS) {
    lastTimelapse = millis();
    captureAndSaveTimelapse();
  }
  
  // Check WiFi connection periodically
  if (millis() - lastWifiCheck >= WIFI_RECONNECT_INTERVAL) {
    lastWifiCheck = millis();
    checkWiFiConnection();
  }
  
  // Update heartbeat file periodically
  if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = millis();
    updateHeartbeat();
  }
  
  // Perform a scheduled reset to prevent memory issues
  if (millis() > AUTO_RESET_INTERVAL) {
    Serial.println("Performing scheduled reset after 24 hours of operation");
    
    // Log the planned reset
    File resetLog = SD_MMC.open("/resets.txt", FILE_APPEND);
    if (resetLog) {
      time_t now;
      time(&now);
      resetLog.printf("Planned reset at %s after %lu seconds of operation\n", 
                    ctime(&now), millis() / 1000);
      resetLog.printf("Photos taken: %lu\n", photosCount);
      resetLog.close();
    }
    
    // Small delay to ensure file is written
    delay(1000);
    ESP.restart();
  }
}
