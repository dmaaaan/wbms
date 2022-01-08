//Stuff so we can blink the LED when done
#include <device.h>
#include <drivers/gpio.h>

#define LED0_NODE DT_ALIAS(led0)
#define LED0	  DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN		  DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS	  DT_GPIO_FLAGS(LED0_NODE, gpios)

void mercury_led_init();
void mercury_led_flash(int ms);
void mercury_led_done(int outer);
void mercury_led_fatal(int n);
void mercury_led_setState(int state);
