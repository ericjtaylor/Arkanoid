#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WELL_WIDTH 11
#define WELL_HEIGHT 13

void make_lvl(char *stage)
{
  FILE *f;
  char buf[256];
  int brix[ WELL_WIDTH ][ WELL_HEIGHT ];
  int x;
  int y = 0;

  f = fopen(stage, "r");
  while (fgets (buf, sizeof(buf), f))
    {
      x = 0;
      y++;
      while (buf[x] != '\n' && x < WELL_WIDTH * 2)
	{
	  
	  if (buf[x] == '1')
	    {
	      brix[x][y] = 1;
	      //printf("%d,%d\t", x, y);
	    }
	  else if (buf[x] == '0')
	    {
	      brix[x][y] = 0;
	    }
	  x++;
	}
      //printf("\n");
    }
  fclose(f);
}

int main()
{
  int *brix;
  brix = malloc(WELL_WIDTH*WELL_HEIGHT*sizeof(int));
  make_lvl("stage1.lvl");
  free(brix);
  return 0;
}
