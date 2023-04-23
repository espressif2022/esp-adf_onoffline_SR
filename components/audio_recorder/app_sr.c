/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "app_sr.h"

#include "esp_mn_speech_commands.h"
#include "esp_process_sdkconfig.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_mn_iface.h"
#include "model_path.h"

static const char *TAG = "app_sr";

typedef struct {
    sr_language_t lang;
    model_iface_data_t *model_data;
    const esp_mn_iface_t *multinet;
    const esp_afe_sr_iface_t *afe_handle;
    esp_afe_sr_data_t *afe_data;
    int16_t *afe_in_buffer;
    int16_t *afe_out_buffer;
    SLIST_HEAD(sr_cmd_list_t, sr_cmd_t) cmd_list;
    uint8_t cmd_num;
    TaskHandle_t feed_task;
    TaskHandle_t detect_task;
    TaskHandle_t handle_task;
    QueueHandle_t result_que;
    EventGroupHandle_t event_group;

    FILE *fp;
    bool b_record_en;
} sr_data_t;

static sr_data_t *g_sr_data = NULL;

#define I2S_CHANNEL_NUM     (2)
#define NEED_DELETE BIT0
#define FEED_DELETED BIT1
#define DETECT_DELETED BIT2

/**
 * @brief all default commands
 */
static const sr_cmd_t g_default_cmd_info[] = {
    // English
    {SR_CMD_LIGHT_ON, SR_LANG_EN, 0, "Turn On the Light", "TkN nN jc LiT", {NULL}},
    {SR_CMD_LIGHT_ON, SR_LANG_EN, 0, "Switch On the Light", "SWgp nN jc LiT", {NULL}},
    {SR_CMD_LIGHT_OFF, SR_LANG_EN, 0, "Switch Off the Light", "SWgp eF jc LiT", {NULL}},
    {SR_CMD_LIGHT_OFF, SR_LANG_EN, 0, "Turn Off the Light", "TkN eF jc LiT", {NULL}},
    {SR_CMD_SET_RED, SR_LANG_EN, 0, "Turn Red", "TkN RfD", {NULL}},
    {SR_CMD_SET_GREEN, SR_LANG_EN, 0, "Turn Green", "TkN GRmN", {NULL}},
    {SR_CMD_SET_BLUE, SR_LANG_EN, 0, "Turn Blue", "TkN BLo", {NULL}},
    {SR_CMD_CUSTOMIZE_COLOR, SR_LANG_EN, 0, "Customize Color", "KcSTcMiZ KcLk", {NULL}},
    {SR_CMD_PLAY, SR_LANG_EN, 0, "Sing a song", "Sgl c Sel", {NULL}},
    {SR_CMD_PLAY, SR_LANG_EN, 0, "Play Music", "PLd MYoZgK", {NULL}},
    {SR_CMD_NEXT, SR_LANG_EN, 0, "Next Song", "NfKST Sel", {NULL}},
    {SR_CMD_PAUSE, SR_LANG_EN, 0, "Pause Playing", "PeZ PLdgl", {NULL}},

    // Chinese
    {SR_CMD_LIGHT_ON, SR_LANG_CN, 0, "打开电灯", "da kai dian deng", {NULL}},
    {SR_CMD_LIGHT_OFF, SR_LANG_CN, 0, "关闭电灯", "guan bi dian deng", {NULL}},

    {SR_CMD_MAX, SR_LANG_CN, 0, "打开空调", "da kai kong tiao", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "关闭空调", "guan bi kong tiao", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "舒适模式", "shu shi mo shi", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "制冷模式", "zhi leng mo shi", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "制热模式", "zhi re mo shi", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "加热模式", "jia re mo shi", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "除湿模式", "chu shi mo shi", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "送风模式", "song feng mo shi", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "升高温度", "sheng gao wen du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "降低温度", "jiang di wen du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "温度调到最高", "wen du tiao dao zui gao", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "温度调到最低", "wen du tiao dao zui di", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "太热了", "tai re le", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "太冷了", "tai leng le", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "十六度", "shi liu du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "十七度", "shi qi du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "十八度", "shi ba du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "十九度", "shi jiu du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "二十度", "er shi du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "二十一度", "er shi yi du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "二十二度", "er shi er du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "二十三度", "er shi san du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "二十四度", "er shi si du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "二十五度", "er shi wu du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "二十六度", "er shi liu du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "二十七度", "er shi qi du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "二十八度", "er shi ba du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "二十九度", "er shi jiu du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "三十度", "san shi du", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "增大风速", "zeng da feng su", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "减小风速", "jian xiao feng su", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "高速风", "gao su feng", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "中速风", "zhong su feng", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "低速风", "di su feng", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "自动风", "zi dong feng", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "打开左右摆风", "da kai zuo you bai feng", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "打开上下摆风", "da kai shang xia bai feng", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "关闭左右摆风", "guan bi zuo you bai feng", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "关闭上下摆风", "guan bi shang xia bai feng", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "打开辅热", "da kai fu re", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "关闭辅热", "guan bi fu re", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "打开自清洁", "da kai zi qing jie", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "关闭自清洁", "guan bi zi qing jie", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "打开静音", "da kai jing yin", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "关闭静音", "guan bi jing yin", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "打开卧室灯", "da kai wo shi deng", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "关闭卧室灯", "guan bi wo shi deng", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "打开客厅灯", "da kai ke ting deng", {NULL}},
    {SR_CMD_MAX, SR_LANG_CN, 0, "关闭客厅灯", "guan bi ke ting deng", {NULL}},

};

