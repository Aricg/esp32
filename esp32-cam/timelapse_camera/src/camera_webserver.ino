#include "esp_camera.h"
#include <WiFi.h>
#include "esp_system.h"
#include "FS.h"
#include "SD_MMC.h"
#include <time.h>
#include "esp_task_wdt.h"  // For watchdog timer
#include "esp_http_server.h" // For httpd_handle_t and httpd_stop
#include "esp_sleep.h"     // For light sleep

#ifndef VERTICAL_FLIP
#define VERTICAL_FLIP 0  // Default to false if not defined
#endif

// =======================================================================
// Camera Model Selection
// Camera Model Selection is now controlled by -DCAMERA_MODEL in platformio.ini
// =======================================================================

// Define numeric values for selection mapping
#define _MODEL_SELECT_AI_THINKER 1
#define _MODEL_SELECT_GENERIC_OV2640 2

// CAMERA_MODEL is expected to be defined in platformio.ini build_flags (e.g., -DCAMERA_MODEL=1)
#ifndef CAMERA_MODEL
  #error "CAMERA_MODEL is not defined in platformio.ini build_flags. Set to 1 (AI_THINKER) or 2 (GENERIC_OV2640)."
#endif

// Define the appropriate model macro for camera_pins.h and for specific logic.
// Both AI_THINKER and our GENERIC_OV2640 configuration use AI_THINKER pins.
#if CAMERA_MODEL == _MODEL_SELECT_AI_THINKER
  #define CAMERA_MODEL_AI_THINKER // Define this for camera_pins.h
  #pragma message "Compiling for AI_THINKER camera model (selected via platformio.ini)"
#elif CAMERA_MODEL == _MODEL_SELECT_GENERIC_OV2640
  #define CAMERA_MODEL_AI_THINKER // GENERIC_OV2640 uses AI_THINKER pins
  #pragma message "Compiling for GENERIC_OV2640 camera model (selected via platformio.ini, using AI_THINKER pins)"
#else
  #error "Unknown CAMERA_MODEL value in platformio.ini. Must be 1 or 2."
#endif

#include "camera_pins.h"
// Note: If CAMERA_MODEL_GENERIC_OV2640 requires different pins than AI_THINKER,
// you would need to create a separate "camera_pins_ov2640.h" (or similar)
// and adjust the #include logic here, for example:
// #if CAMERA_MODEL == CAMERA_MODEL_GENERIC_OV2640
//   #include "camera_pins_ov2640.h"
// #else
//   #include "camera_pins.h" // For AI_THINKER and others
// #endif


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
#define FOCUS_MODE_DURATION_MS (30 * 1000) // 30 seconds for focus mode

// Global camera configuration
camera_config_t global_cam_config;

// Global variables for reliability and focus mode
unsigned long lastWifiCheck = 0;
unsigned long lastHeartbeat = 0;
unsigned long startTime = 0;
unsigned long photosCount = 0;
bool focusModeActive = true; // Start in focus mode
unsigned long focusModeEndTime = 0;

