/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 */

#ifndef _USB_MICROPHONE_H_
#define _USB_MICROPHONE_H_

#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif



void usb_microphone_init();
void usb_microphone_task();
int get_usb_state();
// uint16_t usb_microphone_write(const void * data, uint16_t len);

#ifdef __cplusplus
} // extern c
#endif


#endif
