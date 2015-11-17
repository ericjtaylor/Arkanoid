#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "gpio.h"

// polls for button press by reading the state of a hardware file.
int gpio_poll()
{
  FILE *f;
  char buf[8];

  f = fopen("/sys/class/gpio/gpio7/value", "r");
  if (f == NULL) {
    printf("gpio_poll: fucked: %s\n", strerror(errno));
    _exit(1);
  }
  fgets (buf, sizeof(buf), f);
  fclose(f);
  if (strcmp(buf, "1\n") == 0)
    return 1;
  else
    return 0;

}

// initializes the gpio hardware file and
// sets the direction.
bool gpio_init(char *gpio_num, char *direction)
{
  FILE *f;
  f = fopen("/sys/class/gpio/export", "w");
  if (f == NULL) {
    printf("gpio_init: failed, no biggie, %s\n", strerror(errno));
    return false;
  }
  fprintf(f, "%s", gpio_num);
  fclose(f);
  f = fopen("/sys/class/gpio/gpio7/direction", "w");
  fprintf(f, "%s", direction);
  fclose(f);
  return true;
}
