#include <Arduino.h>          // What allows programming on the Arduino
#include "TVout.h"            // What allows the Arduino to create a composite signal
#include "../lib/data.h"      // Where the sprites & characters are stored

/*
TODO {
  yield
}

NODO {
  EB only collides with Player when PB is active. It is now a feature, because it makes the game more interesting.
  Figured out why that happened. I messed up the functions.
}
*/

TVout tv;

uint8_t flags = 0b00000000;     // Boolean flags
uint8_t frame = 0b00000000;     // Frame counter
uint8_t score = 0;              // Score counter

// Replacement for TVout's bitmap() function
void bitmap8(uint8_t x, uint8_t y, const uint8_t img[8]) {
  for (uint8_t by = 0; by < 8; by++) {
    for (uint8_t bx = 0; bx < 8; bx++) {
      if ((img[by] & 0b00000001 << bx)) tv.set_pixel(x + 8 - bx,y + by, 1);
    }
  }
}

// Clamps a value between a range
#define clamp(val, min, max) (val > max ? max : (val < min ? min : val))
// Clamps a variable between a range
#define aclamp(var, min, max) var = (var > max ? max : (var < min ? min : var))

// Space invaders clone

// Required to subtract from these, because screen goes Right to Left (127 to 0) for some reason
#define SCREEN_OFFSET_X 128
#define SCREEN_OFFSET_Y 96

#define X 0
#define Y 1

#define MOVE_FREQUENCY 2

//#define spr_transfer(a, b) for (uint8_t ii = 0; ii < 8; ii++) a[ii] = b[ii];

uint8_t player_x = 70;                                      // The player's X position
uint8_t board_offset[2] = {32,0};                           // The Enemy Board's position
uint8_t board[3] = {0b11111111,0b11111111,0b11111111};      // The board of enemies; 0 = dead, 1 = alive
uint8_t pb[2] = {70, 94};                                   // The Player Bullet's position
uint8_t eb[2] = {0 , 0 };                                   // The Enemy Bullet's position

/*
Flag definition

0 -> Current animation frame
1 -> Current enemy movement direction
2 -> Player bullet fired
3 -> Enemy bullet fired
4 -> Player has committed genocide
5 ->
6 ->
7 -> Game Over

*/

/*
[---------------------]
[  GRAPHIC FUNCTIONS  ]
[---------------------]
*/

// Default function that draws the main game frame
void draw_frame() {
  tv.fill(0);     // Clears the screen to draw the new frame

  tv.draw_line(7, 89, 128, 89, 1);      // Draws the "Game Line"

  // Draws the player
  tv.draw_rect(SCREEN_OFFSET_X - player_x + 3, 92, 6, 3, 1, 1);
  tv.set_pixel(SCREEN_OFFSET_X - player_x + 6, 91, 1);

  if (flags & 0b00000100)      // If PB active, draw PB
    tv.draw_line(SCREEN_OFFSET_X - pb[X], pb[Y], SCREEN_OFFSET_X - pb[X], pb[Y] - 1, 1);
  if (flags & 0b00001000)      // If EB active, draw EB
    tv.draw_line(SCREEN_OFFSET_X - eb[X], eb[Y], SCREEN_OFFSET_X - eb[X], eb[Y] - 1, 1);

  //Draws the game board of alive enemies
  for (uint8_t y = 0; y < 3; y++) {
    for (uint8_t x = 0; x < 8; x++) {
        if (board[2 - y] & (0b00000001 << (7 - x))) bitmap8(
        SCREEN_OFFSET_X - ((7 - x) * 8 + board_offset[X]),
        (2 - y) * 8 + board_offset[Y],
        sprite[flags & 0b00000001]);
    }
  }
}

// Replaces draw_frame() on Game Over
void game_over() {
  // Draws the Game Over border
  tv.draw_rect(SCREEN_OFFSET_X - 84, 55, 80, 24, 1, 0);

  // Draws the GAME OVER text
  if (!(flags & 0b00010000)) {
  for (uint8_t chr = 0; chr < 4; chr++) bitmap8(SCREEN_OFFSET_X + (chr * 8) - 80, 68, text[chr]);
  for (uint8_t chr = 4; chr < 8; chr++) bitmap8(SCREEN_OFFSET_X + (chr * 8 + 8) - 80, 68, text[chr]);
  }
  // Draws the score above the Game Over text
  bitmap8(SCREEN_OFFSET_X +  0 - 80, 60, text[8]);
  bitmap8(SCREEN_OFFSET_X +  8 - 80, 60, text[9]);
  bitmap8(SCREEN_OFFSET_X + 16 - 80, 60, text[4]);
  bitmap8(SCREEN_OFFSET_X + 24 - 80, 60, text[7]);
  bitmap8(SCREEN_OFFSET_X + 32 - 80, 60, text[6]);
  bitmap8(SCREEN_OFFSET_X + 48 - 80, 60, nums[(uint8_t)(floor(score/100))]);
  bitmap8(SCREEN_OFFSET_X + 56 - 80, 60, nums[(uint8_t)(floor(score/10) - floor(score/100) * 10)]);
  bitmap8(SCREEN_OFFSET_X + 64 - 80, 60, nums[(uint8_t)(score - floor(score / 10) * 10)]);
  ///bitmap8(SCREEN_OFFSET_X + 48 - 80, 60, nums[0]);
  ///bitmap8(SCREEN_OFFSET_X + 56 - 80, 60, nums[0]);
  ///bitmap8(SCREEN_OFFSET_X + 64 - 80, 60, nums[0]);
}

/*
[-------------------]
[  LOGIC FUNCTIONS  ]
[-------------------]
*/

