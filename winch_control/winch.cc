// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winch.h"



// Right slide switch: no effect on winches
// Right top switch: Stops both winches from going up
// Left slide switch: stops left winch from going up
// Zero point: go left until LSS, then Right Up until RTS

class Winch {
  int enable_, dir_;
  const char *side_;
  public:
  Winch(const char* side) : side_(side) {
    if (strcmp(side,"left") == 0) {
      enable_ = LEFT_WINCH_ENABLE;
      dir_ = LEFT_WINCH_DIRECTION;
    } else {
      enable_ = RIGHT_WINCH_ENABLE;
      dir_ = RIGHT_WINCH_DIRECTION;
    }
  }

  // When destructing, shut off winch :)
  ~Winch() {
    if (SetOutput(enable_, 0)) {
      printf("Failed to disable %s winch!\n", side_);
    }
    if (SetOutput(dir_, 0)) {
      printf("Failed to set '0' direction for %s winch!\n", side_);
    }
  }

  int Enable(int direction) {
    if (direction == 0) {
      return 0;
    }
    // translate up: -1 -> 1,  down: 1 -> 0
    if(SetOutput(dir_, direction < 0 ? 1 : 0)) {
      printf("Failed to set %s direction\n", side_);
      return -1;
    }
    if(SetOutput(enable_, 1)) {
      printf("Failed to set %s enable\n", side_);
      return -1;
    }
    return 0;
  }
};

bool WinchController::IsRightSlideAtLimit() {
  int val = ReadInput(RIGHT_SLIDE_SWITCH);
  if (val < 0) return val;
  if (val == 0) return 1;
  return 0;
}

bool WinchController::IsLeftSlideAtLimit() {
  int val = ReadInput(LEFT_SLIDE_SWITCH);
  if (val < 0) return val;
  if (val == 0) return 1;
  return 0;
}

bool WinchController::IsTopAtLimit() {
  int val = ReadInput(TOP_SWITCH);
  if (val < 0) return val;
  if (val == 0) return 1;
  return 0;
}

// TODO: may need to add delay after I set the direction
int WinchController::RunWinches(uint32_t run_time, int left_dir, int right_dir) {
  Winch left("left"), right("right");
  // Set outputs.
  // If anything fails, the destructors will turn off the winch.
  if (left.Enable(left_dir) || right.Enable(right_dir)) {
    printf("Error: Failed to activate winches.\n");
    return -1;
  }
  // Wait until time expires or limits hit
  // Check upfront if we are hitting limits:
  int64_t start_time = GetTimeMsec();
  int64_t tnow = GetTimeMsec();
  // While loop checks all of our stopping conditions every ms:
  while ((tnow - start_time < run_time) &&
      // Left slide switch: stops left winch from going up
      !(left_dir == -1 && IsLeftSlideAtLimit()) &&
      // Right top switch: Stops both winches from going up
      !((left_dir == -1 || right_dir == -1) && IsTopAtLimit()) &&
      // Right slide switch: stops only if winches are moving right
      !((left_dir == 1 && right_dir == -1) && IsRightSlideAtLimit()) &&
      // If we lifted up the kettle, shut it down!
      !(abort_func_ && abort_func_())) {
    usleep(1000);
    tnow = GetTimeMsec();
  }
  // In case it gives us any better reaction time,
  // stop the winches ASAP! Don't worry about the result here...
  SetOutput(RIGHT_WINCH_ENABLE, 0);
  SetOutput(LEFT_WINCH_ENABLE, 0);
  // now, lets update the positions:
  tnow = GetTimeMsec();
  // multiply by direction to get how to modify position
  left_position += left_dir * (tnow - start_time);
  right_position += right_dir * (tnow - start_time);

  // Now just exit, Winch destructor will make sure everything is cleaned up.
  if (abort_func_) {
    return abort_func_() ? -1 : 0;
  }
  return 0;
}


WinchController::WinchController() {
  if (SetDirection(LEFT_WINCH_ENABLE, 1, 0))  {
    printf("WinchController: Failed to set GPIO direction\n");
  }
  if (SetDirection(RIGHT_WINCH_ENABLE, 1, 0))  {
    printf("WinchController: Failed to set GPIO direction\n");
  }
  if (SetDirection(LEFT_WINCH_DIRECTION, 1, 0))  {
    printf("WinchController: Failed to set GPIO direction\n");
  }
  if (SetDirection(RIGHT_WINCH_DIRECTION, 1, 0))  {
    printf("WinchController: Failed to set GPIO direction\n");
  }
  if (SetDirection(RIGHT_SLIDE_SWITCH, 0))  {
    printf("WinchController: Failed to set GPIO direction\n");
  }
  if (SetDirection(LEFT_SLIDE_SWITCH, 0))  {
    printf("WinchController: Failed to set GPIO direction\n");
  }
  if (SetDirection(TOP_SWITCH, 0))  {
    printf("WinchController: Failed to set GPIO direction\n");
  }
}


  int WinchController::LeftGoUp(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, -1, 0);
  }
  int WinchController::LeftGoDown(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, 1, 0);
  }
  int WinchController::RightGoUp(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, 0, -1);
  }
  int WinchController::RightGoDown(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, 0, 1);
  }
  int WinchController::GoLeft(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, -1, 1);
  }
  int WinchController::GoRight(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, 1, -1);
  }
void WinchController::ManualWinchControl(char side, char direction, uint32_t duration_ms) {
  if (side == 'l' && direction == 'u') RunWinches(duration_ms, -1,  0);
  if (side == 'l' && direction == 'd') RunWinches(duration_ms,  1,  0);
  if (side == 'r' && direction == 'u') RunWinches(duration_ms,  0, -1);
  if (side == 'r' && direction == 'd') RunWinches(duration_ms,  0,  1);
  if (side == 'b' && direction == 'l') RunWinches(duration_ms, -1,  1);
  if (side == 'b' && direction == 'r') RunWinches(duration_ms,  1, -1);
}

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
