#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "mdns.h"

// Wi-Fi 認証情報ファイル読み込み (Git管理外)
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

static const char *TAG = "ESP32Clock";

// Web API 設定 (SwitchBot + sensor ボード)
#define SENSOR_API_URL "http://esp32-switchbot.local/api/sensor"

// LCD ピンアサイン (ボード仕様)
#define LCD_HOST       SPI2_HOST
#define PIN_NUM_SCLK   18
#define PIN_NUM_MOSI   23
#define PIN_NUM_MISO   -1
#define PIN_NUM_LCD_DC 2
#define PIN_NUM_LCD_RST 4
#define PIN_NUM_LCD_CS 15
#define PIN_NUM_BK_LIGHT 32

// LCD 解像度 (横画面)
#define LCD_H_RES      320
#define LCD_V_RES      170

// カラー定義 (RGB565)
#define RGB565(r, g, b) (((((r) >> 3) & 0x1F) << 11) | ((((g) >> 2) & 0x3F) << 5) | (((b) >> 3) & 0x1F))
#define SWAP16(v) ((((v) & 0xFF) << 8) | (((v) >> 8) & 0xFF))

#define COLOR_BG        SWAP16(RGB565(10, 14, 24))     // 背景ダークネイビー
#define COLOR_CARD_BG   SWAP16(RGB565(18, 24, 38))     // カード背景
#define COLOR_BORDER    SWAP16(RGB565(36, 48, 70))     // 枠線
#define COLOR_WHITE     SWAP16(0xFFFF)
#define COLOR_CYAN      SWAP16(RGB565(0, 230, 230))    // アクセント水色
#define COLOR_ORANGE    SWAP16(RGB565(255, 150, 25))   // 温度用オレンジ
#define COLOR_YELLOW    SWAP16(RGB565(255, 215, 45))   // 照度用イエロー
#define COLOR_GRAY      SWAP16(RGB565(135, 150, 170))  // サブテキストグレー

static esp_lcd_panel_handle_t panel_handle = NULL;
static uint16_t *frame_buffer = NULL;

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// センサーデータ保持用
static portMUX_TYPE sensor_mux = portMUX_INITIALIZER_UNLOCKED;
static float g_sensor_temp = 0.0f;
static float g_sensor_lux  = 0.0f;
static bool  g_sensor_valid = false;

