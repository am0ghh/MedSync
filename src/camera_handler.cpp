#include <Arduino.h>
#include "esp_camera.h"
#include "camera_handler.h"
#include "supabase_client.h"
#include "config.h"

bool cameraInit() {
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = CAM_PIN_Y2;
  cfg.pin_d1       = CAM_PIN_Y3;
  cfg.pin_d2       = CAM_PIN_Y4;
  cfg.pin_d3       = CAM_PIN_Y5;
  cfg.pin_d4       = CAM_PIN_Y6;
  cfg.pin_d5       = CAM_PIN_Y7;
  cfg.pin_d6       = CAM_PIN_Y8;
  cfg.pin_d7       = CAM_PIN_Y9;
  cfg.pin_xclk     = CAM_PIN_XCLK;
  cfg.pin_pclk     = CAM_PIN_PCLK;
  cfg.pin_vsync    = CAM_PIN_VSYNC;
  cfg.pin_href     = CAM_PIN_HREF;
  cfg.pin_sccb_sda = CAM_PIN_SIOD;
  cfg.pin_sccb_scl = CAM_PIN_SIOC;
  cfg.pin_pwdn     = CAM_PIN_PWDN;
  cfg.pin_reset    = CAM_PIN_RESET;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size   = FRAMESIZE_QVGA; // 320×240 — reliable and fast to upload
  cfg.jpeg_quality = 12;             // 0=best quality, 63=worst
  cfg.fb_count     = 1;
  cfg.fb_location  = CAMERA_FB_IN_DRAM;
  cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

  Serial.println("[camera] initialising...");
  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("[camera] init failed: %s\n", esp_err_to_name(err));
    return false;
  }

  // Warm up the sensor — first couple of frames are often underexposed.
  for (int i = 0; i < 3; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    delay(100);
  }

  Serial.println("[camera] ready");
  return true;
}

String cameraCaptureAndUpload(const String& filename) {
  // The onboard NeoPixel is used as a fill light during capture.
  // alerts.cpp owns the pixel; the caller is responsible for turning it on/off.
  // (We flash white in main.cpp just before calling this.)

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[camera] frame capture failed");
    return "";
  }

  Serial.printf("[camera] captured %u bytes, uploading as %s\n",
                fb->len, filename.c_str());

  String url = supabaseUploadImage(fb->buf, fb->len, filename);
  esp_camera_fb_return(fb);
  return url;
}
