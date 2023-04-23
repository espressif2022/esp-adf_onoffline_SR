#include "esp_log.h"
#include "lvgl.h"

#include "bsp/esp-bsp.h"
#include "lv_example_pub.h"
#include "lv_example_image.h"

static const char *TAG = "ui";

#define TIME_ON_TRIGGER     500
#define LINE_SPACE          12

static bool speech_layer_enter_cb(void *create_layer);
static bool speech_layer_exit_cb(void *create_layer);
static void speech_layer_timer_cb(lv_timer_t *tmr);

lv_layer_t speech_layer = {
    .lv_obj_name = "speech_layer",
    .lv_obj_parent = NULL,
    .lv_obj_layer = NULL,
    .lv_show_layer = NULL,
    .enter_cb = speech_layer_enter_cb,
    .exit_cb = speech_layer_exit_cb,
    .timer_cb = speech_layer_timer_cb,
};

static lv_timer_t *timer_handle = NULL;
static lv_obj_t *label_question, *label_answer = NULL;
static lv_obj_t *cont;

static bool CN_flag = false;
static uint8_t wav_get_result = false;
static uint8_t count_get_result = false;
static uint16_t count_height = 0;

void set_get_wav_flag(bool result)
{
    wav_get_result = result;
}

void update_label_answer(char *text)
{
    if (NULL == label_question) {
        return;
    }

    int i, j, newline_count = 0;
    for (i = 0; i < strlen(text); i++) {
        if ((*(text + i) == '\\') && ((i + 1) < strlen(text)) && (*(text + i + 1) == 'n')) {
            newline_count++;
        }
    }
    ESP_LOGI(TAG, "len:%d, newline_count:%d", strlen(text), newline_count);

    j = 0;
    char *decode = heap_caps_malloc((strlen(text) + 1), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(decode);

    for (i = 0; i < strlen(text);) {
        if ((*(text + i) == '\\') && ((i + 1) < strlen(text)) && (*(text + i + 1) == 'n')) {
            *(decode + j++) = '\n';
            i += 2;
        } else {
            *(decode + j++) = *(text + i);
            i += 1;
        }
    }
    *(decode + j) = '\0';

    ESP_LOGI(TAG, "decode:[%d, %d] %s\r\n", j, strlen(decode), decode);

    CN_flag = false;
    bsp_display_lock(0);
    uint16_t label_len = strlen(text);

    for (int i = 0; i < 10; i++) {
        if (*(decode + 0) & 0x80) {
            CN_flag = true;
        }
    }

    if (CN_flag) {
        count_height = (newline_count + 1 + label_len / 3 / 12) * 35;
        lv_obj_set_height(label_answer, count_height);
        ESP_LOGI(TAG, "CN, hight:%d", count_height);
    } else {
        count_height = (newline_count + 1 + label_len / 22) * 35;
        lv_obj_set_height(label_answer, count_height);
        ESP_LOGI(TAG, "EN, hight:%d", count_height);
    }

    lv_label_set_text(label_answer, decode);
    count_get_result = true;
    bsp_display_unlock();

    if (decode) {
        free(decode);
    }
}

void update_label_question(char *text)
{
    if (NULL == label_question) {
        return;
    }

    bsp_display_lock(0);
    if (NULL == text) {
        lv_label_set_text(label_question, "I didn't hear what you were saying");
    } else {
        lv_label_set_text(label_question, text);
    }
    lv_label_set_text(label_answer, " ");
    lv_obj_set_height(label_answer, 100);

    count_get_result = false;
    wav_get_result = false;
    lv_obj_scroll_to_y(cont, 0, LV_ANIM_OFF);
    bsp_display_unlock();
}

static void lv_on_timer()
{
    if (count_get_result && wav_get_result) {
        lv_coord_t offset = lv_obj_get_scroll_y(cont);
        if (offset < count_height) {
            if (true == CN_flag) {
                offset += 2;
            } else {
                offset += 5;
            }
            lv_obj_scroll_to_y(cont, offset, LV_ANIM_OFF);
        } else {
            count_get_result = false;
        }
    }
}

static void top_pulldown_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCROLL_END) {
    } else if (code == LV_EVENT_SCROLL_BEGIN) {
    } else if (code == LV_EVENT_SCROLL) {
    }
}

void lv_create_label_win(lv_obj_t *parent)
{
    lv_obj_t *win = lv_win_create(parent, 40);
    lv_obj_set_size(win, LV_HOR_RES, LV_VER_RES);
    lv_win_add_title(win, "ChatGPT");

    cont = lv_win_get_content(win);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_add_event_cb(cont, top_pulldown_event_cb, LV_EVENT_ALL, 0);

    lv_obj_t *header = lv_win_get_header(win);
    label_question = lv_obj_get_child(header, 0);
    lv_label_set_long_mode(label_question, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_text_font(label_question, &pingfang_20, LV_STATE_DEFAULT);

    label_answer = lv_label_create(cont);
    lv_obj_set_style_text_color(label_answer, lv_color_hex(0x8F8F8F), 0);

    lv_label_set_text(label_answer, " ");

    lv_obj_set_size(label_answer, lv_pct(100), 400);
    lv_label_set_long_mode(label_answer, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label_answer, &pingfang_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(label_answer, 15, LV_STATE_DEFAULT);

    timer_handle = lv_timer_create(lv_on_timer, TIME_ON_TRIGGER, NULL);
    lv_timer_enable(true);
    ESP_LOGI(TAG, "label_win create ok");
}

static bool speech_layer_enter_cb(void *create_layer)
{
    bool ret = false;

    lv_layer_t *layer = create_layer;

    if (NULL == layer->lv_obj_layer) {
        ret = true;
        layer->lv_obj_layer = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(layer->lv_obj_layer);
        lv_obj_set_size(layer->lv_obj_layer, LV_HOR_RES, LV_VER_RES);
        lv_create_label_win(layer->lv_obj_layer);
    }

    return ret;
}

static bool speech_layer_exit_cb(void *create_layer)
{
    label_question = NULL;
    label_answer = NULL;

    return true;
}

static void speech_layer_timer_cb(lv_timer_t *tmr)
{
}