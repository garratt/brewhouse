// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winch.h"

namespace raw_winch {


// Right slide switch: no effect on winches
// Right top switch: Stops both winches from going up
// Left slide switch: stops left winch from going up
// Zero point: go left until LSS, then Right Up until RTS

// int WaitForStops(uint32_t sleep_time, uint32_t *actual_sleep_time);


// direction: down is 0, up is 1
// side: left is 1, right is 0
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



void ManualWinchControl(char side, char direction, uint32_t duration_ms) {
  if (side == 'l' && direction == 'u') LeftGoUp(duration_ms);
  if (side == 'l' && direction == 'd') LeftGoDown(duration_ms);
  if (side == 'r' && direction == 'u') RightGoUp(duration_ms);
  if (side == 'r' && direction == 'd') RightGoDown(duration_ms);
  if (side == 'b' && direction == 'l') RunBothWinches(duration_ms, 1);
  if (side == 'b' && direction == 'r') RunBothWinches(duration_ms, 0);
}

} // namespace raw_winch

// --------------------------------------------------------------------
// These functions make assumptions about the distances
// the components need to travel, and the velocities the winches run at.
//  -------------------------------------------------------------------

int WinchController::RaiseToDrain_1() {
  // Assumes we are in the mash state
  if (RightGoUp(600)) { // Go up until the top has cleared.
                        // We stop here to se if we are caught.
    return -1;
  }
  return 0;
}

int WinchController::RaiseToDrain_2() {
  // Assumes we are in the mash state
  if (RightGoUp(1500)) { // go up until we are close to the top of the kettle
    return -1;
  }
  return 0;
}

int WinchController::MoveToSink() {
  // Raise to limit:
  if (RightGoUp(900) < 0) return -1;
  // Lower a little:
  if (RightGoDown(100) < 0) return -1;
  // Now scoot over using both winches:
  if (GoRight(3000) < 0) return -1; // This will quit early when it hits the limit switch

  // Now we are over the sink, lower away!
  return RightGoDown(3500);
}

int WinchController::LowerHops() {
  return LeftGoDown(3000);
}

int WinchController::RaiseHops() {
  return LeftGoUp(2500);  // go up a little less than we went down
}

int WinchController::GoToZero() {
  // Raise to limit:
  if (RightGoUp(4000) < 0) return -1;
  // Lower a little:
  if (RightGoDown(100) < 0) return -1;
  // Now scoot over using both winches, until we hit the left slide limit
  if (GoLeft(3500) < 0) return -1; // This will quit early when it hits the limit switch
  // Ideally, we would stop both winches when we hit the limit for this move...
  // Now go back up to the limit:
  if (RightGoUp(900) < 0) return -1;
  return 0;
}
