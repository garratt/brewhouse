// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"


namespace winch {

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

void ManualWinchControl(char side, char direction, uint32_t duration_ms = 200);

} // namespace winch
