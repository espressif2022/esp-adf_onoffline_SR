/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_log.h"
#include "lvgl.h"

#include "lv_example_pub.h"
#include "lv_example_image.h"

static const char *TAG = "ui_sr";

static bool sr_layer_enter_cb(void *create_layer);
static bool sr_layer_exit_cb(void *create_layer);
static void sr_layer_timer_cb(lv_timer_t *tmr);

lv_layer_t sr_layer = {
    .lv_obj_name    = "sr_layer",
    .lv_obj_parent  = NULL,
    .lv_obj_layer   = NULL,
    .lv_show_layer  = NULL,
    .enter_cb       = sr_layer_enter_cb,
    .exit_cb        = sr_layer_exit_cb,
    .timer_cb       = sr_layer_timer_cb,
};


const uint8_t sr_cmd_list[20][15]={
{"打开空调"},
{"关闭空调"},
{"增大风速"},
{"减小风速"},
{"升高一度"},
{"降低一度"},
{"制热模式"},
{"制冷模式"},
{"送风模式"},
{"节能模式"},
{"除湿模式"},
{"健康模式"},
{"睡眠模式"},
{"打开蓝牙"},
{"关闭蓝牙"},
{"开始播放"},
{"暂停播放"},
{"定时一小时"},
{"打开电灯"},
{"关闭电灯"}
};

static bool g_sr_anim_active = false;
static int32_t g_sr_anim_count = 0;
static lv_obj_t *g_sr_label = NULL;
static lv_obj_t *g_sr_mask = NULL;
static lv_obj_t *g_sr_bar[8] = {NULL};
static time_out_count time_100ms;

static int int16_sin(int32_t deg)
{
    static const int16_t sin_0_90_table[] = {
        0,     572,   1144,  1715,  2286,  2856,  3425,  3993,  4560,  5126,  5690,  6252,  6813,  7371,  7927,  8481,
        9032,  9580,  10126, 10668, 11207, 11743, 12275, 12803, 13328, 13848, 14364, 14876, 15383, 15886, 16383, 16876,
        17364, 17846, 18323, 18794, 19260, 19720, 20173, 20621, 21062, 21497, 21925, 22347, 22762, 23170, 23571, 23964,
        24351, 24730, 25101, 25465, 25821, 26169, 26509, 26841, 27165, 27481, 27788, 28087, 28377, 28659, 28932, 29196,
        29451, 29697, 29934, 30162, 30381, 30591, 30791, 30982, 31163, 31335, 31498, 31650, 31794, 31927, 32051, 32165,
        32269, 32364, 32448, 32523, 32587, 32642, 32687, 32722, 32747, 32762, 32767
    };

    if (deg < 0) {
        deg = -deg;
    }

    deg = deg % 360;

    if (deg <= 90) {
        return sin_0_90_table[deg];
    } else if (deg <= 180) {
        return sin_0_90_table[180 - deg];
    } else if (deg <= 270) {
        return -sin_0_90_table[deg - 270];
    } else {
        return -sin_0_90_table[360 - deg];
    }
}

static void sr_label_event_handler(lv_event_t *event)
{
    char *text = (char *) event-> param;
    if (NULL != text) {
        lv_label_set_text_static(g_sr_label, text);
    }
}

static void sr_mask_event_handler(lv_event_t *event)
{
    bool active = (bool) event->param;

    if (active) {
        ESP_LOGI(TAG, "animation start");
        g_sr_anim_count = 0;
        g_sr_anim_active = true;
    } else {
        ESP_LOGI(TAG, "animation stop");
        g_sr_anim_active = false;
    }
}

static void ui_speech_anim_cb(lv_timer_t *timer)
{
    const int32_t step = 40;

    if (g_sr_anim_active) {
        /* Set bar value */
        int32_t sr_val[4] = {
            abs(int16_sin(g_sr_anim_count * step + step * 0)) / (32768 >> 7),
            abs(int16_sin(g_sr_anim_count * step + step * 1)) / (32768 >> 7),
            abs(int16_sin(g_sr_anim_count * step + step * 2)) / (32768 >> 7),
            abs(int16_sin(g_sr_anim_count * step + step * 3)) / (32768 >> 7),
        };

        for (size_t i = 0; i < 4; i++) {
            lv_bar_set_value(g_sr_bar[i], sr_val[i] > 20 ? sr_val[i] : 20, LV_ANIM_ON);
            lv_bar_set_value(g_sr_bar[7 - i], sr_val[i] > 20 ? sr_val[i] : 20, LV_ANIM_ON);
            lv_bar_set_start_value(g_sr_bar[i], sr_val[i] > 20 ? -sr_val[i] : -20, LV_ANIM_ON);
            lv_bar_set_start_value(g_sr_bar[7 - i], sr_val[i] > 20 ? -sr_val[i] : -20, LV_ANIM_ON);
        }
        g_sr_anim_count++;
    } else {
        /* The first timer callback will set the bars to 0 */
        if (g_sr_anim_count != 0) {
            for (size_t i = 0; i < 8; i++) {
                lv_bar_set_value(g_sr_bar[i], 0, LV_ANIM_ON);
                lv_bar_set_start_value(g_sr_bar[i], -0, LV_ANIM_ON);
            }
            g_sr_anim_count = 0;
        } else {
            /* The second timer callback will hide sr mask */
        }
    }
}

