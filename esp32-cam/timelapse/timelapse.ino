#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"

#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"

#define PICTURE_DELAY  3        // Delay for picture taking (in seconds)
#define STARTUP_DELAY 30        // Optional Time ESP32 will sleep on first boot in seconds
#define uS_TO_S_FACTOR (1000000ULL)  // Conversion factor for micro seconds to seconds

#ifdef STARTUP_DELAY 
  const uint64_t totalTimeSleep = uS_TO_S_FACTOR * STARTUP_DELAY; // Avoid integer overflow using unsigned 64 bit int.
#else
  const uint64_t totalTimeSleep = 0; // Skip through the initial sleep and continue with regular code execution
#endif

#define LED_1             33 // Built-in back LED

RTC_DATA_ATTR uint32_t bootCount = 0; // Times restarted

void setup() {
  Serial.begin(115200);
  Serial.println("Startup");
  
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  if(!bootCount){ // Ensure this runs only on the first boot
    ++bootCount; // First boot
    esp_sleep_enable_timer_wakeup(totalTimeSleep); // Init timer
    
    // Flash built-in LED 3 times indicating startup
    pinMode(LED_1, OUTPUT);
    for(byte i = 0; i < 3; i++){
      digitalWrite(LED_1, HIGH); delay(200);
      digitalWrite(LED_1, LOW); delay(200);
    }
    
    // Go to sleep
    Serial.printf("Going to sleep for %d seconds\n", STARTUP_DELAY);
    esp_deep_sleep_start();
  } else {
    rtc_gpio_hold_dis(GPIO_NUM_4); // Re-enable GPIO 4 for SD Card
  }
  
  // Flash once when waking up from deep sleep
  if(bootCount == 1){
    pinMode(LED_1, OUTPUT);
    digitalWrite(LED_1, HIGH); delay(1000);
    digitalWrite(LED_1, LOW);
  }
  
  // Setup camera pins using camera_pins.h definitions
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
  config.pin_sccb_sda = SIOD_GPIO_NUM; // Corrected pin name
  config.pin_sccb_scl = SIOC_GPIO_NUM; // Corrected pin name
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_VGA; // Reduced frame size to match known good config
  config.pixel_format = PIXFORMAT_RGB565; 
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1; // Reduced buffer count to match known good config
  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {Serial.printf("Camera init failed with error 0x%x", err);return;}
  
  // Init SD Card
  if(!SD_MMC.begin()){Serial.println("SD Card Mount Failed");return;}
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){Serial.println("No SD Card attached");return;}
  
  // Take Picture with Camera
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {Serial.println("Camera capture failed");return;}
  
  // Change picture number/name as each boot count increases
  uint32_t pictureNumber = bootCount++;

  // Path where new picture will be saved in SD Card
  String path = "/picture" + String(pictureNumber) +".jpg";
  fs::FS &fs = SD_MMC; 
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file) Serial.println("Failed to open file in writing mode");
  else file.write(fb->buf, fb->len); 
  file.close();
  
  // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  rtc_gpio_hold_en(GPIO_NUM_4); //Latch value when going into deep sleep
  esp_sleep_enable_timer_wakeup(PICTURE_DELAY * uS_TO_S_FACTOR);
  esp_deep_sleep_start(); //Restart cam
}

void loop() {
  //Should never loop
  delay(1000);
}
