#include "ct_audio.h"
#include "ct_classes.h"
#include "ct_crc32.h"
#include "ct_protocol.h"
#include "ct_store_esp.h"
#include "ct_ui.h"
#include "key_guard.h"

#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define INPUT_QUEUE_DEPTH 12
#define IDLE_SLEEP_US (30LL * 1000LL * 1000LL)
#define CT_USB_TASK_STACK_BYTES 6144

typedef enum { APP_INPUT_BUTTON = 1, APP_INPUT_AUDIO, APP_INPUT_RELOAD, APP_INPUT_TIME_SYNC } app_input_type_t;
typedef struct {
    app_input_type_t type;
    bsp_btn_t button;
    bsp_btn_ev_t button_event;
    ct_audio_event_t audio_event;
    unsigned seconds;
} app_input_t;

static const char *TAG = "classtrack";
static QueueHandle_t s_input_queue;
static volatile bool s_input_ready;
static key_guard_t s_key_guard;
static ct_store_t s_store;
static ct_class_list_t s_classes;
static ct_screen_t s_screen = CT_SCREEN_HOME;
static unsigned s_selected;
static unsigned s_class_index;
static unsigned s_record_seconds;
static int64_t s_last_action_us;
static volatile bool s_usb_busy;

static void protocol_reply(const char *status, const char *detail);

static uint64_t current_unix_time(void)
{
    time_t now = time(NULL);
    return now >= 1704067200 ? (uint64_t)now : 0;
}

static void default_classes(void)
{
    static const ct_class_t defaults[] = {
        { 0, "26机电2班", "高职英语" },
        { 1, "26机电3班", "高职英语" },
        { 2, "26学历1班", "汉语听力" },
        { 3, "26语言1班", "汉语听力" },
        { 4, "26语言2班", "汉语听力" },
        { 5, "26语言3班", "汉语听力" },
    };
    memset(&s_classes, 0, sizeof(s_classes));
    s_classes.version = 1;
    s_classes.count = sizeof(defaults) / sizeof(defaults[0]);
    memcpy(s_classes.items, defaults, sizeof(defaults));
    (void)ct_classes_save(&s_classes);
}

static void render(void)
{
    int battery = bsp_battery_soc();
    if (!bsp_lvgl_lock(500)) return;
    ct_ui_show(s_screen, s_selected, s_class_index, s_record_seconds,
               battery, current_unix_time());
    bsp_lvgl_unlock();
}

static unsigned recent_count(void)
{
    unsigned count = 0;
    for (unsigned i = 0; i < s_classes.count; ++i)
        if (ct_store_latest(&s_store, s_classes.items[i].id)) count++;
    return count;
}

static unsigned recent_class_at(unsigned rank)
{
    uint32_t before = UINT32_MAX;
    unsigned result = 0;
    for (unsigned pass = 0; pass <= rank; ++pass) {
        uint32_t best = 0;
        for (unsigned i = 0; i < s_classes.count; ++i) {
            const ct_record_info_t *record = ct_store_latest(&s_store, s_classes.items[i].id);
            if (record && record->sequence < before && record->sequence >= best) {
                best = record->sequence; result = i;
            }
        }
        before = best;
    }
    return result;
}

static void start_recording(void)
{
    if (s_class_index >= s_classes.count) return;
    if (current_unix_time() == 0) ESP_LOGW(TAG, "recording without synchronized timestamp");
    if (ct_audio_start_record(s_classes.items[s_class_index].id) == 0) {
        s_screen = CT_SCREEN_RECORDING; s_record_seconds = 0; render();
    } else {
        s_screen = CT_SCREEN_AUDIO_ERROR; render();
    }
}

static void enter_deep_sleep(void)
{
    if (ct_audio_is_busy()) return;
    int mv = bsp_button_read_mv();
    if (mv >= 0 && mv < BSP_BTN_MV_RELEASED_MIN) return;
    int level = 0;
    if (bsp_button_prepare_deep_sleep(&level) != ESP_OK) { esp_restart(); return; }
    (void)esp_deep_sleep_enable_gpio_wakeup(1ULL << BSP_BTN_GPIO, ESP_GPIO_WAKEUP_GPIO_LOW);
    (void)bsp_battery_sleep();
    (void)bsp_audio_sleep();
    (void)bsp_audio_prepare_deep_sleep();
    (void)bsp_i2c_prepare_deep_sleep();
    if (!bsp_lvgl_lock(1000)) { esp_restart(); return; }
    (void)bsp_display_prepare_deep_sleep();
    esp_deep_sleep_start();
    esp_restart();
}

