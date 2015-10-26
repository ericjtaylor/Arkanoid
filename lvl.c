#include <stdio.h>
#include <string.h>

#define WELL_WIDTH 7 // width is given in bricks

int main()
{
  FILE *f;
  char buf[256];
  int c;

  f = fopen("stage1.lvl", "r");
  while (fgets (buf, sizeof(buf), f))
    {
      c = 0;
      while (buf[c] != '\n' && c < WELL_WIDTH * 2)
	{
	  
	  if (buf[c] == '1')
	    {
	      printf("yes ");
	    }
	  else if (buf[c] == '0')
	    {
	      printf("no ");
	    } 
	  c++; 
	}
      printf("\n");
    }
  fclose(f);
  return 0;
}