esp_err_t app_sr_init_cmd(const esp_mn_iface_t *multinet,  model_iface_data_t *model_data)
{
    #if defined CONFIG_SR_MN_CN_MULTINET6_QUANT || defined CONFIG_SR_MN_EN_MULTINET6_QUANT
    return NULL;
    #endif

    g_sr_data = heap_caps_calloc(1, sizeof(sr_data_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_NO_MEM, TAG, "Failed create sr data");

    g_sr_data->result_que = xQueueCreate(3, sizeof(sr_result_t));
    g_sr_data->event_group = xEventGroupCreate();

    SLIST_INIT(&g_sr_data->cmd_list);

    ESP_LOGI(TAG, "update cmd, %p, %p", multinet, model_data);
    g_sr_data->multinet = multinet;
    g_sr_data->model_data = model_data;
    g_sr_data->lang = SR_LANG_CN;

    esp_mn_commands_alloc();
    g_sr_data->cmd_num = 0;

    uint8_t cmd_number = 0;
    // count command number
    for (size_t i = 0; i < sizeof(g_default_cmd_info) / sizeof(sr_cmd_t); i++) {
        if (g_default_cmd_info[i].lang == SR_LANG_CN) {
            app_sr_add_cmd(&g_default_cmd_info[i]);
            cmd_number++;
        }
    }
    ESP_LOGI(TAG, "cmd_number=%d", cmd_number);
    return app_sr_update_cmds();/* Reset command list */
}

esp_err_t app_sr_stop(void)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");

    /**
     * Waiting for all task stoped
     * TODO: A task creation failure cannot be handled correctly now
     * */
    xEventGroupSetBits(g_sr_data->event_group, NEED_DELETE);
    xEventGroupWaitBits(g_sr_data->event_group, NEED_DELETE | FEED_DELETED | DETECT_DELETED, 1, 1, portMAX_DELAY);

    if (g_sr_data->result_que) {
        vQueueDelete(g_sr_data->result_que);
        g_sr_data->result_que = NULL;
    }

    if (g_sr_data->event_group) {
        vEventGroupDelete(g_sr_data->event_group);
        g_sr_data->event_group = NULL;
    }

    if (g_sr_data->fp) {
        fclose(g_sr_data->fp);
        g_sr_data->fp = NULL;
    }

    if (g_sr_data->model_data) {
        g_sr_data->multinet->destroy(g_sr_data->model_data);
    }

    if (g_sr_data->afe_data) {
        g_sr_data->afe_handle->destroy(g_sr_data->afe_data);
    }

    sr_cmd_t *it;
    while (!SLIST_EMPTY(&g_sr_data->cmd_list)) {
        it = SLIST_FIRST(&g_sr_data->cmd_list);
        SLIST_REMOVE_HEAD(&g_sr_data->cmd_list, next);
        heap_caps_free(it);
    }

    if (g_sr_data->afe_in_buffer) {
        heap_caps_free(g_sr_data->afe_in_buffer);
    }

    if (g_sr_data->afe_out_buffer) {
        heap_caps_free(g_sr_data->afe_out_buffer);
    }

    heap_caps_free(g_sr_data);
    g_sr_data = NULL;
    return ESP_OK;
}