static void handle_button(bsp_btn_t button, bsp_btn_ev_t event)
{
    s_last_action_us = esp_timer_get_time();
    if (s_screen == CT_SCREEN_RECORDING) {
        if (button == BSP_BTN_OK && event == BSP_BTN_CLICK) ct_audio_stop_record();
        return;
    }
    if (event != BSP_BTN_CLICK) return;

    if (s_screen == CT_SCREEN_HOME) {
        unsigned count = s_classes.count + 2U;
        if (button == BSP_BTN_UP) s_selected = (s_selected + count - 1U) % count;
        else if (button == BSP_BTN_DOWN) s_selected = (s_selected + 1U) % count;
        else if (button == BSP_BTN_OK) {
            if (s_selected < s_classes.count) { s_class_index = s_selected; s_selected = 0; s_screen = CT_SCREEN_DETAIL; }
            else if (s_selected == s_classes.count) { s_selected = 0; s_screen = CT_SCREEN_RECENT; }
            else { s_selected = 0; s_screen = CT_SCREEN_SETTINGS; }
        }
    } else if (s_screen == CT_SCREEN_DETAIL) {
        if (button == BSP_BTN_UP) { s_screen = CT_SCREEN_HOME; s_selected = s_class_index; }
        else if (button == BSP_BTN_DOWN && ct_store_latest(&s_store, s_classes.items[s_class_index].id))
            (void)ct_audio_play_latest(s_classes.items[s_class_index].id);
        else if (button == BSP_BTN_OK) { start_recording(); return; }
    } else if (s_screen == CT_SCREEN_REVIEW) {
        if (button == BSP_BTN_UP) s_selected = (s_selected + 3U) % 4U;
        else if (button == BSP_BTN_DOWN) s_selected = (s_selected + 1U) % 4U;
        else if (button == BSP_BTN_OK) {
            if (s_selected == 0) (void)ct_audio_preview();
            else if (s_selected == 1 && ct_audio_commit(current_unix_time()) == 0) {
                s_screen = CT_SCREEN_DETAIL; s_selected = 0;
            } else if (s_selected == 2) start_recording();
            else if (s_selected == 3) { ct_audio_discard(); s_screen = CT_SCREEN_DETAIL; s_selected = 0; }
        }
    } else if (s_screen == CT_SCREEN_RECENT) {
        unsigned count = recent_count();
        if (button == BSP_BTN_UP && s_selected == 0) { s_screen = CT_SCREEN_HOME; s_selected = s_classes.count; }
        else if (count && button == BSP_BTN_UP) s_selected--;
        else if (count && button == BSP_BTN_DOWN) s_selected = (s_selected + 1U) % count;
        else if (count && button == BSP_BTN_OK) { s_class_index = recent_class_at(s_selected); s_screen = CT_SCREEN_DETAIL; s_selected = 0; }
    } else if (s_screen == CT_SCREEN_SETTINGS) {
        if (button == BSP_BTN_UP) s_selected = (s_selected + 3U) % 4U;
        else if (button == BSP_BTN_DOWN) s_selected = (s_selected + 1U) % 4U;
        else if (button == BSP_BTN_OK && s_selected == 3) enter_deep_sleep();
        else if (button == BSP_BTN_OK && s_selected == 0) {
            s_screen = CT_SCREEN_TIME_SYNC;
            protocol_reply("REQUEST", "TIME");
        }
    } else if (s_screen == CT_SCREEN_TIME_SYNC) {
        if (button == BSP_BTN_UP || button == BSP_BTN_OK) { s_screen = CT_SCREEN_SETTINGS; s_selected = 0; }
    } else if (s_screen == CT_SCREEN_AUDIO_ERROR) {
        if (button == BSP_BTN_OK || button == BSP_BTN_UP) { s_screen = CT_SCREEN_DETAIL; s_selected = 0; }
    }
    render();
}

