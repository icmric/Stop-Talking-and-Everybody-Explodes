#ifndef MAZE_MODULE_H
#define MAZE_MODULE_H

/*
  MazeModule.h
  8×8 WS2812B LED matrix maze — navigate from a random start to a random goal.
  
  The maze is a 6×6 grid centred inside the 8×8 matrix (1-pixel border).
  Left encoder = move right/left. Right encoder = move up/down.
  Penalty: hitting a wall = -3s.

  Maze layout is selected from 4 options based on serial letter 2 (via getMazeIndex()):
    A–G → Maze 0,  H–N → Maze 1,  O–U → Maze 2,  V–Z → Maze 3

  Setup is deferred until both the Frequency module is done AND the Core
  event has been resolved, to avoid drawing excess current during the flash.

  Hardware:
    - WS2812B 8×8 matrix: data pin 22, brightness capped at 10%
    - Left encoder:  CLK→D2, DT→D3
    - Right encoder: CLK→D4, DT→D5
*/

#include <Encoder.h>
#include <Adafruit_NeoPixel.h>
#include "SerialNumber.h"

extern void applyPenalty(const char* reason);
extern bool coreSolved;
extern bool coreTriggered;
extern Encoder encLeft;
extern Encoder encRight;
extern Adafruit_NeoPixel matrix;

// ── Colour palette ─────────────────────────────────────────────────────────

const uint32_t COLOR_BLACK = 0x000000;
const uint32_t COLOR_RED   = 0xFF0000;
const uint32_t COLOR_GREEN = 0x00FF00;
const uint32_t COLOR_BLUE  = 0x0000FF;
const uint32_t COLOR_WHITE = 0xFFFFFF;

// ── Grid dimensions ────────────────────────────────────────────────────────

const int MAZE_W = 6;
const int MAZE_H = 6;

// ── State ──────────────────────────────────────────────────────────────────

int playerX = 0, playerY = 0;
int goalX   = 5, goalY   = 5;

bool mazeSolved         = false;
bool mazeSetupDone      = false;
bool mazeDisplayCleared = false;

unsigned long mazeSolvedTime = 0;
const unsigned long MAZE_CLEAR_DELAY = 1500;

long mazeLastLeftPos  = 0;
long mazeLastRightPos = 0;
unsigned long mazeLastLeftMoveTime  = 0;
unsigned long mazeLastRightMoveTime = 0;
const unsigned long mazeMoveDelay = 80;

// Wall arrays (true = wall exists on that side of this cell)
bool wallRight[MAZE_H][MAZE_W];
bool wallDown [MAZE_H][MAZE_W];
bool wallLeft [MAZE_H][MAZE_W];
bool wallUp   [MAZE_H][MAZE_W];

// ── Wall helpers ───────────────────────────────────────────────────────────

void clearWalls() {
  for (int y = 0; y < MAZE_H; y++)
    for (int x = 0; x < MAZE_W; x++)
      wallRight[y][x] = wallDown[y][x] = wallLeft[y][x] = wallUp[y][x] = false;
}

void buildOuterWalls() {
  for (int x = 0; x < MAZE_W; x++) {
    wallUp  [0]        [x] = true;
    wallDown[MAZE_H-1] [x] = true;
  }
  for (int y = 0; y < MAZE_H; y++) {
    wallLeft [y][0]        = true;
    wallRight[y][MAZE_W-1] = true;
  }
}

void addVerticalWall(int x, int y) {
  wallRight[y][x]   = true;
  wallLeft [y][x+1] = true;
}

void addHorizontalWall(int x, int y) {
  wallDown[y]  [x] = true;
  wallUp  [y+1][x] = true;
}

// ── Maze layouts ───────────────────────────────────────────────────────────

void setupMaze0() {
  clearWalls();
  buildOuterWalls();

  addVerticalWall(0,1); addVerticalWall(0,2); addVerticalWall(0,3);
  addVerticalWall(1,5); addVerticalWall(2,0); addVerticalWall(2,1);
  addVerticalWall(2,2); addVerticalWall(2,4); addVerticalWall(3,3);
  addVerticalWall(3,5); addVerticalWall(4,4);

  addHorizontalWall(1,0); addHorizontalWall(4,0); addHorizontalWall(5,0);
  addHorizontalWall(2,1); addHorizontalWall(3,1); addHorizontalWall(4,1);
  addHorizontalWall(1,2); addHorizontalWall(4,2); addHorizontalWall(1,3);
  addHorizontalWall(2,3); addHorizontalWall(3,3); addHorizontalWall(4,3);
  addHorizontalWall(1,4); addHorizontalWall(4,4);
}

