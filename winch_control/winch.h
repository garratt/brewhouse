// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"



#define RIGHT 0
#define LEFT 1
// down is 0
// Reel the winch in, running for |ms| milliseconds.
// the serial number can be specified using serial.
int RunWinch(uint32_t ms, int side, int direction) {
   int enable = (side == RIGHT) ? RIGHT_WINCH_ENABLE : LEFT_WINCH_ENABLE;
   int dir = (side == RIGHT) ? RIGHT_WINCH_DIRECTION : LEFT_WINCH_DIRECTION;
   if(SetOutput(dir, direction)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(100000); // let the relay click around
   if (SetOutput(enable, 1)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(1000 * ms); // run for given amount of time
   if (SetOutput(enable, 0)) {
     printf("Failed to set output\n");
     return -1;
   }
   if (SetOutput(dir, 0)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(100000); // let the relay click around
   return 0;
}

// returns -1 if cannot communicate
// 0 if not stopped
// 1 if stopped (the limit switch is triggered)
int IsRightSlideStop() {
  // Active low (for now)
  int val = ReadInput(RIGHT_SLIDE_SWITCH);
  if (val < 0) return val;
  if (val == 0) return 1;
  return 0;
}


//Run both winches:
// Direction: Right: 0 Left: 1
int RunBothWinches(uint32_t ms, int direction) {
  //if direction == right, left down, right up
  int ldir = direction ? 1 : 0;
  int rdir = direction? 0 : 1;
   if(SetOutput(RIGHT_WINCH_DIRECTION, rdir)) {
     printf("Failed to set output\n");
     return -1;
   }
   if(SetOutput(LEFT_WINCH_DIRECTION, ldir)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(100000); // let the relay click around
   if (SetOutput(RIGHT_WINCH_ENABLE, 1) || SetOutput(LEFT_WINCH_ENABLE, 1)) {
     printf("Failed to set output\n");
     return -1;
   }
   // TODO: if moving right, stop at the right stop
   // Check every ms.
   uint32_t ms_counter = 0;
   do {
     usleep(1000);
     ms_counter++;
     if (direction == 0 && IsRightSlideStop()) {
       break;
     }
   } while (ms_counter <= ms);

   if (SetOutput(RIGHT_WINCH_ENABLE, 0) || SetOutput(LEFT_WINCH_ENABLE, 0)) {
     printf("Failed to set output\n");
     return -1;
   }
   if (SetOutput(LEFT_WINCH_DIRECTION, 0) || SetOutput(RIGHT_WINCH_DIRECTION, 0)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(100000); // let the relay click around
   return 0;
}


int LeftGoUp(uint32_t ms) {    return RunWinch(ms, 1, 1); }
int LeftGoDown(uint32_t ms) {  return RunWinch(ms, 1, 0); }
int RightGoUp(uint32_t ms) {   return RunWinch(ms, 0, 1); }
int RightGoDown(uint32_t ms) { return RunWinch(ms, 0, 0); }
int GoLeft(uint32_t ms) { return RunBothWinches(ms, 1); }
int GoRight(uint32_t ms) { return RunBothWinches(ms, 0); }

// --------------------------------------------------------------------
// These functions make assumptions about the distances
// the components need to travel, and the velocities the winches run at.
//  -------------------------------------------------------------------

int RaiseToDrain() {
  // Assumes we are in the mash state
  if (RightGoUp(2500)) { // go up until we hit the limit
    return -1;
  }
  if (RightGoDown(200)) {
    return -1;
  }
  return 0;
}

int MoveToSink() {
  // Raise to limit:
  RightGoUp(300);
  // Lower a little:
  RightGoDown(100);
  // Now scoot over using both winches:
  GoRight(3000); // This will quit early when it hits the limit switch

  // Now we are over the sink, lower away!
  return RightGoDown(3500);
}

int LowerHops() {
  return LeftGoDown(3000);
}

int RaiseHops() {
  return LeftGoUp(2500);  // go up a little less than we went down
}




void ManualWinchControl(char side, char direction, uint32_t duration_ms = 200) {
  if (side == 'l' && direction == 'u') LeftGoUp(duration_ms);
  if (side == 'l' && direction == 'd') LeftGoDown(duration_ms);
  if (side == 'r' && direction == 'u') RightGoUp(duration_ms);
  if (side == 'r' && direction == 'd') RightGoDown(duration_ms);
  if (side == 'b' && direction == 'l') RunBothWinches(duration_ms, 1);
  if (side == 'b' && direction == 'r') RunBothWinches(duration_ms, 0);
}