// Gets the closest enemy line with alive enemies
uint8_t enemy_line() {
  for (uint8_t l = 3; l > 0; l--) {
    if (board[l - 1] > 0) return l;
  }
}

// Detects enemy collision with player bullet
void enemy_colli() {
  for (uint8_t ey = 0; ey < 3; ey++) {
    for (uint8_t ex = 0; ex < 8; ex++) {
      if (board[ey] & 0b00000001 << ex) {     //If enemy is alive, check collision
        uint8_t rect[2] = {
          (uint8_t)(board_offset[X] + (ex * 8)),   // Get the Collision box's X origin
          (uint8_t)(board_offset[Y] + (ey * 8))    // Get the Collision box's Y origin
        };
        uint8_t t = (pb[Y] > rect[1] && pb[Y] < rect[1] +7);            // Within Y bounds check
        rect[0] = pb[X] > rect[0] -7 && pb[X] < rect[0];                // Within X bounds check
        rect[1] = t||(pb[Y] -1 > rect[1] && pb[Y] -1 < rect[1] +7);     // Check for upper part of bullet
        if (rect[0] && rect[1]) {             // If collision & Enemy is alive:
          pb[Y] = 0xFF;                       // Disable PB
          board[ey] ^= 0b00000001 << ex;      // Kill Enemy
          score++;                            // Increment score
          return;                             // Exit Function
        }
      }
    }
  }
}

// Detects player collision with enemy bullet
void player_colli() {
    uint8_t rect[2] = {
      (uint8_t)(eb[X] >= player_x -10 && eb[X] <= player_x -3),     // Within X bounds check
      (uint8_t)(eb[Y] > 91 && eb[Y] < 94)                           // Within Y bounds check
    };
    rect[1] = rect[1] || (eb[Y] - 1 > 91 && eb[Y] - 1 < 94);        // Check for upper part of bullet
    // If collision, disable EB and Game Over
    if (rect[0] && rect[1]) flags = (flags | 0b10000000) ^ 0b00001000;
}

/*
[------------------]
[  MAIN FUNCTIONS  ]
[------------------]
*/

void setup() {
  DDRD |= 0b00000000;           // Sets the appropriate pins (2, 3, 4) to Input
  tv.begin(NTSC);               // Begin with default size screen

  randomSeed(PINC);     // Seed the RNG with noise from analog pin 0
  tv.draw_rect(SCREEN_OFFSET_X - 84, 55, 80, 24, 1, 0);

  tv.delay_frame(60);     // Wait 1 sec. before beginning
}

void loop() {
  tv.delay_frame(1);                              // Waits until next frame to process
  if (!(flags & 0b10000000)) {                    // If not Game Over, do game logic. else, game over logic
    if (frame < 60) frame++; else frame = 0;      // Increment frame counter if less than 60, else reset

    if ((frame + 1) % MOVE_FREQUENCY == 0) {      // If frame is divisible by MOVE_FREQUENCY:
      flags ^= 0b00000001;                        // Change animation sprite

      if (board_offset[X] < 9 || board_offset[X] > 63) {       // If on board edge:
        flags ^= 0b00000010;                                    // Change movement direction
        board_offset[Y]++;                                      // Increment Y
      }

      board_offset[X] += 1 - (2 * ((flags & 0b00000010) >> 1)); // Increment X

      if(!(flags & 0b00001000)){                    // If EB inactive:
        flags ^= 0b00001000;                        // Set active
        uint8_t r[2] = {
          (uint8_t)random(8),                       // Get random enemy column
          (uint8_t)random(3)                        // Get random enemy row
        };
        if (board[0] || board[1] || board[2])               // If there is enemies still alive:
          while (!(board[r[Y]] & 0b00000001 << r[X])) {     // Reroll until enemy at col, row R is alive
            r[X] = random(8);
            r[Y] = random(3);
          }
        eb[X] = r[X] * 8 - 5 + board_offset[X];     // Set position to random X
        eb[Y] = r[Y] * 8 + 4 + board_offset[Y];     // Set position to random Y
      }
    }

    if ((PIND & 0b00010000) && !(flags & 0b00000100)) {      // If player fired (Pin 4 HI) and PB not active:
      pb[X] = player_x - 6;                                   // Set PB's X position
      flags ^= 0b00000100;                                    // Activate PB
    }

    if (flags & 0b00000100) {     // If PB active & inbound, decrement Y, else reset
      pb[Y] -= 2;
      if (pb[Y] > 96){
        pb[Y] = 93;
        flags ^= 0b00000100;
      }
    }

    if (flags & 0b00001000) {      // If EB active & inbound, increment Y, else reset
      eb[Y] += 2;
      if (eb[Y] > 96){
        eb[Y] = 8;
        flags ^= 0b00001000;
      }
    }

    player_x += !(PIND & 0b00001000) - !(PIND & 0b00000100);      // Player Movement (L/R, Pin 2 HI - Pin 3 HI)

    aclamp(player_x, 10, 124);    // Clamp player position to gameplay area

    if (flags & 0b00001000) player_colli();               // If PB active, check collision
    if (flags & 0b00000100) enemy_colli();                // If EB active, check collision

    if (!(board[0] || board[1] || board[2])) flags |= 0b10010000;      // If all enemies dead, modified Game Over

    // If enemy reached bottom of screen, Game Over
    if(board_offset[Y] + enemy_line() * 8 + 4 > 93) flags |= 0b10000000;

    draw_frame();     // Draw main game screen
  }
  else {
    game_over();            // Draw Game Over screen
    tv.delay_frame(59);     // Wait just shy of 1 second
  }
}
