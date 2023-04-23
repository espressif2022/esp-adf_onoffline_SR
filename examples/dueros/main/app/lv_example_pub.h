/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef LV_EXAMPLE_PUB_H
#define LV_EXAMPLE_PUB_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lv_schedule_basic.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/


/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#define USER_MAINMENU_MAX       3

//#define TIME_ENTER_CLOCK_2MIN    (2*60*1000)/5///2min (2*60*1000)/(50 ms)
#define TIME_ENTER_CLOCK_2MIN    (0xFF*60*1000)/1//2min (2*60*1000)/(50 ms)

#define COLOUR_BLACK            0x000000
#define COLOUR_WHITE            0xFFFFFF
#define COLOUR_YELLOW           0xE9BD85
#define COLOUR_GREY_1F          0x1F1F1F
#define COLOUR_GREY_2F          0x2F2F2F

#define COLOUR_GREY_BF          0xBFBFBF
#define COLOUR_GREY_4F          0x4F4F4F
#define COLOUR_GREY_8F          0x8F8F8F

#define COLOUR_COOL_SET         0x616B7A
#define COLOUR_WARM_SET         0x6B5E58

extern lv_layer_t speech_layer;
extern lv_layer_t boot_Layer;
extern lv_layer_t sr_layer;

#endif /*LV_EXAMPLE_PUB_H*/