static void input_task(void *arg)
{
    (void)arg;
    app_input_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, pdMS_TO_TICKS(250)) == pdTRUE) {
            if (input.type == APP_INPUT_BUTTON) handle_button(input.button, input.button_event);
            else if (input.type == APP_INPUT_AUDIO && input.audio_event == CT_AUDIO_EVENT_RECORD_READY) {
                s_record_seconds = input.seconds; s_screen = CT_SCREEN_REVIEW; s_selected = 0; render();
            } else if (input.type == APP_INPUT_AUDIO && input.audio_event == CT_AUDIO_EVENT_ERROR) {
                s_screen = CT_SCREEN_AUDIO_ERROR; s_selected = 0; render();
            } else if (input.type == APP_INPUT_RELOAD) {
                (void)ct_classes_load(&s_classes); s_screen = CT_SCREEN_HOME; s_selected = 0; render();
            } else if (input.type == APP_INPUT_TIME_SYNC) {
                if (s_screen == CT_SCREEN_TIME_SYNC) { s_screen = CT_SCREEN_SETTINGS; s_selected = 0; }
                render();
            }
        }
        if (s_screen == CT_SCREEN_RECORDING) {
            unsigned seconds = ct_audio_pending_seconds();
            if (seconds != s_record_seconds) { s_record_seconds = seconds; render(); }
        }
        if (s_screen != CT_SCREEN_RECORDING && !ct_audio_is_busy() && !s_usb_busy &&
            esp_timer_get_time() - s_last_action_us >= IDLE_SLEEP_US) enter_deep_sleep();
    }
}

#define KEY_GUARD_MAX_US (10LL * 1000LL * 1000LL)

static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (key_guard_consume(&s_key_guard, event == BSP_BTN_RELEASE, esp_timer_get_time())) return;
    if (!s_input_ready || !s_input_queue) return;
    app_input_t input = { .type = APP_INPUT_BUTTON, .button = button, .button_event = event };
    (void)xQueueSend(s_input_queue, &input, 0);
}

static void on_audio(ct_audio_event_t event, unsigned seconds, void *user)
{
    (void)user;
    if (!s_input_queue) return;
    app_input_t input = { .type = APP_INPUT_AUDIO, .audio_event = event, .seconds = seconds };
    (void)xQueueSend(s_input_queue, &input, 0);
}

static void protocol_reply(const char *status, const char *detail)
{
    printf("CT %s%s%s\n", status, detail && *detail ? " " : "", detail ? detail : "");
    fflush(stdout);
}

static void save_sync_marker(uint64_t epoch)
{
    nvs_handle_t handle;
    if (nvs_open("classtrack", NVS_READWRITE, &handle) == ESP_OK) {
        (void)nvs_set_u64(handle, "last_sync", epoch); (void)nvs_commit(handle); nvs_close(handle);
    }
}

