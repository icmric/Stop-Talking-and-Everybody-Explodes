/*
  Matrix Maze Module
  
  Uses an 8x8 WS2812B addressable LED matrix and two encoders for player movement.
  Navigate from top-left to bottom-right to solve the module.
  
  Penalty: Hitting a wall = -3 seconds from timer
  
  LED Matrix:
  - 64 addressable WS2812B LEDs (8x8 grid)
  - Data pin: Digital 22
  - Brightness limited to 15% to protect power supply
  - Colors: Red (player), Blue (goal unsolved), Green (goal solved), White (walls), Black (empty)
*/

#ifndef MAZE_MODULE_H
#define MAZE_MODULE_H

#include <Encoder.h>
#include <Adafruit_NeoPixel.h>

// Forward declare penalty function
extern void applyPenalty(const char* reason);
extern bool coreSolved;    // Check if core event is complete before setting up maze
extern bool coreTriggered; // Check if core event has been triggered

// =====================================================
// NEOPIXEL COLOR DEFINITIONS
// =====================================================
// NeoPixel colors use 32-bit RGB format (0xRRGGBB)
// Brightness is globally limited to 15% in main sketch
const uint32_t COLOR_BLACK = 0x000000;
const uint32_t COLOR_RED = 0xFF0000;
const uint32_t COLOR_GREEN = 0x00FF00;
const uint32_t COLOR_BLUE = 0x0000FF;
const uint32_t COLOR_WHITE = 0xFFFFFF;

// =====================================================
// MATRIX MAZE MODULE
// =====================================================

const int MAZE_W = 6;
const int MAZE_H = 6;

int playerX = 0;
int playerY = 0;
int goalX = 5;
int goalY = 5;

bool mazeSolved = false;
bool mazeSetupDone = false;  // Track if maze LED matrix has been initialized
bool mazeDisplayCleared = false;  // Track if maze display has been cleared after solving
unsigned long mazeSolvedTime = 0;  // Track when maze was solved (to delay clearing)
const unsigned long MAZE_CLEAR_DELAY = 1500;  // Show solution for 1.5 seconds before clearing
long mazeLastLeftPos = 0;
long mazeLastRightPos = 0;
unsigned long mazeLastLeftMoveTime = 0;  // Track time of last LEFT encoder move
unsigned long mazeLastRightMoveTime = 0;  // Track time of last RIGHT encoder move
const unsigned long mazeMoveDelay = 80;  // Delay between moves (ms) - per encoder

bool wallRight[MAZE_H][MAZE_W];
bool wallDown[MAZE_H][MAZE_W];
bool wallLeft[MAZE_H][MAZE_W];
bool wallUp[MAZE_H][MAZE_W];

// Forward declarations - these objects are declared in main file
extern Encoder encLeft;
extern Encoder encRight;
extern Adafruit_NeoPixel matrix;  // NeoPixel strip object

void clearWalls() {
  for (int y = 0; y < MAZE_H; y++) {
    for (int x = 0; x < MAZE_W; x++) {
      wallRight[y][x] = false;
      wallDown[y][x] = false;
      wallLeft[y][x] = false;
      wallUp[y][x] = false;
    }
  }
}

void buildOuterWalls() {
  for (int x = 0; x < MAZE_W; x++) {
    wallUp[0][x] = true;
    wallDown[MAZE_H - 1][x] = true;
  }
  for (int y = 0; y < MAZE_H; y++) {
    wallLeft[y][0] = true;
    wallRight[y][MAZE_W - 1] = true;
  }
}

void addVerticalWall(int x, int y) {
  wallRight[y][x] = true;
  wallLeft[y][x + 1] = true;
}

void addHorizontalWall(int x, int y) {
  wallDown[y][x] = true;
  wallUp[y + 1][x] = true;
}