esp_err_t app_sr_get_result(sr_result_t *result, TickType_t xTicksToWait)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");

    xQueueReceive(g_sr_data->result_que, result, xTicksToWait);
    return ESP_OK;
}

esp_err_t app_sr_add_cmd(const sr_cmd_t *cmd)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");
    ESP_RETURN_ON_FALSE(NULL != cmd, ESP_ERR_INVALID_ARG, TAG, "pointer of cmd is invaild");
    ESP_RETURN_ON_FALSE(cmd->lang == g_sr_data->lang, ESP_ERR_INVALID_ARG, TAG, "cmd lang error");
    ESP_RETURN_ON_FALSE(ESP_MN_MAX_PHRASE_NUM >= g_sr_data->cmd_num, ESP_ERR_INVALID_STATE, TAG, "cmd is full");

    sr_cmd_t *item = (sr_cmd_t *)heap_caps_calloc(1, sizeof(sr_cmd_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(NULL != item, ESP_ERR_NO_MEM, TAG, "memory for sr cmd is not enough");
    memcpy(item, cmd, sizeof(sr_cmd_t));
    item->next.sle_next = NULL;
#if 1 // insert after
    sr_cmd_t *last = SLIST_FIRST(&g_sr_data->cmd_list);
    if (last == NULL) {
        SLIST_INSERT_HEAD(&g_sr_data->cmd_list, item, next);
    } else {
        sr_cmd_t *it;
        while ((it = SLIST_NEXT(last, next)) != NULL) {
            last = it;
        }
        SLIST_INSERT_AFTER(last, item, next);
    }
#else  // insert head
    SLIST_INSERT_HEAD(&g_sr_data->cmd_list, it, next);
#endif
    esp_mn_commands_add(g_sr_data->cmd_num, (char *)cmd->phoneme);
    g_sr_data->cmd_num++;
    return ESP_OK;
}

esp_err_t app_sr_modify_cmd(uint32_t id, const sr_cmd_t *cmd)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");
    ESP_RETURN_ON_FALSE(NULL != cmd, ESP_ERR_INVALID_ARG, TAG, "pointer of cmd is invaild");
    ESP_RETURN_ON_FALSE(id < g_sr_data->cmd_num, ESP_ERR_INVALID_ARG, TAG, "cmd id out of range");
    ESP_RETURN_ON_FALSE(cmd->lang == g_sr_data->lang, ESP_ERR_INVALID_ARG, TAG, "cmd lang error");

    sr_cmd_t *it;
    SLIST_FOREACH(it, &g_sr_data->cmd_list, next) {
        if (it->id == id) {
            ESP_LOGI(TAG, "modify cmd [%d] from %s to %s", id, it->str, cmd->str);
            esp_mn_commands_modify(it->phoneme, (char *)cmd->phoneme);
            memcpy(it, cmd, sizeof(sr_cmd_t));
            break;
        }
    }
    ESP_RETURN_ON_FALSE(NULL != it, ESP_ERR_NOT_FOUND, TAG, "can't find cmd id:%d", cmd->id);
    return ESP_OK;
}

