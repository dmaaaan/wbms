#include "led.h"

void mercury_led_init()
{
	const struct device *dev;

	dev = device_get_binding(LED0);
	gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
}

void mercury_led_flash(int ms)
{
	const struct device *dev;

	dev = device_get_binding(LED0);

	gpio_pin_set(dev, PIN, 1);
	k_msleep(ms);
	gpio_pin_set(dev, PIN, 0);
}

void mercury_led_setState(int state)
{
	const struct device *dev;

	dev = device_get_binding(LED0);

	gpio_pin_set(dev, PIN, state);
}

void mercury_led_done(int outer)
{
	for (int i = 0; i < outer; i++) {
		for (int j = 0; j < 2; j++) {
			mercury_led_flash(20);
			k_msleep(20);
		}
		k_msleep(500);
	}
}

void mercury_led_fatal(int n)
{
	while (true) {
		for (int i = 0; i < n; i++) {
			mercury_led_flash(250);
			k_msleep(250);
		}

		k_msleep(2000);
	}
}