bool isMazePositionReachable(int startX, int startY, int targetX, int targetY) {
  bool visited[MAZE_H][MAZE_W];
  for (int y = 0; y < MAZE_H; y++) {
    for (int x = 0; x < MAZE_W; x++) {
      visited[y][x] = false;
    }
  }

  const int maxCells = MAZE_W * MAZE_H;
  int queueX[maxCells];
  int queueY[maxCells];
  int head = 0;
  int tail = 0;

  queueX[tail] = startX;
  queueY[tail] = startY;
  tail++;
  visited[startY][startX] = true;

  while (head < tail) {
    int x = queueX[head];
    int y = queueY[head];
    head++;

    if (x == targetX && y == targetY) {
      return true;
    }

    if (!wallRight[y][x] && !visited[y][x + 1]) {
      visited[y][x + 1] = true;
      queueX[tail] = x + 1;
      queueY[tail] = y;
      tail++;
    }
    if (!wallLeft[y][x] && !visited[y][x - 1]) {
      visited[y][x - 1] = true;
      queueX[tail] = x - 1;
      queueY[tail] = y;
      tail++;
    }
    if (!wallDown[y][x] && !visited[y + 1][x]) {
      visited[y + 1][x] = true;
      queueX[tail] = x;
      queueY[tail] = y + 1;
      tail++;
    }
    if (!wallUp[y][x] && !visited[y - 1][x]) {
      visited[y - 1][x] = true;
      queueX[tail] = x;
      queueY[tail] = y - 1;
      tail++;
    }
  }

  return false;
}

void randomizeMazeStartAndGoal() {
  const int maxAttempts = 100;
  for (int attempt = 0; attempt < maxAttempts; attempt++) {
    int startX = random(MAZE_W);
    int startY = random(MAZE_H);
    int endX = random(MAZE_W);
    int endY = random(MAZE_H);

    if (startX == endX && startY == endY) {
      continue;
    }

    if (isMazePositionReachable(startX, startY, endX, endY)) {
      playerX = startX;
      playerY = startY;
      goalX = endX;
      goalY = endY;
      return;
    }
  }

  // Fallback to known-solvable corners if random attempts fail.
  playerX = 0;
  playerY = 0;
  goalX = MAZE_W - 1;
  goalY = MAZE_H - 1;
}

void setupFirstMaze() {
  clearWalls();
  buildOuterWalls();

  addVerticalWall(0, 1);
  addVerticalWall(0, 2);
  addVerticalWall(0, 3);
  addVerticalWall(1, 5);
  addVerticalWall(2, 0);
  addVerticalWall(2, 1);
  addVerticalWall(2, 2);
  addVerticalWall(2, 4);
  addVerticalWall(3, 3);
  addVerticalWall(3, 5);
  addVerticalWall(4, 4);

  addHorizontalWall(1, 0);
  addHorizontalWall(4, 0);
  addHorizontalWall(5, 0);
  addHorizontalWall(2, 1);
  addHorizontalWall(3, 1);
  addHorizontalWall(4, 1);
  addHorizontalWall(1, 2);
  addHorizontalWall(4, 2);
  addHorizontalWall(1, 3);
  addHorizontalWall(2, 3);
  addHorizontalWall(3, 3);
  addHorizontalWall(4, 3);
  addHorizontalWall(1, 4);
  addHorizontalWall(4, 4);
}

void drawScene() {
  // Clear all pixels to black
  for (int i = 0; i < 64; i++) {
    matrix.setPixelColor(i, COLOR_BLACK);
  }

  // Draw border walls (white)
  for (int x = 0; x < 8; x++) {
    matrix.setPixelColor(x, COLOR_WHITE);                    // Top border
    matrix.setPixelColor(7 * 8 + x, COLOR_WHITE);            // Bottom border
  }

  for (int y = 0; y < 8; y++) {
    matrix.setPixelColor(y * 8, COLOR_WHITE);                // Left border
    matrix.setPixelColor(y * 8 + 7, COLOR_WHITE);            // Right border
  }

  // Draw goal (player position + 1 for border offset)
  int gx = goalX + 1;
  int gy = goalY + 1;
  matrix.setPixelColor(gy * 8 + gx, mazeSolved ? COLOR_GREEN : COLOR_BLUE);

  // Draw player (red) - only if not solved
  if (!mazeSolved) {
    int px = playerX + 1;
    int py = playerY + 1;
    matrix.setPixelColor(py * 8 + px, COLOR_RED);
  }

  matrix.show();  // Update the display
}

