/**
 * @file lvgl_display.cpp
 * @brief LVGL display driver implementation for GC9A01A using Adafruit library
 */

#include "lvgl_display.h"
#include <SPI.h>

/* Static variables */
static Adafruit_GC9A01A *tft_ptr = nullptr;
static lv_display_t *lvgl_display = nullptr;

/* Draw buffer - 2 buffers for double buffering */
static lv_color_t draw_buf1[LVGL_BUFFER_SIZE];
static lv_color_t draw_buf2[LVGL_BUFFER_SIZE];

/**
 * @brief Flush callback for LVGL
 * Called when LVGL wants to update a region of the display
 */
static void lvgl_display_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    static int flush_count = 0;
    
    if (tft_ptr == nullptr) {
        Serial.println("[Display] ERROR: tft_ptr is null in flush callback!");
        lv_display_flush_ready(disp);
        return;
    }

    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    if (flush_count < 5) {
        Serial.printf("[Display] Flush callback #%d: x1=%d, y1=%d, w=%d, h=%d\n", 
                      flush_count, area->x1, area->y1, w, h);
        flush_count++;
    }

    /* Start transaction */
    tft_ptr->startWrite();
    
    /* Set window and push pixels */
    tft_ptr->setAddrWindow(area->x1, area->y1, w, h);
    tft_ptr->writePixels((uint16_t *)px_map, w * h);
    
    /* End transaction */
    tft_ptr->endWrite();

    /* Tell LVGL we're done */
    lv_display_flush_ready(disp);
}

/**
 * @brief Initialize LVGL display driver
 */
void lvgl_display_init(Adafruit_GC9A01A *tft) {
    tft_ptr = tft;

    /* Turn on backlight */
    #ifdef TFT_BL_PIN
        pinMode(TFT_BL_PIN, OUTPUT);
        digitalWrite(TFT_BL_PIN, HIGH);
        Serial.printf("[Display] Backlight enabled on GPIO %d\n", TFT_BL_PIN);
    #else
        Serial.println("[Display] WARNING: TFT_BL_PIN not defined!");
    #endif

    /* Initialize TFT */
    Serial.println("[Display] Calling tft->begin()...");
    Serial.flush();
    
    tft->begin();
    Serial.println("[Display] TFT begin() complete");
    Serial.flush();
    
    tft->setRotation(0);  // Portrait mode
    Serial.println("[Display] Rotation set");
    Serial.flush();
    
    /* Test the display with direct drawing */
    Serial.println("[Display] Testing fillScreen...");
    Serial.flush();
    tft->fillScreen(0x0000);  // Black
    delay(100);
    
    Serial.println("[Display] Drawing test circle...");
    Serial.flush();
    tft->fillCircle(120, 120, 50, 0xF800);  // Red circle
    delay(1000);  // Show for 1 second
    
    Serial.println("[Display] Clearing screen...");
    Serial.flush();
    tft->fillScreen(0x0000);  // Black
    
    Serial.println("[Display] TFT test complete, initializing LVGL...");
    Serial.flush();

    /* Initialize LVGL */
    lv_init();
    Serial.println("[Display] LVGL lv_init() complete");
    Serial.flush();

    /* Create display */
    lvgl_display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    Serial.println("[Display] LVGL display created");
    Serial.flush();
    
    /* Set display buffers - use 2 buffers for smooth rendering */
    lv_display_set_buffers(lvgl_display, draw_buf1, draw_buf2, sizeof(draw_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.println("[Display] LVGL buffers set");
    Serial.flush();
    
    /* Set flush callback */
    lv_display_set_flush_cb(lvgl_display, lvgl_display_flush_cb);
    Serial.println("[Display] LVGL flush callback set");
    Serial.flush();

    Serial.println("[Display] LVGL display initialized");
    Serial.printf("[Display] Resolution: %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    Serial.printf("[Display] Buffer size: %d pixels per buffer\n", LVGL_BUFFER_SIZE);
}

/**
 * @brief Get LVGL display object
 */
lv_display_t* lvgl_get_display() {
    return lvgl_display;
}
