#ifndef GPIO_H
#define GPIO_H

int gpio_poll();

bool gpio_init(char *gpio_num, char *direction);

#endif
