// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"

#include <functional>

namespace raw_winch {

#define RIGHT 0
#define LEFT 1

// direction: down is 0, up is 1
// side: left is 1, right is 0
// Reel the winch in, running for |ms| milliseconds.
int RunWinch(uint32_t ms, int side, int direction);

// returns -1 if cannot communicate
// 0 if not stopped
// 1 if stopped (the limit switch is triggered)
int IsRightSlideStop();

//Run both winches:
// Direction: Right: 0 Left: 1
int RunBothWinches(uint32_t ms, int direction);

inline int LeftGoUp(uint32_t ms) {    return RunWinch(ms, 1, 1); }
inline int LeftGoDown(uint32_t ms) {  return RunWinch(ms, 1, 0); }
inline int RightGoUp(uint32_t ms) {   return RunWinch(ms, 0, 1); }
inline int RightGoDown(uint32_t ms) { return RunWinch(ms, 0, 0); }
inline int GoLeft(uint32_t ms) { return RunBothWinches(ms, 1); }
inline int GoRight(uint32_t ms) { return RunBothWinches(ms, 0); }

void ManualWinchControl(char side, char direction, uint32_t duration_ms = 200);

} // namespace raw winch

  //TODO: re-zero at known places
  //add check for reasonable ranges
  //add more limit switches
  //correct travel when a stop is reached
class WinchController {
  // left/right_dir: 
  //   -1 is reel in/go up
  //   0 is do nothing
  //   1 is spool out / go down
  // Invalid options: 0, 0 (returns with no error)
  //                  -1, -1 (returns with error)
  // Error is returned on communication error, or if the abort
  // function is called (which should indicate a pick-up event)
  // Limits are checked during this function, so the run time
  // may be shorter than the requested time.
  int RunWinches(uint32_t run_time, int left_dir, int right_dir);

  bool IsRightSlideAtLimit() {
    int val = ReadInput(RIGHT_SLIDE_SWITCH);
    if (val < 0) return val;
    if (val == 0) return 1;
    return 0;
  }

  bool IsLeftSlideAtLimit() {
    int val = ReadInput(LEFT_SLIDE_SWITCH);
    if (val < 0) return val;
    if (val == 0) return 1;
    return 0;
  }

  bool IsTopAtLimit() {
    int val = ReadInput(TOP_SWITCH);
    if (val < 0) return val;
    if (val == 0) return 1;
    return 0;
  }

  int left_position = 0, right_position = 0;
  bool enabled = true;
  // TODO: use this function!
  std::function<bool()> abort_func_ = nullptr;
  public:
  inline int LeftGoUp(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, -1, 0);
  }
  inline int LeftGoDown(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, 1, 0);
  }
  inline int RightGoUp(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, 0, -1);
  }
  inline int RightGoDown(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, 0, 1);
  }
  inline int GoLeft(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, -1, 1);
  }
  inline int GoRight(uint32_t ms) {
    if (!enabled) return 0;
    return RunWinches(ms, 1, -1);
  }

  void Enable() { enabled = true; }
  void Disable() { enabled = false; }

  void SetAbortCheck(std::function<bool()> abort_func) {
    abort_func_ = abort_func;
  }

  WinchController() {
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

  // --------------------------------------------------------------------
  // These functions make assumptions about the distances
  // the components need to travel, and the velocities the winches run at.
  //  -------------------------------------------------------------------

  // Raise a little bit to check that we are not caught
  int RaiseToDrain_1();
  // Raise the rest of the way
  int RaiseToDrain_2();
  int MoveToSink();
  int LowerHops();
  int RaiseHops();
  int GoToZero();

};