esp_err_t app_sr_remove_cmd(uint32_t id)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");
    ESP_RETURN_ON_FALSE(id < g_sr_data->cmd_num, ESP_ERR_INVALID_ARG, TAG, "cmd id out of range");
    sr_cmd_t *it;
    SLIST_FOREACH(it, &g_sr_data->cmd_list, next) {
        if (it->id == id) {
            ESP_LOGI(TAG, "remove cmd id [%d]", it->id);
            SLIST_REMOVE(&g_sr_data->cmd_list, it, sr_cmd_t, next);
            heap_caps_free(it);
            g_sr_data->cmd_num--;
            break;
        }
    }
    ESP_RETURN_ON_FALSE(NULL != it, ESP_ERR_NOT_FOUND, TAG, "can't find cmd id:%d", id);
    return ESP_OK;
}

esp_err_t app_sr_remove_all_cmd(void)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");
    sr_cmd_t *it;
    while (!SLIST_EMPTY(&g_sr_data->cmd_list)) {
        it = SLIST_FIRST(&g_sr_data->cmd_list);
        SLIST_REMOVE_HEAD(&g_sr_data->cmd_list, next);
        heap_caps_free(it);
    }
    SLIST_INIT(&g_sr_data->cmd_list);
    return ESP_OK;
}

esp_err_t app_sr_update_cmds(void)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");

    uint32_t count = 0;
    sr_cmd_t *it;

    SLIST_FOREACH(it, &g_sr_data->cmd_list, next) {
        it->id = count++;
    }

    ESP_LOGI(TAG, "app_sr_update_cmds 2, %p, %p", g_sr_data->multinet, g_sr_data->model_data);

    esp_mn_error_t *err_id = esp_mn_commands_update(g_sr_data->multinet, g_sr_data->model_data);
    if(err_id){
        for (int i = 0; i < err_id->num; i++) {
            ESP_LOGE(TAG, "err cmd id:%d", err_id->phrase_idx[i]);
        }
    } else{
        for (int i = 0; i < err_id->num; i++) {
            ESP_LOGI(TAG, "err cmd id:%d", err_id->phrase_idx[i]);
        }
    }
    esp_mn_commands_print();

    return ESP_OK;
}

uint8_t app_sr_search_cmd_from_user_cmd(sr_user_cmd_t user_cmd, uint8_t *id_list, uint16_t max_len)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, 0, TAG, "SR is not running");

    uint8_t cmd_num = 0;
    sr_cmd_t *it;
    SLIST_FOREACH(it, &g_sr_data->cmd_list, next) {
        if (user_cmd == it->cmd) {
            if (id_list) {
                id_list[cmd_num] = it->id;
            }
            if (++cmd_num >= max_len) {
                break;
            }
        }
    }
    return cmd_num;
}

uint8_t app_sr_search_cmd_from_phoneme(const char *phoneme, uint8_t *id_list, uint16_t max_len)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, 0, TAG, "SR is not running");

    uint8_t cmd_num = 0;
    sr_cmd_t *it;
    SLIST_FOREACH(it, &g_sr_data->cmd_list, next) {
        if (0 == strcmp(phoneme, it->phoneme)) {
            if (id_list) {
                id_list[cmd_num] = it->id;
            }
            if (++cmd_num >= max_len) {
                break;
            }
        }
    }
    return cmd_num;
}

const sr_cmd_t *app_sr_get_cmd_from_id(uint32_t id)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, NULL, TAG, "SR is not running");
    ESP_RETURN_ON_FALSE(id < g_sr_data->cmd_num, NULL, TAG, "cmd id out of range");

    sr_cmd_t *it;
    SLIST_FOREACH(it, &g_sr_data->cmd_list, next) {
        if (id == it->id) {
            return it;
        }
    }
    ESP_RETURN_ON_FALSE(NULL != it, NULL, TAG, "can't find cmd id:%d", id);
    return NULL;
}