void clearMazeDisplay() {
  // Clear all pixels to black
  for (int i = 0; i < 64; i++) {
    matrix.setPixelColor(i, COLOR_BLACK);
  }
  matrix.show();  // Update the display
}

void checkMazeSolved() {
  if (playerX == goalX && playerY == goalY) {
    mazeSolved = true;
    mazeSolvedTime = millis();  // Record when maze was solved
    drawScene();

  }
}

bool moveRight() {
  if (!wallRight[playerY][playerX]) {
    playerX++;
    return true;
  }
  // Hit a wall
  applyPenalty("MazeWallHit");
  return false;
}

bool moveLeft() {
  if (!wallLeft[playerY][playerX]) {
    playerX--;
    return true;
  }
  // Hit a wall
  applyPenalty("MazeWallHit");
  return false;
}

bool moveDown() {
  if (!wallDown[playerY][playerX]) {
    playerY++;
    return true;
  }
  // Hit a wall
  applyPenalty("MazeWallHit");
  return false;
}

bool moveUp() {
  if (!wallUp[playerY][playerX]) {
    playerY--;
    return true;
  }
  // Hit a wall
  applyPenalty("MazeWallHit");
  return false;
}

void setupMazeModule() {
  // Initialize NeoPixel strip (already done in main setup, but ensure it's ready)
  matrix.begin();
  matrix.show();  // Initialize all pixels to off
  
  mazeSolved = false;
  // Initialize with RAW encoder values (no division) - same as updateMazeModule
  mazeLastLeftPos = encLeft.read();
  mazeLastRightPos = encRight.read();
  setupFirstMaze();
  randomizeMazeStartAndGoal();
  drawScene();
}

void updateMazeModule() {
  if (mazeSolved) {
    // Clear the maze display after 1.5 second delay when the maze is first solved
    if (!mazeDisplayCleared) {
      unsigned long now = millis();
      if (now - mazeSolvedTime >= MAZE_CLEAR_DELAY) {
        clearMazeDisplay();
        mazeDisplayCleared = true;
      }
    }
    return;
  }

  // Lazy initialization: only setup maze after core is solved to save power
  if (!mazeSetupDone) {
    if (activeEncoderModule != MAZE_MODULE || (coreTriggered && !coreSolved)) {
      return;  // Wait until core event is complete before setting up maze
    }
    setupMazeModule();
    mazeSetupDone = true;
    return;  // Return after setup so we don't process moves on first frame
  }

  bool changed = false;
  unsigned long now = millis();

  // Work with RAW encoder values (no division) - same as FrequencyModule
  long currLeft = encLeft.read();
  long changeLeft = currLeft - mazeLastLeftPos;
  
  // Allow left encoder to move independently of right encoder
  if (abs(changeLeft) >= 4 && now - mazeLastLeftMoveTime > mazeMoveDelay) {
    if (changeLeft > 0) {
      changed = moveRight();
    } else {
      changed = moveLeft();
    }
    mazeLastLeftPos = currLeft;
    if (changed) {
      mazeLastLeftMoveTime = now;
    }
  }

  long currRight = encRight.read();
  long changeRight = currRight - mazeLastRightPos;
  
  // Allow right encoder to move independently of left encoder
  if (abs(changeRight) >= 4 && now - mazeLastRightMoveTime > mazeMoveDelay) {
    if (changeRight > 0) {
      changed = moveUp();
    } else {
      changed = moveDown();
    }
      
    mazeLastRightPos = currRight;
    if (changed) {
      mazeLastRightMoveTime = now;
    }
  }

  if (changed) {
    checkMazeSolved();
    drawScene();
  }
}

#endif