// HTTP server handles (defined in app_httpd.cpp)
extern httpd_handle_t camera_httpd;
extern httpd_handle_t stream_httpd;

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

  // camera_config_t config; // Use global_cam_config now
  global_cam_config.ledc_channel = LEDC_CHANNEL_0;
  global_cam_config.ledc_timer = LEDC_TIMER_0;
  global_cam_config.pin_d0 = Y2_GPIO_NUM;
  global_cam_config.pin_d1 = Y3_GPIO_NUM;
  global_cam_config.pin_d2 = Y4_GPIO_NUM;
  global_cam_config.pin_d3 = Y5_GPIO_NUM;
  global_cam_config.pin_d4 = Y6_GPIO_NUM;
  global_cam_config.pin_d5 = Y7_GPIO_NUM;
  global_cam_config.pin_d6 = Y8_GPIO_NUM;
  global_cam_config.pin_d7 = Y9_GPIO_NUM;
  global_cam_config.pin_xclk = XCLK_GPIO_NUM;
  global_cam_config.pin_pclk = PCLK_GPIO_NUM;
  global_cam_config.pin_vsync = VSYNC_GPIO_NUM;
  global_cam_config.pin_href = HREF_GPIO_NUM;
  global_cam_config.pin_sccb_sda = SIOD_GPIO_NUM;
  global_cam_config.pin_sccb_scl = SIOC_GPIO_NUM;
  global_cam_config.pin_pwdn = PWDN_GPIO_NUM;
  global_cam_config.pin_reset = RESET_GPIO_NUM;
  global_cam_config.xclk_freq_hz = 20000000;
  // Available resolutions for OV2640:
  // FRAMESIZE_UXGA (1600x1200) - Highest quality, slower
  // FRAMESIZE_SXGA (1280x1024)
  // FRAMESIZE_XGA (1024x768)
  // FRAMESIZE_SVGA (800x600) - Good balance of quality and performance
  // FRAMESIZE_VGA (640x480) - Lower quality, faster
  global_cam_config.frame_size = FRAMESIZE_SVGA; // 800x600 resolution
  global_cam_config.pixel_format = PIXFORMAT_JPEG; // Set pixel format to JPEG for OV5640
  global_cam_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  global_cam_config.fb_location = CAMERA_FB_IN_PSRAM;
  global_cam_config.jpeg_quality = 12; // Lower number means higher quality (0-63)
  global_cam_config.fb_count = 1;

  // Debugging: Initialize camera and print errors
  esp_err_t err = esp_camera_init(&global_cam_config);
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

  // Camera remains initialized here for the web server during focus mode.
  // It will be de-initialized when focus mode ends or if WiFi fails.

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
    focusModeActive = true; // Confirm focus mode is active
  } else {
    Serial.println("\nOperating without WiFi connection. Focus mode disabled.");
    focusModeActive = false;
    // De-initialize camera as web server won't start and focus mode is off
    Serial.println("De-initializing camera as WiFi connection failed.");
    esp_camera_deinit();
  }
  focusModeEndTime = millis() + FOCUS_MODE_DURATION_MS;
  Serial.printf("Focus mode will be active for %lu minutes.\n", FOCUS_MODE_DURATION_MS / (60 * 1000));
}


