#include <stdio.h>
#include <stdbool.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <sys/time.h>
#include <unistd.h>

/* TODO
moving the paddle with arrows
paddle side collision
paddle collision angle reflection
better frame limiting
*/

//Screen dimension constants
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;
const int BALLS = 32;
const int SCALE = 2;

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
int gpio_poll()
{
  FILE *f;
  char buf[8];

  if (access("/sys/class/gpio/gpio7/value", F_OK) != -1) {
	  f = fopen("/sys/class/gpio/gpio7/value", "r");
	  fgets (buf, sizeof(buf), f);
	  fclose(f);
	  if (strcmp(buf, "1\n") == 0)
	    return 1;
	  else
	    return 0;
  } else return 0;
}

// initializes the gpio hardware file and
// sets the direction.
void gpio_init(char *gpio_num, char *direction)
{
  FILE *f;
  if ((access("/sys/class/gpio/export", F_OK) != -1)
  	&&
  	(access("/sys/class/gpio/gpio7/direction", F_OK) != -1)) {
	  f = fopen("/sys/class/gpio/export", "w");
	  fprintf(f, "%s", gpio_num);
	  fclose(f);
	  f = fopen("/sys/class/gpio/gpio7/direction", "w");
	  fprintf(f, "%s", direction);
	  fclose(f);
  } else printf("GPIO not found\n");
}

Uint32 getpixel(SDL_Surface *surface, int x, int y)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to retrieve */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        return *p;
        break;

    case 2:
        return *(Uint16 *)p;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
            return p[0] << 16 | p[1] << 8 | p[2];
        else
            return p[0] | p[1] << 8 | p[2] << 16;
        break;

    case 4:
        return *(Uint32 *)p;
        break;

    default:
        return 0;       /* shouldn't happen, but avoids warnings */
    }
}

void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to set */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(Uint16 *)p = pixel;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
        } else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }
        break;

    case 4:
        *(Uint32 *)p = pixel;
        break;
    }
}

SDL_Surface* load_image(char *filename)
{
  SDL_Surface *basic = NULL;
  SDL_Surface *opt = NULL;
  basic = IMG_Load(filename);
  if (basic != NULL) opt = SDL_DisplayFormat(basic);

  Sint16 sy, sx, dy, dx;
  SDL_Surface *scaled = SDL_CreateRGBSurface(opt->flags, opt->w*SCALE, opt->h*SCALE, opt->format->BitsPerPixel,opt->format->Rmask, opt->format->Gmask, opt->format->Bmask, opt->format->Amask);
  for(sy = 0; sy < opt->h; sy++) //Run across all source Y pixels.
    for(sx = 0; sx < opt->w; sx++) //Run across all source X pixels.
	    for(dy = 0; dy < SCALE; ++dy) //Draw SCALE pixels for each Y pixel.
	        for(dx = 0; dx < SCALE; ++dx) //Draw SCALE pixels for each X pixel.
	            putpixel(scaled, SCALE*sx + dx, SCALE*sy + dy, getpixel(opt, sx, sy));

  Uint32 colourkey = SDL_MapRGB(scaled->format, 0xFF, 0, 0xFF);
  SDL_SetColorKey(scaled, SDL_SRCCOLORKEY, colourkey);

  SDL_FreeSurface(basic);
  SDL_FreeSurface(opt);
  return scaled;
}

int collision(struct Balls *ball, SDL_Rect *paddle) {

// top down collision
// left of ball
float x1 = ball->loc.x;
float y1 = ball->loc.y+ball->loc.h;
float x2 = ball->loc.x+ball->vel.x;
float y2 = ball->loc.y+ball->loc.h+ball->vel.y;
float x3 = paddle->x-ball->loc.h+1;
float y3 = paddle->y;
float x4 = paddle->x+paddle->w;
float y4 = paddle->y;
float denom = ((y4-y3) * (x2-x1)) - ((x4-x3) * (y2-y1));
float ua = (((x4-x3) * (y1-y3)) - ((y4-y3) * (x1-x3))) / denom;
if ((ua < 0) || (ua > 1)) return 0;
float ub = (((x2-x1) * (y1-y3)) - ((y2-y1) * (x1-x3))) / denom;
if ((ub < 0) || (ub > 1)) return 0;

int x = x1 + (ua * (x2-x1)); // should always be 0
int y = y1 + (ua * (y2-y1));

ball->vel.y *= -1;
ball->loc.y -= 2 * (ball->loc.y + ball->loc.h - y);

// right of ball

return 1;

}

int main() {

  // inits
  //gpio_init("7", "out");

  printf("Launching...\n");

  // Initializes the video subsystem
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
    exit(1);
  }

  printf("Video Init PASS\n");

  if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) < 0) {
    printf("SDL_INIT_PNG Error: %s\n", IMG_GetError());
    exit(1);
  }

  printf("Image Init PASS\n");
  SDL_ShowCursor(SDL_DISABLE);

  SDL_Surface* screen = NULL;
  screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 16, SDL_SWSURFACE | SDL_HWACCEL);

  // main loop flag
  bool quit = false;
  // event handler
  SDL_Event e;
  // frame counter
  int frame = 0;
  Uint32 start = SDL_GetTicks();

  // gfx loading
  SDL_Surface* gfx_ball = load_image("ball.png");
  SDL_Surface* gfx_paddle = load_image("paddle.png");

  // playfield init
  struct Balls	ball[BALLS];
  int i;
  for(i = 0; i < BALLS; i++) {
    ball[i].loc.x = 6*SCALE*i;
    ball[i].loc.y = 0;
    ball[i].loc.h = 6*SCALE;
    ball[i].loc.w = 6*SCALE;
    ball[i].vel.x = 1;+i;
    ball[i].vel.y = 10;+i;
  }
  SDL_Rect paddle = { (SCREEN_WIDTH / 2) - 16*SCALE, SCREEN_HEIGHT*0.9, 32, 8 };
  SDL_Rect paddle_size = { 0, 0, 32*SCALE, 8*SCALE };

  Uint32 frame_start = 0;
  Uint32 frame_end = SDL_GetTicks();

  //rendering loop
  while( !quit ) {

    // shitty framerate limiting
  	while (frame_end - frame_start < 16) {
  		frame_end = SDL_GetTicks();
  	}
  	frame_start = frame_end;

    if (gpio_poll() == 0 ) {
      SDL_Rect black = {1,1,SCREEN_WIDTH-2,SCREEN_HEIGHT-2};
      SDL_FillRect( screen, NULL, SDL_MapRGB(screen->format,0xFF,0xFF,0xFF));
      SDL_FillRect( screen, &black, SDL_MapRGB(screen->format,0,0,0));

      SDL_BlitSurface( gfx_paddle, &paddle_size, screen, &paddle );


      for(i = 0; i < BALLS; i++) {
      // advance motion
      if ((ball[i].vel.y < 0) || (collision(&ball[i], &paddle)) == 0) {
        ball[i].loc.x += ball[i].vel.x;
        ball[i].loc.y += ball[i].vel.y;
      }

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
          SDL_BlitSurface( gfx_ball, NULL, screen,&ball[i].loc );
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

  SDL_FreeSurface(gfx_ball);
  SDL_Quit();

  return 0;
}