// =========================================================================
// 8x16 ビットマップフォントテーブル (統一フォント)
// =========================================================================
static const uint8_t font_table[][16] = {
    // 0: ' '
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 1: '.'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00},
    // 2: '/'
    {0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 3: ':'
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00},
    // 4: '-'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 5-14: '0'-'9'
    {0x00,0x3C,0x66,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x66,0x3C,0x00,0x00,0x00}, // 0
    {0x00,0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00}, // 1
    {0x00,0x3C,0x66,0x06,0x06,0x06,0x0C,0x18,0x30,0x60,0x40,0x7E,0x7E,0x00,0x00,0x00}, // 2
    {0x00,0x3C,0x66,0x06,0x06,0x1C,0x06,0x06,0x06,0x06,0x46,0x66,0x3C,0x00,0x00,0x00}, // 3
    {0x00,0x0C,0x1C,0x2C,0x4C,0x4C,0x8C,0x7E,0x0C,0x0C,0x0C,0x0C,0x1E,0x00,0x00,0x00}, // 4
    {0x00,0x7E,0x40,0x40,0x40,0x5C,0x66,0x02,0x02,0x02,0x46,0x66,0x3C,0x00,0x00,0x00}, // 5
    {0x00,0x3C,0x66,0x40,0x40,0x5C,0x66,0x42,0x42,0x42,0x66,0x66,0x3C,0x00,0x00,0x00}, // 6
    {0x00,0x7E,0x06,0x06,0x06,0x0C,0x0C,0x18,0x18,0x30,0x30,0x30,0x30,0x00,0x00,0x00}, // 7
    {0x00,0x3C,0x66,0x42,0x42,0x66,0x3C,0x66,0x42,0x42,0x42,0x66,0x3C,0x00,0x00,0x00}, // 8
    {0x00,0x3C,0x66,0x42,0x42,0x42,0x66,0x3A,0x02,0x02,0x02,0x66,0x3C,0x00,0x00,0x00}, // 9
    // 15-40: 'A'-'Z'
    {0x00,0x18,0x3C,0x66,0x42,0x42,0x7E,0x42,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00}, // A (15)
    {0x00,0x7C,0x42,0x42,0x42,0x7C,0x42,0x42,0x42,0x42,0x42,0x7C,0x00,0x00,0x00,0x00}, // B (16)
    {0x00,0x3C,0x66,0x42,0x40,0x40,0x40,0x40,0x40,0x42,0x66,0x3C,0x00,0x00,0x00,0x00}, // C (17)
    {0x00,0x78,0x44,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x44,0x78,0x70,0x00,0x00,0x00}, // D (18)
    {0x00,0x7E,0x40,0x40,0x40,0x7C,0x40,0x40,0x40,0x40,0x40,0x7E,0x7E,0x00,0x00,0x00}, // E (19)
    {0x00,0x7E,0x40,0x40,0x40,0x7C,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x00,0x00,0x00}, // F (20)
    {0x00,0x3C,0x66,0x42,0x40,0x40,0x40,0x4E,0x42,0x42,0x66,0x3A,0x00,0x00,0x00,0x00}, // G (21)
    {0x00,0x42,0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00}, // H (22)
    {0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00}, // I (23)
    {0x00,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x46,0x46,0x6C,0x38,0x00,0x00,0x00}, // J (24)
    {0x00,0x42,0x44,0x48,0x50,0x60,0x50,0x48,0x44,0x42,0x42,0x42,0x42,0x00,0x00,0x00}, // K (25)
    {0x00,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x7E,0x00,0x00,0x00}, // L (26)
    {0x00,0x42,0x66,0x7E,0x5A,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00}, // M (27)
    {0x00,0x42,0x62,0x72,0x52,0x4A,0x46,0x46,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00}, // N (28)
    {0x00,0x3C,0x66,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x66,0x3C,0x00,0x00,0x00}, // O (29)
    {0x00,0x7C,0x42,0x42,0x42,0x7C,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x00,0x00,0x00}, // P (30)
    {0x00,0x3C,0x66,0x42,0x42,0x42,0x42,0x42,0x4A,0x46,0x66,0x3A,0x0E,0x00,0x00,0x00}, // Q (31)
    {0x00,0x7C,0x42,0x42,0x42,0x7C,0x48,0x44,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00}, // R (32)
    {0x00,0x3C,0x66,0x40,0x40,0x38,0x0C,0x06,0x02,0x02,0x46,0x66,0x3C,0x00,0x00,0x00}, // S (33)
    {0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00}, // T (34)
    {0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x66,0x3C,0x00,0x00,0x00}, // U (35)
    {0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x24,0x24,0x18,0x18,0x18,0x00,0x00,0x00}, // V (36)
    {0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x5A,0x5A,0x7E,0x66,0x42,0x42,0x00,0x00,0x00}, // W (37)
    {0x00,0x42,0x66,0x24,0x18,0x18,0x18,0x24,0x66,0x42,0x42,0x00,0x00,0x00,0x00,0x00}, // X (38)
    {0x00,0x42,0x42,0x42,0x24,0x24,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00}, // Y (39)
    {0x00,0x7E,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x40,0x20,0x7E,0x7E,0x00,0x00,0x00}, // Z (40)
    // 41: 'l'
    {0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x1C,0x00,0x00,0x00,0x00},
    // 42: 'x'
    {0x00,0x00,0x00,0x00,0x00,0x42,0x24,0x18,0x18,0x24,0x42,0x00,0x00,0x00,0x00,0x00},
    // 43: '('
    {0x00,0x02,0x04,0x08,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x08,0x04,0x02,0x00,0x00},
    // 44: ')'
    {0x00,0x40,0x20,0x10,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x10,0x20,0x40,0x00,0x00},
};

