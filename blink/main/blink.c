#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define LED_GPIO 32
#define BUTTON_GPIO 5

void app_main(void){
    /* Configure the IOMUX register for pad LED_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(LED_GPIO);
    gpio_pad_select_gpio(BUTTON_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    bool pressed=1;
    while(1) {
	    pressed=gpio_get_level(BUTTON_GPIO);
	    printf("Turning %s the LED\n",pressed?"on":"off");
	    gpio_set_level(LED_GPIO, pressed);
	    vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
