// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Testing plan
// Test GPIO connections
//   - all winch relays
//   - valves
// Test Scale
// Test Grainfather interface
// load values from spreadsheet
// Disconnect weights, run the trolley back and forth, hit limits
// start pump, measure weight while changing valve config
//
// Figure out way to get spreadsheet id to program
// global log - log of sessions
// Test going to limits with winch

#include "gpio.h"
#include "winch.h"



int TestWinchLimits() {
  // Raise right winch until top limit hits.  Perhaps should go to zero first...
  // Go Left to hit left slide limit
  WinchController wc;
  int expected, actual = wc.GetLeftPos();
  expected = actual;
  while (abs(expected-actual) < 10) {
    expected = actual - 300;
    wc.LeftGoUp(300);
  };
  return 0;
}



int TestLeftWinch() {
  // Can't test all the relays independantly, but here is the best effort:
  if (SetDirection(LEFT_WINCH_ENABLE, 1, 0)) return -1;
  if (SetDirection(RIGHT_WINCH_ENABLE, 1, 0)) return -1;
  if (SetDirection(LEFT_WINCH_DIRECTION, 1, 0)) return -1;
  if (SetDirection(RIGHT_WINCH_DIRECTION, 1, 0)) return -1;
  if (SetDirection(TOP_SWITCH, 0)) return -1;
  usleep(100000);
  WinchController wc;
  if(wc.LeftGoDown(900)) return -1;
  usleep(1000000);
  if(wc.LeftGoUp(900)) return -1;
  usleep(100000);
  return 0;
}

int TestRightWinch() {
  // Can't test all the relays independantly, but here is the best effort:
  if (SetDirection(LEFT_WINCH_ENABLE, 1, 0)) return -1;
  if (SetDirection(RIGHT_WINCH_ENABLE, 1, 0)) return -1;
  if (SetDirection(LEFT_WINCH_DIRECTION, 1, 0)) return -1;
  if (SetDirection(RIGHT_WINCH_DIRECTION, 1, 0)) return -1;
  if (SetDirection(TOP_SWITCH, 0)) return -1;
  usleep(100000);
  WinchController wc;
  if(wc.RightGoDown(600)) return -1;
  usleep(1000000);
  if(wc.RightGoUp(600)) return -1;
  usleep(100000);
  return 0;
}
//
//
//
int TestWinchRelays() {
  // Can't test all the relays independantly, but here is the best effort:
  if (SetDirection(LEFT_WINCH_ENABLE, 1, 0)) return -1;
  if (SetDirection(RIGHT_WINCH_ENABLE, 1, 0)) return -1;
  if (SetDirection(LEFT_WINCH_DIRECTION, 1, 0)) return -1;
  if (SetDirection(RIGHT_WINCH_DIRECTION, 1, 0)) return -1;
  if (SetDirection(TOP_SWITCH, 0)) return -1;

  // Should disable all the winches from going up
  if (SetOpenDrain(TOP_SWITCH, 1)) return -1;
  usleep(500000);

  // Enable up direction, so the movement will be disabled (b/c top switch)
  if (SetOutput(RIGHT_WINCH_DIRECTION, 1)) return -1;
  usleep(500000);

  if (SetOutput(RIGHT_WINCH_ENABLE, 1)) return -1;
  usleep(500000);
  if (SetOutput(RIGHT_WINCH_ENABLE, 0)) return -1;

  usleep(500000);
  if (SetOutput(RIGHT_WINCH_DIRECTION, 0)) return -1;

  if (SetOutput(LEFT_WINCH_DIRECTION, 1)) return -1;
  usleep(500000);
  // Leave left direction high (up) to disable motion (b/c top switch)

  if (SetOutput(LEFT_WINCH_ENABLE, 1)) return -1;
  usleep(500000);
  if (SetOutput(LEFT_WINCH_ENABLE, 0)) return -1;

  usleep(500000);
  if (SetOutput(LEFT_WINCH_DIRECTION, 0)) return -1;

  if (SetOpenDrain(TOP_SWITCH, 0)) return -1;
  usleep(500000);
  return 0;
}

int TestValveRelays() {
  if (SetDirection(CHILLER_PUMP, 1, 1)) return -1;
  if (SetDirection(VALVE_ENABLE, 1, 1)) return -1;
  if (SetDirection(CARBOY_VALVE, 1, 1)) return -1;
  if (SetDirection(CHILLER_VALVE, 1, 1)) return -1;
  if (SetDirection(KETTLE_VALVE, 1, 1)) return -1;

  usleep(100000);
  if (SetOutput(CHILLER_PUMP, 0)) return -1;
  usleep(500000);
  if (SetOutput(CHILLER_PUMP, 1)) return -1;
  usleep(100000);
  if (SetOutput(VALVE_ENABLE, 0)) return -1;
  usleep(500000);
  if (SetOutput(VALVE_ENABLE, 1)) return -1;
  usleep(100000);
  if (SetOutput(CARBOY_VALVE, 0)) return -1;
  usleep(500000);
  if (SetOutput(CARBOY_VALVE, 1)) return -1;
  usleep(100000);
  if (SetOutput(CHILLER_VALVE, 0)) return -1;
  usleep(500000);
  if (SetOutput(CHILLER_VALVE, 1)) return -1;
  usleep(100000);
  if (SetOutput(KETTLE_VALVE, 0)) return -1;
  usleep(500000);
  if (SetOutput(KETTLE_VALVE, 1)) return -1;
  usleep(100000);
  return 0;
}

int TestLimitSwitches() {
  if (SetDirection(RIGHT_SLIDE_SWITCH, 0)) return -1;
  if (SetDirection(LEFT_SLIDE_SWITCH, 0)) return -1;
  if (SetDirection(TOP_SWITCH, 0)) return -1;
  bool limits_closed[3] = {false, false, false};
  const char* names[3] = {"Right", "Left", "Top"};

  printf("Close each limit switch\n");

  while(!(limits_closed[0] && limits_closed[1] && limits_closed[2])) {
    printf("Waiting for limits:  ");
    for (int i = 0; i < 3; ++i) {
      if (!limits_closed[i]) {
        printf(" %s ", names[i]);
      }
    }
    printf("\n");
    if (WinchController::IsRightSlideAtLimit()) limits_closed[0] = true;
    if (WinchController::IsLeftSlideAtLimit()) limits_closed[1] = true;
    if (WinchController::IsTopAtLimit()) limits_closed[2] = true;
    usleep(1000);
  }
  return 0;
}

int main(int argc, char **argv) {

  if(TestWinchRelays()) {
    printf("TestWinchRelays failed!\n");
  }
  usleep(2000000);
  if(TestValveRelays()) {
    printf("TestValveRelays failed!\n");
  }

  TestLeftWinch();
  TestRightWinch();


  return 0;
}
