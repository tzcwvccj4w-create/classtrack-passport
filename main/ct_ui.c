#include "ct_ui.h"

#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define COLOR_PAPER 0xF4EFE5
#define COLOR_INK   0x24433E
#define COLOR_MUTED 0x748C85
#define COLOR_MINT  0xE5EEE7
#define COLOR_LINE  0xC6D5CE
#define COLOR_ACCENT 0xBD7651

LV_FONT_DECLARE(classtrack_font_16);

static const ct_class_list_t *s_classes;
static const ct_store_t *s_store;
static lv_obj_t *s_screen;

static lv_obj_t *label_at(lv_obj_t *parent, const char *text, int x, int y,
                          int width, int height, lv_text_align_t align, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_font(label, &classtrack_font_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_label_set_text(label, text);
    return label;
}

static void frame(const char *hint, int battery_soc, uint64_t unix_time)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(COLOR_PAPER), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);

    char date_text[48] = "时间待校准";
    char time_text[8] = "";
    if (unix_time > 0) {
        time_t epoch = (time_t)unix_time;
        struct tm local;
        localtime_r(&epoch, &local);
        snprintf(date_text, sizeof(date_text), "%04d年%02d月%02d日",
                 local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
        strftime(time_text, sizeof(time_text), "%H:%M", &local);
    }
    label_at(s_screen, date_text, 12, 1, 148, 20, LV_TEXT_ALIGN_LEFT, COLOR_INK);
    if (time_text[0]) label_at(s_screen, time_text, 12, 21, 70, 20, LV_TEXT_ALIGN_LEFT, COLOR_INK);
    char battery[16];
    if (battery_soc >= 0) snprintf(battery, sizeof(battery), "%d%%", battery_soc);
    else snprintf(battery, sizeof(battery), "--%%");
    label_at(s_screen, battery, 170, 11, 58, 22, LV_TEXT_ALIGN_RIGHT, COLOR_INK);

    lv_obj_t *line = lv_obj_create(s_screen);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 12, 43); lv_obj_set_size(line, 216, 1);
    lv_obj_set_style_bg_color(line, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    line = lv_obj_create(s_screen);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 12, 290); lv_obj_set_size(line, 216, 1);
    lv_obj_set_style_bg_color(line, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    label_at(s_screen, hint, 8, 296, 224, 20, LV_TEXT_ALIGN_CENTER, COLOR_MUTED);
}

static void title(const char *eyebrow, const char *heading)
{
    label_at(s_screen, eyebrow, 14, 50, 212, 18, LV_TEXT_ALIGN_CENTER, COLOR_MUTED);
    label_at(s_screen, heading, 14, 70, 212, 24, LV_TEXT_ALIGN_CENTER, COLOR_INK);
}

