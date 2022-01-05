#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tusb.h"

void usbaudio_init();
void usbaudio_update();
void usbaudio_addbuffer(uint32_t *samples, size_t size);