void setupMaze1() {
  clearWalls();
  buildOuterWalls();

  addVerticalWall(0,0); addVerticalWall(4,0);
  addVerticalWall(0,1); addVerticalWall(1,1); addVerticalWall(4,1);
  addVerticalWall(0,2); addVerticalWall(1,2); addVerticalWall(3,2); addVerticalWall(4,2);
  addVerticalWall(0,3); addVerticalWall(4,3);
  addVerticalWall(0,4); addVerticalWall(2,4); addVerticalWall(4,4);
  addVerticalWall(2,5);

  addHorizontalWall(2,0); addHorizontalWall(3,0);
  addHorizontalWall(3,1); addHorizontalWall(4,1);
  addHorizontalWall(2,2); addHorizontalWall(3,2);
  addHorizontalWall(1,3); addHorizontalWall(3,3);
  addHorizontalWall(2,4); addHorizontalWall(4,4);
}

void setupMaze2() {
  clearWalls();
  buildOuterWalls();

  addVerticalWall(0,0); addVerticalWall(2,0); addVerticalWall(3,0);
  addVerticalWall(2,1);
  addVerticalWall(0,2); addVerticalWall(1,2); addVerticalWall(4,2);
  addVerticalWall(0,3); addVerticalWall(1,3); addVerticalWall(3,3);
  addVerticalWall(0,4); addVerticalWall(3,4);
  addVerticalWall(1,5); addVerticalWall(4,5);

  addHorizontalWall(1,0); addHorizontalWall(4,0);
  addHorizontalWall(3,1);
  addHorizontalWall(3,2); addHorizontalWall(4,2);
  addHorizontalWall(2,3); addHorizontalWall(4,3); addHorizontalWall(5,3);
  addHorizontalWall(0,4); addHorizontalWall(2,4);
}

void setupMaze3() {
  clearWalls();
  buildOuterWalls();

  addVerticalWall(0,1); addVerticalWall(1,1); addVerticalWall(4,1);
  addVerticalWall(0,2); addVerticalWall(4,2);
  addVerticalWall(1,3); addVerticalWall(3,3); addVerticalWall(4,3);
  addVerticalWall(0,4); addVerticalWall(4,4);
  addVerticalWall(1,5);

  addHorizontalWall(1,0); addHorizontalWall(3,0); addHorizontalWall(4,0);
  addHorizontalWall(0,1); addHorizontalWall(2,1); addHorizontalWall(3,1);
  addHorizontalWall(4,2);
  addHorizontalWall(1,3); addHorizontalWall(2,3); addHorizontalWall(5,3);
  addHorizontalWall(1,4); addHorizontalWall(3,4); addHorizontalWall(4,4);
}

void selectAndBuildMaze() {
  switch (getMazeIndex()) {
    case 0: setupMaze0(); break;
    case 1: setupMaze1(); break;
    case 2: setupMaze2(); break;
    case 3: setupMaze3(); break;
    default: setupMaze0(); break;
  }
}

// ── BFS reachability check ─────────────────────────────────────────────────

bool isMazePositionReachable(int sx, int sy, int tx, int ty) {
  bool visited[MAZE_H][MAZE_W] = {};
  int qx[MAZE_W * MAZE_H], qy[MAZE_W * MAZE_H];
  int head = 0, tail = 0;

  qx[tail] = sx; qy[tail] = sy; tail++;
  visited[sy][sx] = true;

  while (head < tail) {
    int x = qx[head], y = qy[head]; head++;
    if (x == tx && y == ty) return true;

    if (!wallRight[y][x] && !visited[y][x+1]) { visited[y][x+1]=true; qx[tail]=x+1; qy[tail]=y;   tail++; }
    if (!wallLeft [y][x] && !visited[y][x-1]) { visited[y][x-1]=true; qx[tail]=x-1; qy[tail]=y;   tail++; }
    if (!wallDown [y][x] && !visited[y+1][x]) { visited[y+1][x]=true; qx[tail]=x;   qy[tail]=y+1; tail++; }
    if (!wallUp   [y][x] && !visited[y-1][x]) { visited[y-1][x]=true; qx[tail]=x;   qy[tail]=y-1; tail++; }
  }
  return false;
}