static void row(int y, const char *text, const char *meta, bool selected)
{
    if (selected) {
        lv_obj_t *background = lv_obj_create(s_screen);
        lv_obj_remove_style_all(background);
        lv_obj_set_pos(background, 10, y - 3); lv_obj_set_size(background, 220, 27);
        lv_obj_set_style_radius(background, 9, 0);
        lv_obj_set_style_bg_color(background, lv_color_hex(COLOR_MINT), 0);
        lv_obj_set_style_bg_opa(background, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(background, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_border_width(background, 1, 0);
    }
    label_at(s_screen, text, 20, y, 132, 20, LV_TEXT_ALIGN_LEFT, COLOR_INK);
    label_at(s_screen, meta, 152, y, 70, 20, LV_TEXT_ALIGN_RIGHT, COLOR_MUTED);
}

static void show_home(unsigned selected)
{
    title("CLASS TRACK", "课到哪了？");
    unsigned count = s_classes ? s_classes->count : 0;
    unsigned total = count + 2U;
    unsigned start = selected >= 5U ? selected - 4U : 0U;
    if (start + 7U > total && total > 7U) start = total - 7U;
    unsigned visible = total - start;
    if (visible > 7U) visible = 7U;
    for (unsigned i = 0; i < visible; ++i) {
        unsigned index = start + i;
        if (index < count) {
            const ct_class_t *item = &s_classes->items[index];
            row(100 + (int)i * 27, item->name,
                ct_store_latest(s_store, item->id) ? "已记录" : "未记录", selected == index);
        } else {
            row(100 + (int)i * 27, index == count ? "最近更新" : "设置", ">", selected == index);
        }
    }
}

static void show_detail(unsigned class_index)
{
    if (!s_classes || class_index >= s_classes->count) return;
    const ct_class_t *item = &s_classes->items[class_index];
    title(item->course, item->name);
    const ct_record_info_t *record = ct_store_latest(s_store, item->id);
    label_at(s_screen, "最近一次记录", 20, 105, 200, 22, LV_TEXT_ALIGN_CENTER, COLOR_MUTED);
    char stamp[40] = "尚未记录";
    char duration[16] = "--:--";
    if (record) {
        if (record->unix_time == 0) {
            snprintf(stamp, sizeof(stamp), "时间待校准");
        } else {
            time_t epoch = (time_t)record->unix_time;
            struct tm local; localtime_r(&epoch, &local);
            strftime(stamp, sizeof(stamp), "%Y-%m-%d  %H:%M", &local);
        }
        snprintf(duration, sizeof(duration), "00:%02lu", (unsigned long)(record->sample_count / record->sample_rate));
    }
    label_at(s_screen, stamp, 20, 132, 200, 22, LV_TEXT_ALIGN_CENTER, COLOR_INK);
    label_at(s_screen, duration, 20, 165, 200, 28, LV_TEXT_ALIGN_CENTER, COLOR_INK);
    label_at(s_screen, record ? "按下键播放原录音" : "这个班还没有录音", 20, 213, 200, 22, LV_TEXT_ALIGN_CENTER, COLOR_INK);
    label_at(s_screen, "按 OK 开始录音", 20, 241, 200, 22, LV_TEXT_ALIGN_CENTER, COLOR_ACCENT);
}

static void show_recording(unsigned seconds)
{
    title("RECORDING", "正在录音");
    label_at(s_screen, "●", 20, 104, 200, 40, LV_TEXT_ALIGN_CENTER, COLOR_ACCENT);
    char text[16]; snprintf(text, sizeof(text), "00:%02u", seconds > 20 ? 20 : seconds);
    label_at(s_screen, text, 20, 155, 200, 30, LV_TEXT_ALIGN_CENTER, COLOR_INK);
    label_at(s_screen, "再按 OK 结束录音", 20, 213, 200, 22, LV_TEXT_ALIGN_CENTER, COLOR_INK);
    label_at(s_screen, "最长 20 秒", 20, 241, 200, 22, LV_TEXT_ALIGN_CENTER, COLOR_MUTED);
}

static void show_review(unsigned selected, unsigned seconds)
{
    title("REVIEW BEFORE SAVING", "录音完成");
    char duration[16]; snprintf(duration, sizeof(duration), "00:%02u", seconds > 20 ? 20 : seconds);
    label_at(s_screen, duration, 20, 98, 200, 26, LV_TEXT_ALIGN_CENTER, COLOR_INK);
    static const char *actions[] = { "试听这次录音", "确认保存", "重新录制", "放弃修改" };
    for (unsigned i = 0; i < 4; ++i) row(128 + (int)i * 31, actions[i], "", selected == i);
    label_at(s_screen, "确认前原录音保持不变", 12, 258, 216, 20, LV_TEXT_ALIGN_CENTER, COLOR_MUTED);
}

static void show_recent(unsigned selected)
{
    title("RECENT UPDATES", "最近更新");
    unsigned y = 104, row_index = 0;
    for (unsigned i = 0; s_classes && i < s_classes->count && row_index < 6; ++i) {
        const ct_class_t *item = &s_classes->items[i];
        const ct_record_info_t *record = ct_store_latest(s_store, item->id);
        if (!record) continue;
        char stamp[16] = "未校时";
        if (record->unix_time > 0) {
            time_t epoch = (time_t)record->unix_time; struct tm local;
            localtime_r(&epoch, &local); strftime(stamp, sizeof(stamp), "%m-%d", &local);
        }
        row((int)y, item->name, stamp, selected == row_index);
        y += 31; row_index++;
    }
    if (row_index == 0) label_at(s_screen, "还没有录音", 20, 145, 200, 22, LV_TEXT_ALIGN_CENTER, COLOR_MUTED);
}

static void show_settings(unsigned selected)
{
    title("OFFLINE SETTINGS", "设置");
    static const char *items[] = { "Mac USB 校时", "导入班级 CSV", "备份数据", "进入深度睡眠" };
    for (unsigned i = 0; i < 4; ++i) row(107 + (int)i * 39, items[i], "", selected == i);
}

static void show_time_sync(void)
{
    title("USB TIME SYNC", "等待 Mac 校时");
    label_at(s_screen, "请在 Mac 运行校时工具", 20, 139, 200, 22, LV_TEXT_ALIGN_CENTER, COLOR_INK);
    label_at(s_screen, "同步后自动返回", 20, 177, 200, 22, LV_TEXT_ALIGN_CENTER, COLOR_MUTED);
}

static void show_audio_error(void)
{
    title("RECORDING ERROR", "录音失败");
    label_at(s_screen, "没有保存任何修改", 20, 132, 200, 22, LV_TEXT_ALIGN_CENTER, COLOR_INK);
    label_at(s_screen, "按 OK 返回后重试", 20, 176, 200, 22, LV_TEXT_ALIGN_CENTER, COLOR_ACCENT);
}

void ct_ui_init(const ct_class_list_t *classes, const ct_store_t *store)
{
    s_classes = classes;
    s_store = store;
}

void ct_ui_show(ct_screen_t screen, unsigned selected, unsigned class_index,
                unsigned seconds, int battery_soc, uint64_t unix_time)
{
    if (s_screen) { lv_obj_delete(s_screen); s_screen = NULL; }
    static const char *hints[] = {
        "上下选择  OK进入", "上返回  下播放  OK录音", "OK结束  最长20秒",
        "上下选择  OK确认", "上键返回  OK进入", "上下选择  OK执行", "上键返回", "OK返回"
    };
    frame(hints[screen], battery_soc, unix_time);
    switch (screen) {
        case CT_SCREEN_HOME: show_home(selected); break;
        case CT_SCREEN_DETAIL: show_detail(class_index); break;
        case CT_SCREEN_RECORDING: show_recording(seconds); break;
        case CT_SCREEN_REVIEW: show_review(selected, seconds); break;
        case CT_SCREEN_RECENT: show_recent(selected); break;
        case CT_SCREEN_SETTINGS: show_settings(selected); break;
        case CT_SCREEN_TIME_SYNC: show_time_sync(); break;
        case CT_SCREEN_AUDIO_ERROR: show_audio_error(); break;
    }
    lv_screen_load(s_screen);
}

static bool decode_utf8(const unsigned char **cursor, uint32_t *codepoint)
{
    const unsigned char *p = *cursor;
    if (*p < 0x80) { *codepoint = *p++; *cursor = p; return true; }
    unsigned count; uint32_t value;
    if ((*p & 0xe0) == 0xc0) { count = 1; value = *p++ & 0x1f; }
    else if ((*p & 0xf0) == 0xe0) { count = 2; value = *p++ & 0x0f; }
    else if ((*p & 0xf8) == 0xf0) { count = 3; value = *p++ & 0x07; }
    else return false;
    while (count--) { if ((*p & 0xc0) != 0x80) return false; value = (value << 6) | (*p++ & 0x3f); }
    *codepoint = value; *cursor = p; return true;
}

bool ct_ui_font_supports_text(const char *utf8)
{
    if (!utf8) return false;
    const unsigned char *cursor = (const unsigned char *)utf8;
    while (*cursor) {
        uint32_t codepoint;
        if (!decode_utf8(&cursor, &codepoint)) return false;
        if (codepoint < 0x20) continue;
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(&classtrack_font_16, &glyph, codepoint, 0) || glyph.is_placeholder) return false;
    }
    return true;
}