void ui_sr_anim_start(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "sr animation initialize");
    g_sr_mask = lv_obj_create(parent);
    lv_obj_set_size(g_sr_mask, LV_HOR_RES, LV_VER_RES);
    lv_obj_clear_flag(g_sr_mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_sr_mask, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_sr_mask, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_sr_mask, lv_obj_get_style_bg_color(lv_obj_get_parent(g_sr_mask), LV_PART_MAIN), LV_STATE_DEFAULT);
    lv_obj_align(g_sr_mask, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(g_sr_mask, sr_mask_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *obj_img = NULL;
    obj_img = lv_obj_create(g_sr_mask);
    lv_obj_set_size(obj_img, 80, 80);
    lv_obj_clear_flag(obj_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj_img, 40, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj_img, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(obj_img, 40, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(obj_img, LV_OPA_30, LV_STATE_DEFAULT);
    lv_obj_align(obj_img, LV_ALIGN_CENTER, 0, -30);
    lv_obj_t *img_mic_logo = lv_img_create(obj_img);
    lv_img_set_src(img_mic_logo, &mic_logo);
    lv_obj_center(img_mic_logo);

    g_sr_label = lv_label_create(g_sr_mask);
    lv_label_set_long_mode(g_sr_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text_static(g_sr_label, "Listening...");
    lv_obj_set_style_text_font(g_sr_label, &pingfang_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_sr_label, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_align(g_sr_label, LV_ALIGN_CENTER, 0, 80);
    lv_obj_add_event_cb(g_sr_label, sr_label_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    for (size_t i = 0; i < sizeof(g_sr_bar) / sizeof(g_sr_bar[0]); i++) {
        g_sr_bar[i] = lv_bar_create(g_sr_mask);
        lv_obj_set_size(g_sr_bar[i], 5, 60);
        lv_obj_set_style_anim_time(g_sr_bar[i], 400, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_sr_bar[i], lv_color_make(237, 238, 239), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_sr_bar[i], lv_color_make(246, 175, 171), LV_PART_INDICATOR);
        lv_bar_set_range(g_sr_bar[i], -100, 100);
        lv_bar_set_value(g_sr_bar[i], 20, LV_ANIM_OFF);
        lv_bar_set_start_value(g_sr_bar[i], -20, LV_ANIM_OFF);
        lv_obj_set_style_anim_time(g_sr_bar[i], 400, LV_STATE_DEFAULT);

    }

    for (size_t i = 0; i < sizeof(g_sr_bar) / sizeof(g_sr_bar[0]) / 2; i++) {
        lv_obj_align_to(g_sr_bar[i], obj_img, LV_ALIGN_OUT_LEFT_MID, 15 * i - 65, 0);
        lv_obj_align_to(g_sr_bar[i + 4], obj_img, LV_ALIGN_OUT_RIGHT_MID, 15 * i + 20, 0);
    }

    g_sr_anim_count = 0;
    g_sr_anim_active = false;
    //lv_timer_create(ui_speech_anim_cb, 500, NULL);
}

void sr_anim_start(void)
{
    lv_event_send(g_sr_mask, LV_EVENT_VALUE_CHANGED, (void *) true);
}

void sr_anim_stop(void)
{
    lv_event_send(g_sr_mask, LV_EVENT_VALUE_CHANGED, (void *) false);
}

void sr_anim_set_text(char *text)
{
    if (g_sr_label) {
        lv_event_send(g_sr_label, LV_EVENT_VALUE_CHANGED, (void *) text);
    } else {
        ESP_LOGW(TAG, "g_sr_label null");
    }
}

static bool sr_layer_enter_cb(void *create_layer)
{
    bool ret = false;

    lv_layer_t *layer = create_layer;
    ESP_LOGI(TAG, "sr_layer_enter_cb");

    if (NULL == layer->lv_obj_layer) {
        ret = true;
        layer->lv_obj_layer = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(layer->lv_obj_layer);
        lv_obj_set_size(layer->lv_obj_layer, LV_HOR_RES, LV_VER_RES);

        ui_sr_anim_start(layer->lv_obj_layer);
        set_time_out(&time_100ms, 100);
    }
    sr_anim_stop();

    return ret;
}

static bool sr_layer_exit_cb(void *create_layer)
{
    ESP_LOGI(TAG, "sr_layer_exit_cb");
    g_sr_label = NULL;
    return true;
}

static void sr_layer_timer_cb(lv_timer_t *tmr)
{
    if (is_time_out(&time_100ms)) {
        ui_speech_anim_cb(NULL);
    }
}
