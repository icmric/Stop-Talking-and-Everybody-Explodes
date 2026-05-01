/*
  Matrix Maze Module
  
  Uses an 8x8 RGB LED matrix and two encoders for player movement.
  Navigate from top-left to bottom-right to solve the module.
*/

#ifndef MAZE_MODULE_H
#define MAZE_MODULE_H

#include <Encoder.h>
#include "grove_two_rgb_led_matrix.h"

// =====================================================
// MATRIX MAZE MODULE
// =====================================================

const int MAZE_W = 6;
const int MAZE_H = 6;

int playerX = 0;
int playerY = 0;
const int goalX = 5;
const int goalY = 5;

bool mazeSolved = false;
uint8_t frame[64];
long mazeLastLeftPos = 0;
long mazeLastRightPos = 0;
unsigned long mazeLastMoveTime = 0;
const unsigned long mazeMoveDelay = 120;

bool wallRight[MAZE_H][MAZE_W];
bool wallDown[MAZE_H][MAZE_W];
bool wallLeft[MAZE_H][MAZE_W];
bool wallUp[MAZE_H][MAZE_W];

// Forward declarations - these objects are declared in main file
extern Encoder encLeft;
extern Encoder encRight;
extern GroveTwoRGBLedMatrixClass matrix;

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
  for (int i = 0; i < 64; i++) {
    frame[i] = black;
  }

  for (int x = 0; x < 8; x++) {
    frame[x] = white;
    frame[7 * 8 + x] = white;
  }

  for (int y = 0; y < 8; y++) {
    frame[y * 8] = white;
    frame[y * 8 + 7] = white;
  }

  int px = playerX + 1;
  int py = playerY + 1;
  int gx = goalX + 1;
  int gy = goalY + 1;

  frame[gy * 8 + gx] = mazeSolved ? green : blue;

  if (!mazeSolved) {
    frame[py * 8 + px] = red;
  }

  matrix.displayFrames(frame, 0, true, 1);
}

void checkMazeSolved() {
  if (playerX == goalX && playerY == goalY) {
    mazeSolved = true;
    drawScene();
    Serial.println("Maze solved!");
  }
}

bool moveRight() {
  if (!wallRight[playerY][playerX]) {
    playerX++;
    return true;
  }
  return false;
}

bool moveLeft() {
  if (!wallLeft[playerY][playerX]) {
    playerX--;
    return true;
  }
  return false;
}

bool moveDown() {
  if (!wallDown[playerY][playerX]) {
    playerY++;
    return true;
  }
  return false;
}

bool moveUp() {
  if (!wallUp[playerY][playerX]) {
    playerY--;
    return true;
  }
  return false;
}

void setupMazeModule() {
  if (matrix.getDeviceVID() != 0x2886) {
    Serial.println("Matrix not detected");
    return;
  }

  playerX = 0;
  playerY = 0;
  mazeSolved = false;
  mazeLastLeftPos = encLeft.read() / 4;
  mazeLastRightPos = encRight.read() / 4;
  setupFirstMaze();
  drawScene();
}

void updateMazeModule() {
  if (mazeSolved) {
    return;
  }

  bool changed = false;
  unsigned long now = millis();

  long currLeft = encLeft.read() / 4;
  if (currLeft != mazeLastLeftPos && now - mazeLastMoveTime > mazeMoveDelay) {
    if (currLeft > mazeLastLeftPos) {
      changed = moveRight();
    } else {
      changed = moveLeft();
    }
    mazeLastLeftPos = currLeft;
    if (changed) {
      mazeLastMoveTime = now;
    }
  }

  long currRight = encRight.read() / 4;
  if (currRight != mazeLastRightPos && now - mazeLastMoveTime > mazeMoveDelay) {
    if (currRight > mazeLastRightPos) {
      changed = moveDown();
    } else {
      changed = moveUp();
    }
    mazeLastRightPos = currRight;
    if (changed) {
      mazeLastMoveTime = now;
    }
  }

  if (changed) {
    checkMazeSolved();
    drawScene();
  }
}

#endif
