#include <stdio.h>


int main()
{
  FILE *f;
  char buf[8];
  
  f = fopen("stage1.lvl", "r");
  while (fgets (buf, sizeof(buf), f))
    {
      printf("%s", buf);
    }
  fclose(f);
}
