#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

/* TODO
   add flat double hits
   add cancave double hits
   add corner rebounds
   add edges as invisible indestructible blocks
   add paddle as indestructible block
   dynamic ball speeds
*/

/* IMPROVEMENTS
   ball race condition handling for collisions
   edge race condition handling (near corner hits)
   left & right collision spacing?
*/

//Screen dimension constants
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;
const int SCALE = 2;
const int FRAME_LEFT = 72;
const int FRAME_RIGHT = 247;
const int FRAME_TOP = 16;
const int BALLS = 1;
const int WELL_WIDTH = 11;
const int WELL_HEIGHT = 28;

struct Vector  {
  int x;
  int y;
};

struct Balls {
  SDL_Rect loc;
  struct Vector vel;
};

struct Bricks {
  SDL_Rect loc;
  int type;
};

void make_lvl(char *stage, struct Bricks brix[][WELL_WIDTH])
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
	  brix[y][x].type = c;
	  x++;
	}
      y++;
    }
  fclose(f);
}

// polls for button press by reading the state of a hardware file.
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
  SDL_SetColorKey(opt, SDL_SRCCOLORKEY, colourkey);
  SDL_SetColorKey(scaled, SDL_SRCCOLORKEY, colourkey);

  if (SCALE == 2) {
    SDL_Surface *scan = NULL;
    SDL_Surface *scan_opt = NULL;
    scan = IMG_Load("scanlines.png");
    scan_opt = SDL_DisplayFormat(scan);
    SDL_SetColorKey(scan_opt, SDL_SRCCOLORKEY, colourkey);
    SDL_BlitSurface(scan_opt, NULL, scaled, NULL);
    SDL_FreeSurface(scan);
    SDL_FreeSurface(scan_opt);
  }

  SDL_FreeSurface(basic);
  SDL_FreeSurface(opt);
  return scaled;
}

int collision(struct Balls *ball, SDL_Rect *paddle) {

  // top surface collision
  float x1 = ball->loc.x;
  float y1 = ball->loc.y + ball->loc.h;
  float x2 = ball->loc.x + ball->vel.x;
  float y2 = ball->loc.y + ball->loc.h + ball->vel.y;
  float x3 = paddle->x - ball->loc.w;
  float y3 = paddle->y;
  float x4 = paddle->x + paddle->w;
  float y4 = paddle->y;

  float denom = ((y4-y3) * (x2-x1)) - ((x4-x3) * (y2-y1));
  float ua = (((x4-x3) * (y1-y3)) - ((y4-y3) * (x1-x3))) / denom;
  if ((ua < 0) || (ua > 1)) return 0;
  float ub = (((x2-x1) * (y1-y3)) - ((y2-y1) * (x1-x3))) / denom;
  if ((ub < 0) || (ub > 1)) return 0;

  float x = x1 + (ua * (x2-x1));
  float y = y1 + (ua * (y2-y1));

  /* calc % remaining after collision */
  float pct;
  if (abs(ball->vel.y) > abs(ball->vel.y)) pct = 1.0 - ((y-y1)/(y2-y1));
  else pct = 1.0 - ((x-x1)/(x2-x1));

  /* 37 pixels of collision, x offset -5 through 31 relative to paddle */
  if (x - paddle->x < 1) { /* wide */
    ball->vel.x = -4;
    ball->vel.y = -2;
  } else if (x - paddle->x < 7) {
    ball->vel.x = -3;
    ball->vel.y = -3;
  } else if (x - paddle->x < 13) {
    ball->vel.x = -2;
    ball->vel.y = -4;
  } else if (x - paddle->x < 20) {
    ball->vel.x = 2;
    ball->vel.y = -4;
  } else if (x - paddle->x < 26) {
    ball->vel.x = 3;
    ball->vel.y = -3;
  } else if (x - paddle->x < 32) { /* wide */
    ball->vel.x = 4;
    ball->vel.y = -4;
  }

  /* advance a partial velocity amount */
  //ball->loc.y += (float) ball->vel.y * pct;
  //ball->loc.x += (float) ball->vel.x * pct;

  /* or force a render on the paddle -- better collision feel? */
  ball->loc.y = y - ball->loc.h - 1;
  ball->loc.x = x;

  return 1;

}