static const uint8_t* get_font_glyph(char c) {
    if (c == ' ') return font_table[0];
    if (c == '.') return font_table[1];
    if (c == '/') return font_table[2];
    if (c == ':') return font_table[3];
    if (c == '-') return font_table[4];
    if (c >= '0' && c <= '9') return font_table[5 + (c - '0')];
    if (c >= 'A' && c <= 'Z') return font_table[15 + (c - 'A')];
    if (c == 'l') return font_table[41];
    if (c == 'x') return font_table[42];
    if (c == '(') return font_table[43];
    if (c == ')') return font_table[44];
    return font_table[0];
}

// =========================================================================
// 描画プリミティブ
// =========================================================================
static void fill_rect(int x, int y, int w, int h, uint16_t color) {
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= LCD_V_RES) continue;
        for (int i = x; i < x + w; i++) {
            if (i >= 0 && i < LCD_H_RES) {
                frame_buffer[j * LCD_H_RES + i] = color;
            }
        }
    }
}

static void draw_round_rect(int x, int y, int w, int h, int r, uint16_t color) {
    fill_rect(x + r, y, w - 2 * r, 1, color);
    fill_rect(x + r, y + h - 1, w - 2 * r, 1, color);
    fill_rect(x, y + r, 1, h - 2 * r, color);
    fill_rect(x + w - 1, y + r, 1, h - 2 * r, color);
}

// 文字列描画 (拡大スケール対応)
static void draw_string(int x, int y, const char *str, uint16_t color, int scale) {
    int cur_x = x;
    while (*str) {
        const uint8_t *glyph = get_font_glyph(*str);
        for (int row = 0; row < 16; row++) {
            uint8_t b = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (b & (0x80 >> col)) {
                    fill_rect(cur_x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        cur_x += 8 * scale;
        str++;
    }
}

// =========================================================================
// LCD ST7789 初期化
// =========================================================================
static void init_lcd(void) {
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT
    };
    gpio_config(&bk_gpio_config);
    gpio_set_level(PIN_NUM_BK_LIGHT, 1);

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 35));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    frame_buffer = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(frame_buffer != NULL);
}

// =========================================================================
// SwitchBot+sensor Web API 取得タスク (HTTP GET)
// =========================================================================
static void fetch_sensor_data(void) {
    char response_buffer[1024] = {0};
    int total_read = 0;

    esp_http_client_config_t config = {
        .url = SENSOR_API_URL,
        .timeout_ms = 4000,
        .method = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            total_read = esp_http_client_read_response(client, response_buffer, sizeof(response_buffer) - 1);
            if (total_read > 0) {
                response_buffer[total_read] = '\0';
                cJSON *root = cJSON_Parse(response_buffer);
                if (root) {
                    cJSON *item_lux  = cJSON_GetObjectItem(root, "lux");
                    cJSON *item_temp = cJSON_GetObjectItem(root, "temp_celsius");

                    portENTER_CRITICAL(&sensor_mux);
                    if (cJSON_IsNumber(item_lux)) {
                        g_sensor_lux = (float)item_lux->valuedouble;
                    }
                    if (cJSON_IsNumber(item_temp)) {
                        g_sensor_temp = (float)item_temp->valuedouble;
                    }
                    g_sensor_valid = true;
                    portEXIT_CRITICAL(&sensor_mux);

                    ESP_LOGI(TAG, "Sensor update: Temp=%.1f C, Lux=%.1f", g_sensor_temp, g_sensor_lux);
                    cJSON_Delete(root);
                }
            }
        } else {
            ESP_LOGW(TAG, "Sensor API HTTP status: %d", status_code);
        }
    } else {
        ESP_LOGW(TAG, "Failed to connect to Sensor API: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static void sensor_task(void *pvParameters) {
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        fetch_sensor_data();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// =========================================================================
// Wi-Fi & SNTP 初期化
// =========================================================================
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Reconnecting to Wi-Fi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi Connected!");
    }
}

