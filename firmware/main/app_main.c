#include <stdio.h>
#include "led_status.h"
#include "oled_demo.h"

void app_main(void)
{
    printf("Emiuet firmware: LED status state machine (auto demo)\n");
    led_status_start();
    oled_demo_start();

    // app_main can return; tasks keep running.
}
