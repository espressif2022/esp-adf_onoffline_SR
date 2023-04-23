/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef LV_EXAMPLE_IMAGE_H
#define LV_EXAMPLE_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

LV_IMG_DECLARE(esp_logo);
LV_IMG_DECLARE(esp_text);
LV_IMG_DECLARE(mic_logo);

/*
0x4E00-0x9FA5,0x20-0x7F,0xFF01-0xFF5E
*/
LV_FONT_DECLARE(pingfang_20);
/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EXAMPLE_IMAGE_H*/
