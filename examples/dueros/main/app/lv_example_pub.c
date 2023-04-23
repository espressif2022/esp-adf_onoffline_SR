/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "time.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lv_example_pub.h"

static const char *TAG = "LVGL_PUB";

void lv_style_pre_init()
{

}
