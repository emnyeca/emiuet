    // app_main can return; tasks keep running.#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_pins.h"
#include "led_status.h"
#include "oled_demo.h"

static void board_late_init_task(void *arg)
{
    (void)arg;

    /* Strapping safety delay */
    vTaskDelay(pdMS_TO_TICKS(MATRIX_SCAN_START_DELAY_MS));

    /* Now it's safe to touch matrix pins (GPIO45/46) */
    board_pins_init_matrix_late();

    /* One-shot task */
    vTaskDelete(NULL);
}

void app_main(void)
{
    printf("Emiuet firmware: boot\n");

    /* Stage 1: safe pins only (LED/buttons/power status, etc.) */
    board_pins_init_early();

    printf("Emiuet firmware: starting demo tasks\n");
    led_status_start();
    oled_demo_start();

    /* Stage 2: matrix pins after boot delay (strapping pins safety) */
    xTaskCreate(board_late_init_task, "board_late_init", 8192, NULL, 5, NULL);

    // app_main can return; tasks keep running.
}