void stopWebServerAndWiFi() {
    Serial.println("Stopping web server...");
    if (camera_httpd) {
        httpd_stop(camera_httpd);
        camera_httpd = NULL; // Mark as stopped
    }
    if (stream_httpd) {
        httpd_stop(stream_httpd);
        stream_httpd = NULL; // Mark as stopped
    }
    Serial.println("Web server stopped.");

    Serial.println("Turning off WiFi...");
    WiFi.disconnect(true); // Disconnect from the network
    WiFi.mode(WIFI_OFF);   // Turn off WiFi radio
    Serial.println("WiFi turned off.");
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
    esp_task_wdt_reset(); // Reset watchdog

#if CAMERA_MODEL == _MODEL_SELECT_GENERIC_OV2640
    Serial.println("Timelapse (OV2640 specific logic): Adding pre-init delay...");
    delay(500); // Add a delay for OV2640 to stabilize, may help with init after sleep
#endif

    Serial.println("Timelapse: Initializing camera...");
    esp_err_t init_err = esp_camera_init(&global_cam_config);
    if (init_err != ESP_OK) {
        Serial.printf("Timelapse: Camera init failed with error 0x%x\n", init_err);
        File errorLog = SD_MMC.open("/camera_errors.txt", FILE_APPEND);
        if (errorLog) {
            time_t now_log;
            time(&now_log);
            errorLog.printf("Timelapse: Camera init error at %s: 0x%x\n", ctime(&now_log), init_err);
            errorLog.close();
        }
        return;
    }
    Serial.println("Timelapse: Camera initialized successfully.");

    // Re-apply sensor settings as they might be reset after deinit/init
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        Serial.println("Timelapse: Failed to get camera sensor after init.");
        File errorLog = SD_MMC.open("/camera_errors.txt", FILE_APPEND);
        if (errorLog) {
            time_t now_log;
            time(&now_log);
            errorLog.printf("Timelapse: Failed to get sensor at %s\n", ctime(&now_log));
            errorLog.close();
        }
        esp_camera_deinit(); // Deinit if sensor get fails
        Serial.println("Timelapse: De-initialized camera due to sensor get failure.");
        return;
    }
    s->set_vflip(s, VERTICAL_FLIP == 1);
    if (s->id.PID == OV3660_PID) { // Re-apply specific sensor settings
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
    }
    Serial.println("Timelapse: Sensor settings re-applied.");

    // Allow AWB (Auto White Balance) and AEC (Auto Exposure Control) to stabilize
    Serial.println("Timelapse: Allowing AWB/AEC to stabilize...");
    for (int i = 0; i < 3; i++) { // Discard 3 frames
        camera_fb_t *stab_fb = esp_camera_fb_get();
        if (!stab_fb) {
            Serial.println("Timelapse: AWB/AEC stabilization frame capture failed.");
            // If stabilization fails, we might still try to capture the main frame,
            // or we could deinit and return here. For now, let's just log and continue.
            break; 
        }
        esp_camera_fb_return(stab_fb); // Return frame to free buffer
        delay(100); // Small delay to allow processing
        esp_task_wdt_reset(); // Reset watchdog during stabilization
    }
    Serial.println("Timelapse: AWB/AEC stabilization complete.");

    bool success = false;
    esp_task_wdt_reset(); // Reset watchdog before capture
    
    // 1) Grab a frame
    Serial.println("Timelapse: Attempting to capture frame...");
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Timelapse: Frame capture failed (esp_camera_fb_get returned NULL).");
        File errorLog = SD_MMC.open("/camera_errors.txt", FILE_APPEND);
        if (errorLog) {
            time_t now_log;
            time(&now_log);
            errorLog.printf("Timelapse: Frame capture failed at %s\n", ctime(&now_log));
            errorLog.close();
        }
        esp_camera_deinit(); // De-initialize camera
        Serial.println("Timelapse: De-initialized camera due to frame capture failure.");
        return; // Exit function
    }
    Serial.println("Timelapse: Frame captured successfully.");

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
            esp_camera_deinit(); // De-initialize camera
            Serial.println("Timelapse: De-initialized camera due to JPEG conversion failure.");
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

    Serial.println("Timelapse: De-initializing camera after successful capture and save.");
    esp_camera_deinit();
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

  // Manage focus mode
  if (focusModeActive && millis() >= focusModeEndTime) {
    Serial.println("Focus mode duration elapsed.");
    stopWebServerAndWiFi();
    Serial.println("De-initializing camera after focus mode.");
    esp_camera_deinit(); // De-initialize camera as web server is no longer needed
    focusModeActive = false;
    // Ensure lastTimelapse is set so the first sleep cycle after focus mode calculates correctly
    // or the first timelapse capture occurs promptly.
  }

  // Timelapse and Power Saving Logic (only when not in focus mode)
  if (!focusModeActive) {
    unsigned long current_millis = millis();
    if (current_millis - lastTimelapse >= TIMELAPSE_INTERVAL_MS) {
      // Time for timelapse
      lastTimelapse = current_millis; // Update timestamp before capture
      captureAndSaveTimelapse();
    } else {
      // Not time for timelapse yet, consider sleeping
      unsigned long time_to_next_capture = (lastTimelapse + TIMELAPSE_INTERVAL_MS) - current_millis;
      
      // Sleep for at most (WDT_TIMEOUT_SECONDS - 5 seconds), or time_to_next_capture, whichever is smaller.
      // This ensures WDT is reset before it expires.
      // Subtract a safety margin (e.g., 5 seconds) from WDT_TIMEOUT_SECONDS.
      unsigned long max_safe_sleep_ms = (unsigned long)(WDT_TIMEOUT_SECONDS > 5 ? WDT_TIMEOUT_SECONDS - 5 : WDT_TIMEOUT_SECONDS / 2) * 1000;
      if (max_safe_sleep_ms == 0 && WDT_TIMEOUT_SECONDS > 0) max_safe_sleep_ms = WDT_TIMEOUT_SECONDS * 500; // 50% of WDT if too short
      if (max_safe_sleep_ms == 0) max_safe_sleep_ms = 1000; // Default to 1s if WDT is 0 or extremely short


      unsigned long sleep_duration_ms = min(time_to_next_capture, max_safe_sleep_ms);

      if (sleep_duration_ms > 1000) { // Only sleep if duration is meaningful (e.g., > 1 sec)
        Serial.printf("Light sleeping for %lu ms...\n", sleep_duration_ms);
        esp_sleep_enable_timer_wakeup(sleep_duration_ms * 1000); // us
        esp_light_sleep_start();
        // Execution resumes from top of loop() after waking up, WDT will be reset.
        // No need to manually reset WDT here as it's done at the start of loop().
      }
      // If sleep_duration_ms is too short, just loop normally (effectively a short busy wait).
      // This also allows WDT to be reset by the loop() iteration.
    }
  }
  // End Timelapse and Power Saving Logic
  
  // Check WiFi connection periodically only if in focus mode
  if (focusModeActive && millis() - lastWifiCheck >= WIFI_RECONNECT_INTERVAL) {
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
