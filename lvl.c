#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WELL_WIDTH 11
#define WELL_HEIGHT 14

void make_lvl(char *stage, int brix[][WELL_HEIGHT])
{
  FILE *f;
  char buf[256];
  int x;
  int y = 0;
  int c;

  f = fopen(stage, "r");
  while (fgets (buf, sizeof(buf), f))
    {
      x = 0;
      while (buf[x] != '\n' && x < WELL_WIDTH)
	{
	  c = buf[x] - '0'; // cast char to int
	  brix[x][y] = c;
	  x++;
	}
      y++;
    }
  fclose(f);
}

int main()
{
  int brix[ WELL_WIDTH ][ WELL_HEIGHT ];
  make_lvl("stage1.lvl", brix); //example call

  // check to make sure that the values are correct
  /* 
  int x =0;
  int y =0;
  while (y < 14)
    {
      while (x < 11)
	{
	  printf("%d", brix[x][y]);     //use case
	  x++;
	}
      x = 0;
      printf("\n");
      y ++;
    }
  */
  return 0;
}