void randomizeMazeStartAndGoal() {
  for (int attempt = 0; attempt < 100; attempt++) {
    int sx = random(MAZE_W), sy = random(MAZE_H);
    int ex = random(MAZE_W), ey = random(MAZE_H);
    if (sx == ex && sy == ey) continue;
    if (isMazePositionReachable(sx, sy, ex, ey)) {
      playerX = sx; playerY = sy;
      goalX   = ex; goalY   = ey;
      return;
    }
  }
  // Fallback: opposite corners
  playerX = 0; playerY = 0;
  goalX = MAZE_W-1; goalY = MAZE_H-1;
}

// ── Rendering ──────────────────────────────────────────────────────────────

void drawScene() {
  for (int i = 0; i < 64; i++) matrix.setPixelColor(i, COLOR_BLACK);

  for (int x = 0; x < 8; x++) {
    matrix.setPixelColor(x,      COLOR_WHITE);
    matrix.setPixelColor(56 + x, COLOR_WHITE);
  }
  for (int y = 0; y < 8; y++) {
    matrix.setPixelColor(y * 8,     COLOR_WHITE);
    matrix.setPixelColor(y * 8 + 7, COLOR_WHITE);
  }

  matrix.setPixelColor((goalY+1)*8 + (goalX+1), mazeSolved ? COLOR_GREEN : COLOR_BLUE);

  if (!mazeSolved)
    matrix.setPixelColor((playerY+1)*8 + (playerX+1), COLOR_RED);

  matrix.show();
}

void clearMazeDisplay() {
  for (int i = 0; i < 64; i++) matrix.setPixelColor(i, COLOR_BLACK);
  matrix.show();
}

// ── Movement ───────────────────────────────────────────────────────────────

bool moveRight() {
  if (!wallRight[playerY][playerX]) { playerX++; return true; }
  applyPenalty("MazeWallHit"); return false;
}
bool moveLeft() {
  if (!wallLeft[playerY][playerX])  { playerX--; return true; }
  applyPenalty("MazeWallHit"); return false;
}
bool moveDown() {
  if (!wallDown[playerY][playerX])  { playerY++; return true; }
  applyPenalty("MazeWallHit"); return false;
}
bool moveUp() {
  if (!wallUp[playerY][playerX])    { playerY--; return true; }
  applyPenalty("MazeWallHit"); return false;
}

// ── Setup / Update ─────────────────────────────────────────────────────────

void setupMazeModule() {
  matrix.begin();
  matrix.show();
  mazeSolved         = false;
  mazeDisplayCleared = false;
  mazeLastLeftPos    = encLeft.read();
  mazeLastRightPos   = encRight.read();
  selectAndBuildMaze();
  randomizeMazeStartAndGoal();
  drawScene();
}

void updateMazeModule() {
  if (mazeSolved) {
    if (!mazeDisplayCleared && millis() - mazeSolvedTime >= MAZE_CLEAR_DELAY) {
      clearMazeDisplay();
      mazeDisplayCleared = true;
    }
    return;
  }

  if (!mazeSetupDone) {
    if (activeEncoderModule != MAZE_MODULE || (coreTriggered && !coreSolved)) return;
    setupMazeModule();
    mazeSetupDone = true;
    return;
  }

  bool changed = false;
  unsigned long now = millis();

  long currLeft   = encLeft.read();
  long changeLeft = currLeft - mazeLastLeftPos;
  if (abs(changeLeft) >= 4 && now - mazeLastLeftMoveTime > mazeMoveDelay) {
    changed = (changeLeft > 0) ? moveRight() : moveLeft();
    mazeLastLeftPos = currLeft;
    if (changed) mazeLastLeftMoveTime = now;
  }

  long currRight   = encRight.read();
  long changeRight = currRight - mazeLastRightPos;
  if (abs(changeRight) >= 4 && now - mazeLastRightMoveTime > mazeMoveDelay) {
    changed = (changeRight > 0) ? moveUp() : moveDown();
    mazeLastRightPos = currRight;
    if (changed) mazeLastRightMoveTime = now;
  }

  if (changed) {
    if (playerX == goalX && playerY == goalY) {
      mazeSolved     = true;
      mazeSolvedTime = millis();
    }
    drawScene();
  }
}

#endif