int down_collide(struct Balls *ball, struct Bricks *brick) {

  float x1 = ball->loc.x;
  float y1 = ball->loc.y;
  float x2 = ball->loc.x + ball->vel.x;
  float y2 = ball->loc.y + ball->vel.y;
  float x3 = brick->loc.x - ball->loc.w;
  float y3 = brick->loc.y - ball->loc.h;
  float x4 = brick->loc.x + brick->loc.w;
  float y4 = brick->loc.y - ball->loc.h;

  float denom = ((y4-y3) * (x2-x1)) - ((x4-x3) * (y2-y1));
  float ua = (((x4-x3) * (y1-y3)) - ((y4-y3) * (x1-x3))) / denom;
  if ((ua < 0) || (ua > 1)) return 0;
  float ub = (((x2-x1) * (y1-y3)) - ((y2-y1) * (x1-x3))) / denom;
  if ((ub < 0) || (ub > 1)) return 0;

  float x = x1 + (ua * (x2-x1));
  float y = y1 + (ua * (y2-y1));

  ball->vel.y *= -1;

  /* calc % remaining after collision */
  float pct;
  if (abs(ball->vel.y) > abs(ball->vel.y)) pct = 1.0 - ((y-y1)/(y2-y1));
  else pct = 1.0 - ((x-x1)/(x2-x1));

  /* advance a partial velocity amount */
  //ball->loc.y += (float) ball->vel.y * pct;
  //ball->loc.x += (float) ball->vel.x * pct;

  /* or force a render on the brick -- better collision feel? */
  ball->loc.y = y - 1;
  ball->loc.x = x;
  brick->type = 0;

  return 1;

}

int up_collide(struct Balls *ball, struct Bricks *brick) {

  float x1 = ball->loc.x;
  float y1 = ball->loc.y;
  float x2 = ball->loc.x + ball->vel.x;
  float y2 = ball->loc.y + ball->vel.y;
  float x3 = brick->loc.x - ball->loc.w;
  float y3 = brick->loc.y + brick->loc.h;
  float x4 = brick->loc.x + brick->loc.w;
  float y4 = brick->loc.y + brick->loc.h;

  float denom = ((y4-y3) * (x2-x1)) - ((x4-x3) * (y2-y1));
  float ua = (((x4-x3) * (y1-y3)) - ((y4-y3) * (x1-x3))) / denom;
  if ((ua < 0) || (ua > 1)) return 0;
  float ub = (((x2-x1) * (y1-y3)) - ((y2-y1) * (x1-x3))) / denom;
  if ((ub < 0) || (ub > 1)) return 0;

  float x = x1 + (ua * (x2-x1));
  float y = y1 + (ua * (y2-y1));

  ball->vel.y *= -1;

  /* calc % remaining after collision */
  float pct;
  if (abs(ball->vel.y) > abs(ball->vel.y)) pct = 1.0 - ((y-y1)/(y2-y1));
  else pct = 1.0 - ((x-x1)/(x2-x1));

  /* advance a partial velocity amount */
  //ball->loc.y += (float) ball->vel.y * pct;
  //ball->loc.x += (float) ball->vel.x * pct;

  /* or force a render on the brick -- better collision feel? */
  ball->loc.y = y + 1;
  ball->loc.x = x;
  brick->type = 0;

  return 1;

}

int left_collide(struct Balls *ball, struct Bricks *brick) {

  float x1 = ball->loc.x;
  float y1 = ball->loc.y;
  float x2 = ball->loc.x + ball->vel.x;
  float y2 = ball->loc.y + ball->vel.y;
  float x3 = brick->loc.x + brick->loc.w;
  float y3 = brick->loc.y - ball->loc.h;
  float x4 = brick->loc.x + brick->loc.w;
  float y4 = brick->loc.y + brick->loc.h;

  float denom = ((y4-y3) * (x2-x1)) - ((x4-x3) * (y2-y1));
  float ua = (((x4-x3) * (y1-y3)) - ((y4-y3) * (x1-x3))) / denom;
  if ((ua < 0) || (ua > 1)) return 0;
  float ub = (((x2-x1) * (y1-y3)) - ((y2-y1) * (x1-x3))) / denom;
  if ((ub < 0) || (ub > 1)) return 0;

  float x = x1 + (ua * (x2-x1));
  float y = y1 + (ua * (y2-y1));

  ball->vel.x *= -1;

  /* calc % remaining after collision */
  float pct;
  if (abs(ball->vel.y) > abs(ball->vel.y)) pct = 1.0 - ((y-y1)/(y2-y1));
  else pct = 1.0 - ((x-x1)/(x2-x1));

  /* advance a partial velocity amount */
  //ball->loc.y += (float) ball->vel.y * pct;
  //ball->loc.x += (float) ball->vel.x * pct;

  /* or force a render on the brick -- better collision feel? */
  ball->loc.y = y;
  ball->loc.x = x + 1;
  brick->type = 0;

  return 1;

}

