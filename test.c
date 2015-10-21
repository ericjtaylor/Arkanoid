// user@jpiece:~/Desktop$ gcc -o test test.c -lSDL2 -lSDL2_image
// user@jpiece:~/Desktop$ ./test

#include <stdio.h>
#include <stdbool.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

//Screen dimension constants
const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
const int BALLS = 32;

struct Vector  {
  int x;
  int y;
};

struct Balls {
  SDL_Rect loc;
  struct Vector vel;
};

// polls for button press by reading the state
// of a hardware file.
int poll()
{
  FILE *f;
  char buf[8];
  
  f = fopen("/sys/class/gpio/gpio7/value", "r");
  fgets (buf, sizeof(buf), f);
  fclose(f);
  if (strcmp(buf, "1\n") == 0)
    return 1;
  else
    return 0;
}

// initializes the gpio hardware file and
// sets the direction.
void gpio_init(char *gpio_num, char *direction)
{
  FILE *f;
  f = fopen("/sys/class/gpio/export", "w");
  fprintf(f, "%s", gpio_num);
  fclose(f);
  f = fopen("/sys/class/gpio/gpio7/direction", "w");
  fprintf(f, "%s", direction);
  fclose(f);
}

SDL_Surface *load_image ()
{
  SDL_Surface* basic = NULL;
  SDL_Surface* opt = NULL;
  basic = SDL_LoadBMP("ball.bmp");
  if (basic != NULL) {
    opt = SDL_DisplayFormat(basic);
    if (opt != NULL) {
      Uint32 colourkey = SDL_MapRGB(opt->format, 0xFF, 0, 0xFF);
      SDL_SetColorKey(opt, SDL_SRCCOLORKEY, colourkey);
    }
  }
  SDL_FreeSurface(basic);
  return opt;
}

int main() {
  SDL_Surface* screen = NULL;

  gpio_init("7", "out");
  
  //First we need to start up SDL, and make sure it went ok
  if (SDL_Init(SDL_INIT_VIDEO) != 0){
    printf("SDL_Init Error: %s\n", SDL_GetError());
    return 1;
  }
  SDL_ShowCursor(SDL_DISABLE);

  screen = SDL_SetVideoMode( SCREEN_WIDTH, SCREEN_HEIGHT, 16, SDL_SWSURFACE | SDL_HWACCEL);
  
  //Main loop flag
  bool quit = false;
  //Event handler
  SDL_Event e;
  SDL_Surface* bmp = load_image();
  int frame = 0;
  Uint32 start = SDL_GetTicks();

  struct Balls	ball[BALLS];
  int i;
  for(i = 0; i < BALLS; i++) {
    ball[i].loc.x = 50*i;
    ball[i].loc.y = 0;
    ball[i].loc.h = 6;
    ball[i].loc.w = 6;
    ball[i].vel.x = 10+i;
    ball[i].vel.y = 10+i;
  }
  
  //rendering loop
  while( !quit ) {
    
    if (poll() == 0 ) {
      SDL_FillRect( screen, NULL, SDL_MapRGB(screen->format,0,0,0));
      for(i = 0; i < BALLS; i++) {
      // advance motion
      ball[i].loc.x += ball[i].vel.x;
      ball[i].loc.y += ball[i].vel.y;
      
      // reverse x momentum
      if (ball[i].loc.x + ball[i].loc.w > SCREEN_WIDTH) {
	ball[i].vel.x *= -1;
	ball[i].loc.x -= 2 * (ball[i].loc.x + ball[i].loc.w - SCREEN_WIDTH);
      } else if (ball[i].loc.x < 0) {
	ball[i].vel.x *= -1;
	ball[i].loc.x -= 2 * ball[i].loc.x;
      }
      
      // reverse y momentum
      if (ball[i].loc.y + ball[i].loc.h > SCREEN_HEIGHT) {
	ball[i].vel.y *= -1;
	ball[i].loc.y -= 2 * (ball[i].loc.y + ball[i].loc.h - SCREEN_HEIGHT);
      } else if (ball[i].loc.y < 0) {
	ball[i].vel.y *= -1;
	ball[i].loc.y -= 2 * ball[i].loc.y;
      }
      
          //Apply image to screen
          SDL_BlitSurface( bmp, NULL, screen,&ball[i].loc );
        }
    }
    
    SDL_Flip( screen );
    frame++;

    //Handle events on queue
    while( SDL_PollEvent( &e ) != 0 )
      {
	//User requests quit
	if( e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
	  {
	    quit = true;
	  }
      }
  }

  Uint32 end = SDL_GetTicks();

double d_frame =  (double) frame;
double d_start = (double) start;
double d_end = (double) end;
double d_time = (d_end - d_start) / 1000.0;

  printf("%u frames\n", frame);
  printf("%u ticks\n", end - start);
  printf("%f seconds\n", d_time);
  printf("%f fps\n", d_frame / d_time);

  SDL_FreeSurface(bmp);
  SDL_Quit();
  
  return 0;
}
