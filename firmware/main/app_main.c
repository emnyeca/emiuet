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

    /* Stage 1: configure rows only to avoid touching strapping pins */
    board_pins_init_matrix_prepare();

    /* Short pause before enabling column inputs */
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Stage 2: enable column inputs (now safe) */
    board_pins_enable_matrix_columns();

    /* Start matrix -> MIDI bridge with initial discard cycles to avoid
     * reacting to boot-time strapping states or keys held during boot.
     */
    extern void matrix_midi_bridge_start(int discard_cycles);
    extern void slider_task_start(void);
    matrix_midi_bridge_start(MATRIX_INITIAL_DISCARD_CYCLES);
    /* Start slider polling task (pitch-bend) */
    slider_task_start();

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