int right_collide(struct Balls *ball, struct Bricks *brick) {

  float x1 = ball->loc.x;
  float y1 = ball->loc.y;
  float x2 = ball->loc.x + ball->vel.x;
  float y2 = ball->loc.y + ball->vel.y;
  float x3 = brick->loc.x - ball->loc.w;
  float y3 = brick->loc.y - ball->loc.h;
  float x4 = brick->loc.x - ball->loc.w;
  float y4 = brick->loc.y + brick->loc.h;

  float denom = ((y4-y3) * (x2-x1)) - ((x4-x3) * (y2-y1));
  float ua = (((x4-x3) * (y1-y3)) - ((y4-y3) * (x1-x3))) / denom;
  if ((ua < 0) || (ua > 1)) return 0;
  float ub = (((x2-x1) * (y1-y3)) - ((y2-y1) * (x1-x3))) / denom;
  if ((ub < 0) || (ub > 1)) return 0;

  float x = x1 + (ua * (x2-x1));
  float y = y1 + (ua * (y2-y1));

  ball->vel.x *= -1;

  /* calc % remaining after collision */
  float pct;
  if (abs(ball->vel.y) > abs(ball->vel.y)) pct = 1.0 - ((y-y1)/(y2-y1));
  else pct = 1.0 - ((x-x1)/(x2-x1));

  /* advance a partial velocity amount */
  //ball->loc.y += (float) ball->vel.y * pct;
  //ball->loc.x += (float) ball->vel.x * pct;

  /* or force a render on the brick -- better collision feel? */
  ball->loc.y = y;
  ball->loc.x = x - 1;
  brick->type = 0;

  return 1;

}

int box_collide(struct Balls *ball, SDL_Rect *paddle) {

  int collision = 1;

  if (ball->loc.x + ball->loc.w < paddle->x) // R1 < L2
    collision = 0;
  else if (ball->loc.x > paddle->x + paddle->w) // L1 > R2
    collision = 0;
  else if (ball->loc.y > paddle->y + paddle->h) // U1 < D2
    collision = 0;else if (ball->loc.y + ball->loc.h < paddle->y) // D1 > U2
    collision = 0;

  return collision;

}

inline void add_ns(struct timespec *ts1, struct timespec *ts2) {
  ts1->tv_nsec = ts1->tv_nsec + ts2->tv_nsec;
  if (ts1->tv_nsec >= 1000000000) {
    ts1->tv_sec += 1;
    ts1->tv_nsec -= 1000000000;
  }
  return;
}

inline int there_yet(struct timespec *ts1, struct timespec *ts2) {
  if (ts2->tv_sec < ts1->tv_sec) return 0;
  if (ts2->tv_sec > ts1->tv_sec) return 1;
  if (ts2->tv_nsec < ts1->tv_nsec) return 0;
  else return 1;
}

void Scale_Rect(SDL_Rect *srcrect, SDL_Rect *srcrect2) {
  srcrect2->x = srcrect->x*SCALE;
  srcrect2->y = srcrect->y*SCALE;
  srcrect2->h = srcrect->h*SCALE;
  srcrect2->w = srcrect->w*SCALE;
  return;
}

