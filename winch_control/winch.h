// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"

#include <functional>

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

  int left_position = 0, right_position = 0;
  bool enabled = true;
  // TODO: use this function!
  std::function<bool()> abort_func_ = nullptr;

  public:

  int LeftGoUp(uint32_t ms);
  int LeftGoDown(uint32_t ms);
  int RightGoUp(uint32_t ms);
  int RightGoDown(uint32_t ms);
  int GoLeft(uint32_t ms);
  int GoRight(uint32_t ms);

  int GetLeftPos() { return left_position; }
  int GetRightPos() { return right_position; }

  static bool IsRightSlideAtLimit();
  static bool IsLeftSlideAtLimit();
  static bool IsTopAtLimit();

  void Enable() { enabled = true; }
  void Disable() { enabled = false; }

  void SetAbortCheck(std::function<bool()> abort_func) {
    abort_func_ = abort_func;
  }

  WinchController();

  void ManualWinchControl(char side, char direction, uint32_t duration_ms);

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

