#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

/* TODO
   test flat double hits
   test concave double hits
   test corner rebounds
   test dynamic ball speeds
*/

/* IMPROVEMENTS
   complete
*/

//Screen dimension constants
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;
const int SCALE = 2;
const int FRAME_LEFT = 72;
const int FRAME_RIGHT = 247;
const int FRAME_TOP = 16;
const int FRAME_BOTTOM = 240;

const int BALLS = 1;
const int WELL_WIDTH = 11;
const int WELL_HEIGHT = 28;
const int TICKS_PER_FRAME = 0x20000*3;

struct Vector  {
  int x;
  int y;
};

struct Balls {
  SDL_Rect loc;
  struct Vector direction;
  struct Vector ticks_max;
  struct Vector ticks;
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
  if (f != NULL) {
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
  } else printf("Unable to load '%s'\n", stage);
  return;
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

int box_collide(struct Balls *ball, SDL_Rect *paddle) {

  int collision = 1;

  if (ball->loc.x + ball->direction.x + ball->loc.w < paddle->x) // R1 < L2
    collision = 0;
  else if (ball->loc.x + ball->direction.x > paddle->x + paddle->w) // L1 > R2
    collision = 0;
  else if (ball->loc.y + ball->direction.y > paddle->y + paddle->h) // U1 < D2
    collision = 0;
  else if (ball->loc.y + ball->direction.y + ball->loc.h < paddle->y) // D1 > U2
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
  int ticks_remaining = 0;

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

  struct Balls ball[BALLS];
  int i, j;
  for(i = 0; i < BALLS; i++) {
    ball[i].loc.x = paddle.x - 12 + -3 + 9;
    ball[i].loc.y = 240*0.8;
    ball[i].loc.h = 6;
    ball[i].loc.w = 6;
    ball[i].direction.x = 1;
    ball[i].direction.y = 1;
    ball[i].ticks_max.x = 0x20000;
    ball[i].ticks_max.y = 0x20000;
    ball[i].ticks.x = 0;
    ball[i].ticks.y = 0;
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

  if (argc > 1) make_lvl(argv[1], brick);
  
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

      // find where we are in relation to the brick grid
      int ticks;
      struct Vector brick_coord, brick_inner;
    if (ball[b].direction.x == 1) {
      brick_coord.x = ((ball[b].loc.x + ball[b].loc.w - FRAME_LEFT) / 0x10);
      brick_inner.x = ((ball[b].loc.x + ball[b].loc.w - FRAME_LEFT) & 0x0F);
    } else {
      brick_coord.x = ((ball[b].loc.x - FRAME_LEFT) / 0x10);
      brick_inner.x = 0x0F - ((ball[b].loc.x - FRAME_LEFT) & 0x0F);
    }
    if (ball[b].direction.y == 1) {
      brick_coord.y = ((ball[b].loc.y + ball[b].loc.h - FRAME_TOP) / 0x08);
      brick_inner.y = ((ball[b].loc.y + ball[b].loc.h - FRAME_TOP) & 0x07);
    } else {
      brick_coord.y = ((ball[b].loc.y - FRAME_TOP) / 0x08);
      brick_inner.y = 0x07 - ((ball[b].loc.y - FRAME_TOP) & 0x07);
    }

      // check for paddle hits
      if (box_collide(&ball[b], &paddle)) {
        ball[b].direction.y = -1;
        // ran into the side
        if (ball[b].loc.y + ball[b].loc.h >= paddle.y) {
          ball[b].ticks_max.x = 0x194C6;
          ball[b].ticks_max.y = 0x3298C;
          switch (ball[b].loc.x - paddle.x) {
        case -7 ... 12:
          printf("inner paddle left\n");
          ball[b].direction.x = -1;
          break;
        case 13 ... 33: /* wide */
          printf("inner paddle right\n");;
          ball[b].direction.x = 1;
          break;
        default:
          printf("********** invalid inner paddle hit x delta = %d\n", ball[b].loc.x - paddle.x);
      }
        // surface hit -- 37 pixels of collision, x offset -5 through 31 relative to paddle
        } else {
          ball[b].ticks.x = 0;
          ball[b].ticks.y = 0;
            switch (ball[b].loc.x - paddle.x) {
        case -6 ... 0: /* wide */
              printf("wide left\n");
              ball[b].direction.x = -1;
          ball[b].ticks_max.x = 0x194C6;
          ball[b].ticks_max.y = 0x3298C;
          break;
        case 1 ... 6:
          printf("left\n");
          ball[b].direction.x = -1;
          ball[b].ticks_max.x = 0x20000;
          ball[b].ticks_max.y = 0x20000;
          break;
        case 7 ... 12:
          printf("tall left\n");
          ball[b].direction.x = -1;
          ball[b].ticks_max.x = 0x3298C;
          ball[b].ticks_max.y = 0x194C6;
          break;
        case 13 ... 19:
          printf("tall right\n");
          ball[b].direction.x = 1;
          ball[b].ticks_max.x = 0x3298C;
          ball[b].ticks_max.y = 0x194C6;
          break;
        case 20 ... 25:
          printf("right\n");
          ball[b].direction.x = 1;
          ball[b].ticks_max.x = 0x20000;
          ball[b].ticks_max.y = 0x20000;
          break;
        case 26 ... 33: /* wide */
          printf("wide right\n");
          ball[b].direction.x = 1;
          ball[b].ticks_max.x = 0x194C6;
          ball[b].ticks_max.y = 0x3298C;
          break;
        default:
          printf("********** invalid paddle hit x delta = %d\n", ball[b].loc.x - paddle.x);
        }
       }
      }

      // move 1 pixel at a time
      while(ticks_remaining) {
          int move_x = 0;
          int move_y = 0;
          int hit = 0;

          // check for movement
        if (ticks_remaining >= 0x10000){
          ticks = 0x10000;
          ticks_remaining -= 0x10000;
        } else {
          ticks = ticks_remaining;
          ticks_remaining = 0;
        }
          ball[b].ticks.x += ticks;
          if (ball[b].ticks.x >= ball[b].ticks_max.x) {
      move_x = 1;
      ball[b].ticks.x -= ball[b].ticks_max.x;
      }
      ball[b].ticks.y += ticks;
      if (ball[b].ticks.y >= ball[b].ticks_max.y) {
      move_y = 1;
      ball[b].ticks.y -= ball[b].ticks_max.y;
      }
      if (move_x) brick_inner.x++;
      if (move_y) brick_inner.y++;
      if (move_y == 0 && move_x == 0) continue;

      // check for horizontal surface hits
      if (brick_inner.x == 16) {
        if (brick_coord.x + ball[b].direction.x < 0 || brick_coord.x + ball[b].direction.x >= WELL_WIDTH) {
          hit = 1;
          ball[b].direction.x *= -1;
        } else {
          if (brick[brick_coord.y][brick_coord.x + ball[b].direction.x].type != 0) {
            hit = 1;
            brick[brick_coord.y][brick_coord.x + ball[b].direction.x].type = 0;
            ball[b].direction.x *= -1;
          }
          // splashover
          if (brick_inner.y < ball[b].loc.h && (brick_coord.y - ball[b].direction.y) >= 0 && (brick_coord.y - ball[b].direction.y) < SCREEN_HEIGHT) {
            if (brick[brick_coord.y - ball[b].direction.y][brick_coord.x + ball[b].direction.x].type != 0) {
              hit = 1;
              brick[brick_coord.y - ball[b].direction.y][brick_coord.x + ball[b].direction.x].type = 0;
              ball[b].direction.x *= -1;
            }
          }
        }
      }
      // check for vertical surface hits
      if (brick_inner.y == 8) {
        if (brick_coord.y + ball[b].direction.y < 0) {
          hit = 1;
          ball[b].direction.y *= -1;
        } else if (brick_coord.y + ball[b].direction.y >= WELL_HEIGHT) {
          /******* bottom of screen bounce -- remove at some point *******/
          hit = 1;
          ball[b].direction.y *= -1;
        } else {
          if (brick[brick_coord.y + ball[b].direction.y][brick_coord.x].type != 0) {
            hit = 1;
            brick[brick_coord.y + ball[b].direction.y][brick_coord.x].type = 0;
            ball[b].direction.y *= -1;
          }
          // splashover
          if (brick_inner.x < ball[b].loc.w && (brick_coord.x - ball[b].direction.x) >= 0 && (brick_coord.x - ball[b].direction.x) < SCREEN_WIDTH) {
            if (brick[brick_coord.y + ball[b].direction.y][brick_coord.x - ball[b].direction.x].type != 0) {
              hit = 1;
              brick[brick_coord.y + ball[b].direction.y][brick_coord.x - ball[b].direction.x].type = 0;
              ball[b].direction.y *= -1;
            }
        }
        }
      }
      // check for corner hits (out of bounds should have already triggered a hit)
      if (brick_inner.x == 16 && brick_inner.y == 8 && hit == 0) {
        if (brick[brick_coord.y + ball[b].direction.y][brick_coord.x + ball[b].direction.x].type != 0) {
          hit = 1;
          brick[brick_coord.y + ball[b].direction.y][brick_coord.x + ball[b].direction.x].type = 0;
          ball[b].direction.x *= -1;
          ball[b].direction.y *= -1;
        }
      }

      // move if no hits
      if (hit == 0) {
      if (move_x) {
        ball[b].loc.x += ball[b].direction.x;
        if (brick_inner.x == 16) {
          brick_coord.x += ball[b].direction.x;
          brick_inner.x = 0;
        }
      }
        if (move_y) {
          ball[b].loc.y += ball[b].direction.y;
        if (brick_inner.y == 8) {
          brick_coord.y += ball[b].direction.y;
          brick_inner.y = 0;
        }
        }
      }
      }

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
    ticks_remaining = TICKS_PER_FRAME;

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