int main(int argc, char *argv[]) {
  printf("Launching...\n");

  // inits
  gpio_init("7", "out");
  //printf("I/O INIT OK\n");
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
    exit(1);
  } else printf("VIDEO INIT 1 OK\n");
  if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) < 0) {
    printf("SDL_INIT_PNG Error: %s\n", IMG_GetError());
    exit(1);
  } else printf("VIDEO INIT 2 OK\n");
  SDL_ShowCursor(SDL_DISABLE);
  struct timespec time_ns, next_frame, frame_delay;
  if (clock_gettime (CLOCK_MONOTONIC, &time_ns) == -1) {
    printf("Unable to init timer: clock_gettime CLOCK_MONOTONIC failure\n");
    exit(1);
  } else printf("TIMER INIT OK\n");
  next_frame.tv_sec = time_ns.tv_sec;
  next_frame.tv_nsec = time_ns.tv_nsec;
  frame_delay.tv_sec = 0;
  frame_delay.tv_nsec = 0;
  add_ns(&next_frame, &frame_delay);

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
  SDL_Surface* gfx_bg = load_image("background4.png");
  SDL_Surface* gfx_frame = load_image("frame.png");
  SDL_BlitSurface( gfx_frame, NULL, gfx_bg, NULL );

  SDL_Surface* gfx_ball = load_image("ball.png");
  SDL_Surface* gfx_paddle = load_image("paddle.png");
  SDL_Surface* gfx_brick = load_image("blox.png");

  // playfield init
  SDL_Rect paddle = { (320/2) - 16, 240-24, 32, 8 };
  SDL_Rect paddle_size = { 0, 0, 32, 8 };

  struct Balls	ball[BALLS];
  int i, j;
  for(i = 0; i < BALLS; i++) {
    ball[i].loc.x = paddle.x - 12 + -3 + 9;
    ball[i].loc.y = 240*0.8;
    ball[i].loc.h = 6;
    ball[i].loc.w = 6;
    ball[i].vel.x = 1;
    ball[i].vel.y = 20;
  }

  struct Bricks brick[28][11];
  for (i=0; i<28; i++) {
    for (j=0; j<11; j++) {
      brick[i][j].loc.x = (FRAME_LEFT + j*16);
      brick[i][j].loc.y = (FRAME_TOP + i*8);
      brick[i][j].loc.h = 8;
      brick[i][j].loc.w = 16;
      brick[i][j].type = 0;
      if (i<=13) brick[i][j].type = 1;
    }
  }

  make_lvl(argv[1], brick);
  
  struct Bricks wall_U, wall_D, wall_L, wall_R;

  wall_U.loc.x = FRAME_LEFT;
  wall_U.loc.y = FRAME_TOP;
  wall_U.loc.h = 0;
  wall_U.loc.w = (FRAME_RIGHT - FRAME_LEFT);

  wall_D.loc.x = FRAME_LEFT;
  wall_D.loc.y = 240;
  wall_D.loc.h = 0;
  wall_D.loc.w = (FRAME_RIGHT - FRAME_LEFT);

  wall_L.loc.x = FRAME_LEFT;
  wall_L.loc.y = FRAME_TOP;
  wall_L.loc.h = 240;
  wall_L.loc.w = 0;

  wall_R.loc.x = FRAME_RIGHT;
  wall_R.loc.y = FRAME_TOP;
  wall_R.loc.h = 240;
  wall_R.loc.w = 0;


  Uint8* keystate;

  // rendering loop
  while( !quit ) {

    // framerate limiting
    while (there_yet(&next_frame, &time_ns) == 0 && !keystate[SDLK_SPACE]) {
      clock_gettime (CLOCK_MONOTONIC, &time_ns);
    }

    if (gpio_poll() == 0 ) {
      SDL_BlitSurface( gfx_bg, NULL, screen, NULL );

      keystate = SDL_GetKeyState(NULL);
      // continuous-response keys
      if((keystate[SDLK_LEFT]) && (!keystate[SDLK_RIGHT]))
	{
	  paddle.x -= 5;
	  if (paddle.x < FRAME_LEFT) paddle.x = FRAME_LEFT;
	}
      if((keystate[SDLK_RIGHT]) && (!keystate[SDLK_LEFT]))
	{
	  paddle.x += 5;
	  if (paddle.x + paddle_size.w > FRAME_RIGHT) paddle.x = (FRAME_RIGHT - paddle_size.w);
	}

      // paddle animation
      switch (frame % 28) {
      case 0 ... 6:
	paddle_size.y = 0;
	break;
      case 7 ... 13:
	paddle_size.y = 8;
	break;
      case 14 ... 20:
	paddle_size.y = 16;
	break;
      case 21 ... 27:
	paddle_size.y = 24;
	break;
      }
      struct SDL_Rect temp, temp2;
      Scale_Rect(&paddle_size, &temp);
      Scale_Rect(&paddle, &temp2);
      SDL_BlitSurface( gfx_paddle, &temp, screen, &temp2 );

      int b;
      for(b = 0; b < BALLS; b++) {

        // moved the paddle into the ball (change to wide angle and reflect)
        int move_hit = box_collide(&ball[b], &paddle);
        if ((move_hit == 1) && ((ball[b].loc.x + ball[b].loc.w/2) < (paddle.x + paddle.w/2))) {
          ball[b].vel.x = -4;
          ball[b].vel.y = -2;
        } else if ((move_hit == 1) && ((ball[b].loc.x + ball[b].loc.w/2) >= (paddle.x + paddle.w/2))) {
          ball[b].vel.x = 4;
          ball[b].vel.y = -2;
        }

        // ball bounces on the surface of the paddle (change angle and reflect)
        if ((ball[b].vel.y < 0) || (collision(&ball[b], &paddle) == 0)) {

	  // brick collisions
	  int x_dir, bound_x1, bound_x2, y_dir, bound_y1, bound_y2;
	  if (ball[b].vel.x > 0) {
	    x_dir = 1;
	    bound_x1 = ((ball[b].loc.x + ball[b].loc.w - FRAME_LEFT)/(16));
	    bound_x2 = x_dir+((ball[b].loc.x + ball[b].loc.w - FRAME_LEFT + ball[b].vel.x)/(16));
	  }
	  else {
	    x_dir = -1;
	    bound_x1 = ((ball[b].loc.x - FRAME_LEFT)/(16));
	    bound_x2 = x_dir+((ball[b].loc.x - FRAME_LEFT + ball[b].vel.x)/(16));
	  }

	  if (ball[b].vel.y > 0) {
	    y_dir = 1;
	    bound_y1 = ((ball[b].loc.y + ball[b].loc.h - FRAME_TOP)/(8));
	    bound_y2 = y_dir+((ball[b].loc.y + ball[b].loc.h - FRAME_TOP + ball[b].vel.y)/(8));
	  }
	  else {
	    y_dir = -1;
	    bound_y1 = ((ball[b].loc.y - FRAME_TOP)/(8));
	    bound_y2 = y_dir+((ball[b].loc.y - FRAME_TOP + ball[b].vel.y)/(8));
	  }

	  int hit = 0;
	  for (i=bound_y1; i!=bound_y2; i+=y_dir) {
	    for (j=bound_x1; j!=bound_x2; j+=x_dir) {
	      if (brick[i][j].type == 0) continue;

	      if (ball[b].vel.y < 0) up_collide(&ball[b], &brick[i][j]);
	      else down_collide(&ball[b], &brick[i][j]);
	      if (hit == 1) break;

	      if (ball[b].vel.x < 0) left_collide(&ball[b], &brick[i][j]);
	      else right_collide(&ball[b], &brick[i][j]);
	      if (hit == 1) break;
	    }
	    if (hit == 1) break;
	  }

	  if (hit == 0) hit = up_collide(&ball[b], &wall_U);
	  if (hit == 0) hit = down_collide(&ball[b], &wall_D);
	  if (hit == 0) hit = left_collide(&ball[b], &wall_L);
	  if (hit == 0) hit = right_collide(&ball[b], &wall_R);

	  // todo -- collide with the nearest brick

	  // no collisions -- move
	  if (hit == 0) {
	    ball[b].loc.x += ball[b].vel.x;
	    ball[b].loc.y += ball[b].vel.y;
	  }
        }

        // // ball bounces on the side walls (reflect x)
        // if (ball[b].loc.x + ball[b].loc.w > FRAME_RIGHT) {
        // 	ball[b].vel.x *= -1;
        // 	ball[b].loc.x -= 2 * (ball[b].loc.x + ball[b].loc.w - FRAME_RIGHT);
        // } else if (ball[b].loc.x < FRAME_LEFT) {
        // 	ball[b].vel.x *= -1;
        // 	ball[b].loc.x -= 2 * (ball[b].loc.x - FRAME_LEFT);
        // }

        // // ball bounces on the top wall (reflect y)
        // if (ball[b].loc.y + ball[b].loc.h > SCREEN_HEIGHT) {
        // 	ball[b].vel.y *= -1;
        // 	ball[b].loc.y -= 2 * (ball[b].loc.y + ball[b].loc.h - SCREEN_HEIGHT);
        // } else if (ball[b].loc.y < FRAME_TOP) {
        // 	ball[b].vel.y *= -1;
        // 	ball[b].loc.y -= 2 * (ball[b].loc.y - FRAME_TOP);
        // }

        // draw ball
	Scale_Rect(&ball[b].loc, &temp);
        SDL_BlitSurface( gfx_ball, NULL, screen, &temp );
      }

      for (i = 0; i < 28; i++) {
	for (j=0; j<11; j++) {
	  if (brick[i][j].type != 0)
	    {
	      struct SDL_Rect temp3;
	      temp3.x = 0;
	      temp3.y = (brick[i][j].type - 1) *8*SCALE;
	      temp3.h = 8*SCALE;
	      temp3.w = 16*SCALE;
	      Scale_Rect(&brick[i][j].loc, &temp);
	      SDL_BlitSurface( gfx_brick, &temp3, screen, &temp );
	    }
	}
      }
    }

    SDL_Flip( screen );
    frame++;
    if (frame % 3 == 0) frame_delay.tv_nsec = 16666667;
    else frame_delay.tv_nsec = 16666666;
    add_ns(&next_frame, &frame_delay);

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

  double d_frame = (double) frame;
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