static void init_wifi_sntp(void) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(mdns_init());
    mdns_hostname_set("esp32-clock");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // SNTP 初期化 (JST: UTC+9)
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.nict.jp");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();

    setenv("TZ", "JST-9", 1);
    tzset();
}

// =========================================================================
// メイン画面更新タスク
// =========================================================================
static const char* days_full[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};

static void clock_task(void *pvParameters) {
    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        fill_rect(0, 0, LCD_H_RES, LCD_V_RES, COLOR_BG);
        draw_round_rect(2, 2, 316, 166, 4, COLOR_BORDER);

        if (timeinfo.tm_year > (2020 - 1900)) {
            // -------------------------------------------------------------
            // 1. ヘッダー: 年月日・曜日 (左) ＆ 時計 (右: scale=2)
            // -------------------------------------------------------------
            char date_day_buf[64];
            snprintf(date_day_buf, sizeof(date_day_buf), "%04d/%02d/%02d (%s)",
                     timeinfo.tm_year + 1900,
                     timeinfo.tm_mon + 1,
                     timeinfo.tm_mday,
                     days_full[timeinfo.tm_wday]);
            draw_string(14, 10, date_day_buf, COLOR_GRAY, 1);

            char time_buf[16];
            snprintf(time_buf, sizeof(time_buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            draw_string(230, 4, time_buf, COLOR_CYAN, 2);

            fill_rect(10, 36, 300, 1, COLOR_BORDER);

            // -------------------------------------------------------------
            // 2. メインエリア: 温度を特大表示 (中央カード: scale=4)
            // -------------------------------------------------------------
            float cur_temp = 0.0f;
            float cur_lux  = 0.0f;
            bool  is_valid = false;

            portENTER_CRITICAL(&sensor_mux);
            cur_temp = g_sensor_temp;
            cur_lux  = g_sensor_lux;
            is_valid = g_sensor_valid;
            portEXIT_CRITICAL(&sensor_mux);

            fill_rect(10, 42, 300, 78, COLOR_CARD_BG);
            draw_round_rect(10, 42, 300, 78, 4, COLOR_BORDER);
            draw_string(20, 48, "ROOM TEMP", COLOR_GRAY, 1);

            char temp_main_str[24];
            if (is_valid) {
                snprintf(temp_main_str, sizeof(temp_main_str), "%.1f C", cur_temp);
            } else {
                snprintf(temp_main_str, sizeof(temp_main_str), "--.- C");
            }
            int text_len = strlen(temp_main_str);
            int text_width = text_len * (8 * 4);
            int text_x = 10 + (300 - text_width) / 2;
            draw_string(text_x, 52, temp_main_str, COLOR_ORANGE, 4);

            // -------------------------------------------------------------
            // 3. フッターエリア: 照度 (LUX)
            // -------------------------------------------------------------
            fill_rect(10, 126, 300, 36, COLOR_CARD_BG);
            draw_round_rect(10, 126, 300, 36, 3, COLOR_BORDER);

            draw_string(20, 136, "BRIGHTNESS", COLOR_GRAY, 1);

            char lux_str[32];
            if (is_valid) {
                if (cur_lux >= 1000.0f) {
                    snprintf(lux_str, sizeof(lux_str), "%.0f lx", cur_lux);
                } else {
                    snprintf(lux_str, sizeof(lux_str), "%.1f lx", cur_lux);
                }
            } else {
                snprintf(lux_str, sizeof(lux_str), "--- lx");
            }
            draw_string(170, 132, lux_str, COLOR_YELLOW, 2);

        } else {
            draw_string(60, 75, "CONNECTING & NTP...", COLOR_CYAN, 1);
        }

        esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, frame_buffer);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initializing LCD...");
    init_lcd();

    ESP_LOGI(TAG, "Initializing Wi-Fi & SNTP...");
    init_wifi_sntp();

    xTaskCreatePinnedToCore(sensor_task, "sensor_task", 4096, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(clock_task, "clock_task", 4096, NULL, 5, NULL, 1);
}
