/** @file sm_timer.c
 *  @brief ESP32 timing for the NXP nano-package (delay loops).
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sm_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"

uint32_t sm_initSleep(void)
{
    return 0;
}

void sm_sleep(uint32_t msec)
{
    if (msec == 0) msec = 1;
    vTaskDelay(pdMS_TO_TICKS(msec));
}

void sm_usleep(uint32_t microsec)
{
    esp_rom_delay_us(microsec);
}
