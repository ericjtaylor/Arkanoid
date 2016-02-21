#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include "gpio.h"

/* TODO
   add indestructible blocks
   add preliminary gold blocks
   add multiball meter
   add gold balls
   add score
   add scoring
*/

/* IMPROVEMENTS
   unhack scaling and add bounds checking (segfault?)
*/

// screen dimension constants
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;
const int SCALE = 2;
const int FRAME_LEFT = 72;
const int FRAME_RIGHT = 247;
const int FRAME_TOP = 16;
const int FRAME_BOTTOM = 240;
const int WELL_WIDTH = 11;
const int WELL_HEIGHT = 28;

// game speed
const int TICKS_PER_FRAME = 0x20000*3;

// gameplay
const int BALL_INVINCIBLE = 0;
const int SUPER_MAX = 25;

static volatile bool gpio_exists = false;

struct Vector  {
  int32_t x;
  int32_t y;
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
    scan = IMG_Load("art/scanlines.png");
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

int box_collide(SDL_Rect *ball, SDL_Rect *paddle) {

  int collision = 1;

  if (ball->x + (ball->w - 1) < paddle->x) // R1 < L2
    collision = 0;
  else if (ball->x > paddle->x + (paddle->w - 1)) // L1 > R2
    collision = 0;
  else if (ball->y > paddle->y + (paddle->h - 1)) // U1 < D2
    collision = 0;
  else if (ball->y + (ball->h - 1) < paddle->y) // D1 > U2
    collision = 0;

  return collision;
}

inline int hit(struct Bricks *brick) {
  switch (brick->type) {
    case 0:
      return 0;
    case 1 ... 7:
    case 9:
      brick->type = 0;
    case 8:
      return 1;
    case 14:
      brick->type = 9;
      return 1;
    default:
      brick->type--;
      return 1;
  }
  return;
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
  if (srcrect->y + srcrect->h - 1 >= 320)
    srcrect2->h -= ((srcrect2->y + srcrect2->h - 1) - (320 - 1)) * SCALE;
  return;
}

int main(int argc, char *argv[]) {
  printf("Launching...\n");

  // inits
  gpio_exists = gpio_init("7", "out");
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
  int32_t ticks_remaining = 0;

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
  SDL_Surface* gfx_bg = load_image("art/background4.png");
  SDL_Surface* gfx_frame = load_image("art/frame.png");
  SDL_BlitSurface( gfx_frame, NULL, gfx_bg, NULL );

  SDL_Surface* gfx_ball = load_image("art/ball.png");
  SDL_Surface* gfx_paddle = load_image("art/paddle.png");
  SDL_Surface* gfx_brick = load_image("art/blocks.png");

  // playfield init
  SDL_Rect paddle = { (320/2) - 16, 240-24, 32, 8 };
  SDL_Rect paddle_size = { 0, 0, 32, 8 };

  struct Balls ball[8];
  int active_balls = 1;
  int supermeter = 0;
  int i, j;
  for(i = 0; i < 8; i++) {
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
    if (i > active_balls - 1) {
      ball[i].direction.x = 0;
      ball[i].loc.y = 999;
    }
  }

  struct Bricks brick[28][11];
  for (i=0; i<28; i++) {
    for (j=0; j<11; j++) {
      brick[i][j].loc.x = (FRAME_LEFT + j*16);
      brick[i][j].loc.y = (FRAME_TOP + i*8);
      brick[i][j].loc.h = 8;
      brick[i][j].loc.w = 16;
      brick[i][j].type = 0;
      if (i<=13) brick[i][j].type = (j % 9) + 1;
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

    SDL_BlitSurface( gfx_bg, NULL, screen, NULL );
    struct SDL_Rect temp4;
    temp4.x = 52*SCALE;
    temp4.y = (120+SUPER_MAX-supermeter)*SCALE;
    temp4.h = supermeter*SCALE;
    temp4.w = 8*SCALE;
    if (supermeter == SUPER_MAX && frame % 2)
      SDL_FillRect(screen, &temp4, SDL_MapRGB(screen->format, 0x8F, 0x8F, 0x8F));
    else
      SDL_FillRect(screen, &temp4, SDL_MapRGB(screen->format, 0xFF, 0xFF, 0xFF));

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
    for(b = 0; b < 8; b++) {

      if (ball[b].direction.x == 0) continue;

      // check for the paddle moving into the ball
      int penetration = 0;
      if (box_collide(&ball[b].loc, &paddle)) {
        supermeter += 2*active_balls;
        if (supermeter > SUPER_MAX) supermeter = SUPER_MAX;
        ball[b].direction.y = -1;
        ball[b].ticks_max.x = 0x194C6;
        ball[b].ticks_max.y = 0x3298C;
        penetration = 1;
        switch (ball[b].loc.x - paddle.x) {
        case -5 ... 12:
          printf("hit paddle inner paddle left\n");
          ball[b].direction.x = -1;
          break;
        case 13 ... 31: /* wide */
          printf("hit paddle inner paddle right\n");;
          ball[b].direction.x = 1;
          break;
        default:
          printf("********** invalid inner paddle hit x delta = %d\n", ball[b].loc.x - paddle.x);
        }
      }

      // find where we are in relation to the brick grid
      int ticks;
      struct Vector brick_coord, brick_inner;
      if (ball[b].direction.x == 1) {
      	brick_coord.x = ((ball[b].loc.x + (ball[b].loc.w - 1) - FRAME_LEFT) / 0x10);
      	brick_inner.x = ((ball[b].loc.x + (ball[b].loc.w - 1) - FRAME_LEFT) & 0x0F);
      } else {
      	brick_coord.x = ((ball[b].loc.x - FRAME_LEFT) / 0x10);
      	brick_inner.x = 0x0F - ((ball[b].loc.x - FRAME_LEFT) & 0x0F);
      }
      if (ball[b].direction.y == 1) {
      	brick_coord.y = ((ball[b].loc.y + (ball[b].loc.h - 1) - FRAME_TOP) / 0x08);
      	brick_inner.y = ((ball[b].loc.y + (ball[b].loc.h - 1) - FRAME_TOP) & 0x07);
      } else {
      	brick_coord.y = ((ball[b].loc.y - FRAME_TOP) / 0x08);
      	brick_inner.y = 0x07 - ((ball[b].loc.y - FRAME_TOP) & 0x07);
      }

      // move 1 pixel at a time
      int32_t ball_ticks_remaining = ticks_remaining;
      int iteration = 0;
      while(ball_ticks_remaining) {
          int move_x = 0;
          int move_y = 0;
          int hit_x = 0;
          int hit_y = 0;
          iteration++;
          // check for movement
          if (ball_ticks_remaining >= 0x10000){
            ticks = 0x10000;
            ball_ticks_remaining -= 0x10000;
          } else {
            ticks = ball_ticks_remaining;
            ball_ticks_remaining = 0;
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
          // check for vertical surface hits
          if (brick_inner.x == 16) {
            if (brick_coord.x + ball[b].direction.x < 0 || brick_coord.x + ball[b].direction.x > WELL_WIDTH - 1) {
              hit_y = 1;
              printf("frame %d: hit vert wall\n", frame);
            } else {
              if (hit(&brick[brick_coord.y][brick_coord.x + ball[b].direction.x])) {
                hit_y = 1;
                printf("frame %d: hit vert brick\n", frame);
              }
              // splashover
              if (brick_inner.y < ball[b].loc.h && (brick_coord.y - ball[b].direction.y) >= 0 && (brick_coord.y - ball[b].direction.y) < SCREEN_HEIGHT) {
                if (hit(&brick[brick_coord.y - ball[b].direction.y][brick_coord.x + ball[b].direction.x])) {
                  hit_y = 1;
                  printf("frame %d: hit vert brick splash\n", frame);
                }
              }
            }
          }
          // check for horizontal surface hits
          if (brick_inner.y == 8) {
            if (brick_coord.y + ball[b].direction.y < 0) {
              hit_x = 1;
              printf("frame %d: hit top wall\n", frame);
	    } else if (brick_coord.y + ball[b].direction.y > WELL_HEIGHT - 1) {
              /* bottom of screen bounce (only if invicibility is set) */
              if(active_balls == BALL_INVINCIBLE) {
                hit_x = 1;
                printf("frame %d: hit bottom wall\n", frame);
              } else if (brick_coord.y + ball[b].direction.y == WELL_HEIGHT) {
                active_balls--;
                ball_ticks_remaining = 0;
              }
            } else {
              if (hit(&brick[brick_coord.y + ball[b].direction.y][brick_coord.x])) {
                hit_x = 1;
                printf("frame %d: hit hori brick\n", frame);
              }
              // splashover
              if (brick_inner.x < ball[b].loc.w && (brick_coord.x - ball[b].direction.x) >= 0 && (brick_coord.x - ball[b].direction.x) < SCREEN_WIDTH) {
                if (hit(&brick[brick_coord.y + ball[b].direction.y][brick_coord.x - ball[b].direction.x])) {
                  hit_x = 1;
                  printf("frame %d: hit hori brick splash\n", frame);
                }
              }
            }
          }
          // check for corner hits (out of bounds should have already triggered a hit)
          if (brick_inner.x == 16 && brick_inner.y == 8 && hit_x == 0 && hit_y == 0) {
            if (hit(&brick[brick_coord.y + ball[b].direction.y][brick_coord.x + ball[b].direction.x])) {
              hit_x = 1;
              hit_y = 1;
              printf("frame %d: hit corner\n", frame);
            }
          }
          // check for paddle surface hits
          if((move_x == 1 || move_y == 1) && penetration == 0) {
            struct Balls temp_ball;
            temp_ball.loc.x = ball[b].loc.x + (move_x*ball[b].direction.x);
            temp_ball.loc.y = ball[b].loc.y;
            temp_ball.loc.h = ball[b].loc.h;
            temp_ball.loc.w = ball[b].loc.w;
            if (box_collide(&temp_ball.loc, &paddle)) {
              supermeter += 2*active_balls;
              if (supermeter > SUPER_MAX) supermeter = SUPER_MAX;
              hit_y = 1;
              printf("frame %d: hit paddle side\n", frame);
              ball[b].ticks_max.x = 0x194C6;
      	      ball[b].ticks_max.y = 0x3298C;
      	      hit_x = 1;
      	      ball[b].direction.y = 1;
            } else {
              temp_ball.loc.y = ball[b].loc.y + (move_y*ball[b].direction.y);
              if (box_collide(&temp_ball.loc, &paddle)) {
                supermeter += 2*active_balls;
                if (supermeter > SUPER_MAX) supermeter = SUPER_MAX;
                hit_x = 1;
                /* 37 pixels of collision, from -5 to 31 */
            		switch (temp_ball.loc.x - paddle.x) {
              		case -5 ... 0: /* wide */
              		  printf("frame %d: hit paddle wide left\n", frame);
              		  if (ball[b].direction.x == 1) hit_y = 1;
              		  ball[b].ticks_max.x = 0x194C6;
              		  ball[b].ticks_max.y = 0x3298C;
              		  break;
              		case 1 ... 6:
              		  printf("frame %d: hit paddle left\n", frame);
              		  if (ball[b].direction.x == 1) hit_y = 1;
              		  ball[b].ticks_max.x = 0x20000;
              		  ball[b].ticks_max.y = 0x20000;
              		  break;
              		case 7 ... 12:
              		  printf("frame %d: hit paddle tall left\n", frame);
              		  if (ball[b].direction.x == 1) hit_y = 1;
              		  ball[b].ticks_max.x = 0x3298C;
              		  ball[b].ticks_max.y = 0x194C6;
              		  break;
              		case 13 ... 19:
              		  printf("frame %d: hit paddle tall right\n", frame);
              		  if (ball[b].direction.x == -1) hit_y = 1;
              		  ball[b].ticks_max.x = 0x3298C;
              		  ball[b].ticks_max.y = 0x194C6;
              		  break;
              		case 20 ... 25:
              		  printf("frame %d: hit paddle right\n", frame);
              		  if (ball[b].direction.x == -1) hit_y = 1;
              		  ball[b].ticks_max.x = 0x20000;
              		  ball[b].ticks_max.y = 0x20000;
              		  break;
              		case 26 ... 31: /* wide */
              		  printf("frame %d: hit paddle wide right\n", frame);
              		  if (ball[b].direction.x == -1) hit_y = 1;
              		  ball[b].ticks_max.x = 0x194C6;
              		  ball[b].ticks_max.y = 0x3298C;
              		  break;
              		default:
              		  printf("********** invalid paddle surface hit -- x delta = %d\n", ball[b].loc.x - paddle.x);
            		}
              }
            }
          }

          // reflect if hit
          if (hit_x) {
            ball[b].direction.y *= -1;
            if (move_x == 1 && hit_y == 0) brick_inner.x--;
            brick_inner.y = (8 - brick_inner.y + ball[b].loc.h - 1);
            if (brick_inner.y >= 8) {
              brick_coord.y += ball[b].direction.y;
              brick_inner.y -= 8;
            }

          }
          if (hit_y) {
            ball[b].direction.x *= -1;
            if (move_y == 1 && hit_x == 0) brick_inner.y--;
            brick_inner.x = (16 - brick_inner.x + ball[b].loc.w - 1);
            if (brick_inner.x >= 16) {
      	      brick_coord.x += ball[b].direction.x;
      	      brick_inner.x -= 16;
            }
          }

          // move if no hits
          if (hit_x == 0 && hit_y == 0) {
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
          } else {
      	    ball[b].ticks.x = 0;
      	    ball[b].ticks.y = 0;
          }
        }

      if (ball[b].loc.y >= 240) ball[b].direction.x = 0;
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

      //Multiball
      if( e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE && supermeter == SUPER_MAX)
      {
        supermeter = 0;

        for (i = 0; i < 28; i++) {
          for (j=0; j<11; j++) {
            switch (brick[i][j].type)
            case 9 ... 13:
            brick[i][j].type++;
          }
        }

        int m;
        int highest = 0;
        int highest_x = 999;
        int highest_y = 999;
        active_balls = 8;
        for (m = 0; m < 8; m++) {
          if (ball[m].loc.y < highest_y) {
            highest_x = ball[m].loc.x;
            highest_y = ball[m].loc.y;
            highest = m;
          }
        }
        for (m = 0; m < 8; m++) {
          ball[m].loc.x = highest_x;
          ball[m].loc.y = highest_y;
          ball[m].ticks.x = 0;
          ball[m].ticks.y = 0;
          switch (m) {
          case 0: case 3: case 4: case 7:
            ball[m].ticks_max.x = 0x3298C;
            ball[m].ticks_max.y = 0x194C6;
            break;
          case 1: case 2: case 5: case 6:
            ball[m].ticks_max.x = 0x194C6;
            ball[m].ticks_max.y = 0x3298C;
            break;
          }
          switch (m) {
          case 0 ... 3:
            ball[m].direction.x = -1;
            break;
          case 4 ... 7:
            ball[m].direction.x = 1;
            break;
          }
          switch (m) {
          case 0: case 1: case 6: case 7:
            ball[m].direction.y = -1;
            break;
          case 2 ... 5:
            ball[m].direction.y = 1;
            break;
          }
        }
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