static size_t serial_read_exact(void *buffer, size_t length, TickType_t timeout)
{
    uint8_t *bytes = buffer;
    size_t received = 0;
    TickType_t started = xTaskGetTickCount();
    while (received < length && xTaskGetTickCount() - started < timeout) {
        size_t chunk = fread(bytes + received, 1, length - received, stdin);
        if (chunk > 0) {
            received += chunk;
            continue;
        }
        clearerr(stdin);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return received;
}

static void serial_task(void *arg)
{
    (void)arg;
    setvbuf(stdin, NULL, _IONBF, 0); setvbuf(stdout, NULL, _IONBF, 0);
    char line[96];
    for (;;) {
        if (!fgets(line, sizeof(line), stdin)) { vTaskDelay(pdMS_TO_TICKS(50)); clearerr(stdin); continue; }
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        s_last_action_us = esp_timer_get_time();
        ct_protocol_command_t command;
        if (ct_protocol_parse_line(line, &command) != 0) { protocol_reply("ERR", "BAD_COMMAND"); continue; }
        if (command.type == CT_PROTO_HELLO) {
            protocol_reply("OK", "{\"app\":\"ClassTrack\",\"protocol\":1,\"codec\":\"IMA_ADPCM_BLOCK_256\"}");
        } else if (command.type == CT_PROTO_TIME) {
            struct timeval value = { .tv_sec = (time_t)command.value };
            if (settimeofday(&value, NULL) == 0) {
                save_sync_marker(command.value);
                app_input_t synced = { .type = APP_INPUT_TIME_SYNC };
                (void)xQueueSend(s_input_queue, &synced, 0);
                protocol_reply("OK", "TIME");
            }
            else protocol_reply("ERR", "TIME");
        } else if (command.type == CT_PROTO_IMPORT) {
            s_usb_busy = true;
            char *csv = malloc(command.length + 1U);
            if (!csv) { s_usb_busy = false; protocol_reply("ERR", "NO_MEMORY"); continue; }
            size_t got = serial_read_exact(csv, command.length, pdMS_TO_TICKS(5000)); csv[got] = '\0';
            ct_class_list_t imported; char error[64] = {0};
            bool fonts_ok = true;
            int parsed = got == command.length && ct_crc32(csv, got) == command.crc32
                ? ct_classes_parse_csv(csv, got, &imported, error, sizeof(error)) : -1;
            if (parsed == 0 && bsp_lvgl_lock(500)) {
                for (unsigned i = 0; i < imported.count; ++i)
                    fonts_ok = fonts_ok && ct_ui_font_supports_text(imported.items[i].name) && ct_ui_font_supports_text(imported.items[i].course);
                bsp_lvgl_unlock();
            }
            if (parsed == 0 && fonts_ok && ct_classes_save(&imported) == 0) {
                app_input_t reload = { .type = APP_INPUT_RELOAD }; (void)xQueueSend(s_input_queue, &reload, 0);
                protocol_reply("OK", "IMPORT");
            } else protocol_reply("ERR", fonts_ok ? (error[0] ? error : "IMPORT") : "UNSUPPORTED_GLYPH");
            free(csv);
            s_last_action_us = esp_timer_get_time();
            s_usb_busy = false;
        } else if (command.type == CT_PROTO_LIST) {
            printf("CT OK ["); bool comma = false;
            for (unsigned id = 0; id < CT_MAX_CLASSES; ++id) {
                const ct_record_info_t *record = ct_store_latest(&s_store, id); if (!record) continue;
                printf("%s{\"class_id\":%u,\"unix_time\":%llu,\"bytes\":%lu}", comma ? "," : "", id,
                       (unsigned long long)record->unix_time, (unsigned long)record->encoded_bytes); comma = true;
            }
            printf("]\n");
        } else if (command.type == CT_PROTO_GET) {
            const ct_record_info_t *record = ct_store_latest(&s_store, command.class_id);
            if (!record || ct_audio_is_busy()) { protocol_reply("ERR", record ? "BUSY" : "NOT_FOUND"); continue; }
            s_usb_busy = true;
            printf("CT AUDIO %lu %08lx %lu %lu %llu\n", (unsigned long)record->encoded_bytes,
                   (unsigned long)record->payload_crc, (unsigned long)record->sample_rate,
                   (unsigned long)record->sample_count, (unsigned long long)record->unix_time);
            uint8_t buffer[512]; size_t offset = 0;
            while (offset < record->encoded_bytes) {
                size_t chunk = record->encoded_bytes - offset; if (chunk > sizeof(buffer)) chunk = sizeof(buffer);
                if (ct_store_read_record(&s_store, record, offset, buffer, chunk) != CT_STORE_OK) break;
                fwrite(buffer, 1, chunk, stdout); offset += chunk;
            }
            printf("CT DONE\n");
            s_last_action_us = esp_timer_get_time();
            s_usb_busy = false;
        }
    }
}

void app_main(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_LOGI(TAG, "ClassTrack first-stage firmware starting");
    const uint32_t wake_causes = esp_sleep_get_wakeup_causes();
    if (wake_causes & (1U << ESP_SLEEP_WAKEUP_GPIO))
        key_guard_arm(&s_key_guard, esp_timer_get_time(), KEY_GUARD_MAX_US);
    if (nvs_flash_init() != ESP_OK) ESP_LOGW(TAG, "NVS unavailable; preserving contents and continuing");
    if (ct_classes_load(&s_classes) != 0) default_classes();
    if (ct_store_init_partition(&s_store) != CT_STORE_OK) { ESP_LOGE(TAG, "classdata partition init failed"); return; }
    (void)bsp_i2c_init();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) { ESP_LOGE(TAG, "display init failed"); return; }
    bsp_display_backlight(75);
    (void)bsp_battery_init();
    if (bsp_audio_init() != ESP_OK) ESP_LOGW(TAG, "audio unavailable");
    ct_ui_init(&s_classes, &s_store);

    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(app_input_t));
    if (!s_input_queue || xTaskCreate(input_task, "ct_input", 4096, NULL, 6, NULL) != pdPASS) return;
    if (bsp_button_init(on_key, NULL) != ESP_OK) { ESP_LOGE(TAG, "button init failed"); return; }
    if (s_key_guard.armed)
        key_guard_sync_released(&s_key_guard, bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN);
    if (ct_audio_init(&s_store, on_audio, NULL) != 0) ESP_LOGW(TAG, "audio worker unavailable");
    if (xTaskCreate(serial_task, "ct_usb", CT_USB_TASK_STACK_BYTES, NULL, 3, NULL) != pdPASS)
        ESP_LOGW(TAG, "USB manager unavailable");
    s_input_ready = true;
    s_last_action_us = esp_timer_get_time();
    render();
}
